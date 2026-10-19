#include <gtest/gtest.h>

#include <deque>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "gcs/geo.hpp"
#include "gcs/mission.hpp"

using namespace gcs;

namespace {

std::vector<MissionItem> sampleMission(int n) {
    std::vector<MissionItem> items;
    for (int i = 0; i < n; ++i) {
        MissionItem m;
        m.lat = 39.8890 + 0.001 * i;
        m.lon = 32.7785 + 0.0005 * i;
        m.alt = 40.f + static_cast<float>(i);
        items.push_back(m);
    }
    return items;
}

// GCS (uploader) ile araç (receiver) arasında, istenen mesajları düşürebilen
// bellek içi kanal. Zaman elle ilerletilir.
struct Channel {
    using ToVehicle = std::variant<mav::MissionCount, mav::MissionItemInt, mav::MissionAck>;
    using ToGcs = std::variant<mav::MissionRequestInt, mav::MissionAck>;

    MissionUploader up;
    MissionReceiver rx;
    std::deque<ToVehicle> toVehicle;
    std::deque<ToGcs> toGcs;
    std::function<bool(int)> dropToVehicle = [](int) { return false; };
    std::function<bool(int)> dropToGcs = [](int) { return false; };
    int sentToVehicle = 0, sentToGcs = 0;
    std::int64_t now = 0;
    bool finished = false, ok = false;
    std::string message;
    std::vector<MissionItem> received;
    bool vehicleComplete = false;

    Channel() {
        up.sendCount = [this](const mav::MissionCount& c) { push(c); };
        up.sendItem = [this](const mav::MissionItemInt& i) { push(i); };
        up.sendAck = [this](const mav::MissionAck& a) { push(a); };
        up.onFinished = [this](bool success, const std::string& msg) {
            finished = true;
            ok = success;
            message = msg;
        };
        rx.sendRequest = [this](const mav::MissionRequestInt& r) { pushGcs(r); };
        rx.sendAck = [this](const mav::MissionAck& a) { pushGcs(a); };
        rx.onComplete = [this](std::vector<MissionItem> items) {
            received = std::move(items);
            vehicleComplete = true;
        };
    }
    void push(ToVehicle m) {
        if (!dropToVehicle(sentToVehicle++)) toVehicle.push_back(m);
    }
    void pushGcs(ToGcs m) {
        if (!dropToGcs(sentToGcs++)) toGcs.push_back(m);
    }

    // Kuyruklar boşalana kadar teslim et, sonra zamanı ilerlet.
    void run(std::int64_t untilMs, std::int64_t stepMs = 50) {
        while (now <= untilMs && !finished) {
            while (!toVehicle.empty() || !toGcs.empty()) {
                if (!toVehicle.empty()) {
                    auto m = toVehicle.front();
                    toVehicle.pop_front();
                    if (auto* c = std::get_if<mav::MissionCount>(&m)) rx.handleCount(*c, 255, 190, now);
                    if (auto* i = std::get_if<mav::MissionItemInt>(&m)) rx.handleItem(*i, now);
                }
                if (!toGcs.empty()) {
                    auto m = toGcs.front();
                    toGcs.pop_front();
                    if (auto* r = std::get_if<mav::MissionRequestInt>(&m)) up.handleRequest(r->seq, now);
                    if (auto* a = std::get_if<mav::MissionAck>(&m)) up.handleAck(*a, now);
                }
            }
            now += stepMs;
            up.tick(now);
            rx.tick(now);
        }
    }
};

}  // namespace

TEST(MissionItem, WireRoundTripKeepsDegE7Precision) {
    MissionItem m;
    m.lat = 39.88912346;
    m.lon = -32.12345678;
    m.alt = 75.5f;
    m.param1 = 3.f;
    auto w = toWire(m, 4, 1, 1);
    EXPECT_EQ(w.x, 398891235);  // 1e-7 dereceye yuvarlanır (~1.1 cm)
    EXPECT_EQ(w.y, -321234568);
    EXPECT_EQ(w.frame, mav::FrameGlobalRelativeAlt);
    EXPECT_EQ(w.current, 0);
    auto back = fromWire(w);
    EXPECT_NEAR(back.lat, m.lat, 1e-7);
    EXPECT_NEAR(back.lon, m.lon, 1e-7);
    EXPECT_FLOAT_EQ(back.alt, 75.5f);
    EXPECT_EQ(toWire(m, 0, 1, 1).current, 1);
}

TEST(MissionItem, MoveItem) {
    auto items = sampleMission(4);
    const double lat0 = items[0].lat, lat3 = items[3].lat;
    ASSERT_TRUE(moveItem(items, 0, 3));
    EXPECT_DOUBLE_EQ(items[3].lat, lat0);
    EXPECT_DOUBLE_EQ(items[2].lat, lat3);
    EXPECT_FALSE(moveItem(items, -1, 2));
    EXPECT_FALSE(moveItem(items, 1, 4));
}

TEST(MissionItem, PathLengthSkipsNonPositionalItems) {
    auto items = sampleMission(3);
    const double expected = geo::distanceM({items[0].lat, items[0].lon}, {items[1].lat, items[1].lon}) +
                            geo::distanceM({items[1].lat, items[1].lon}, {items[2].lat, items[2].lon});
    MissionItem rtl;
    rtl.command = mav::cmd::NavReturnToLaunch;
    items.insert(items.begin() + 1, rtl);  // konumsuz öğe (0,0) hesaba katılmamalı
    EXPECT_NEAR(pathLengthM(items), expected, 1e-6);
}

TEST(MissionProtocol, LosslessUpload) {
    Channel ch;
    ASSERT_TRUE(ch.up.start(sampleMission(8), 0));
    ch.run(10000);
    EXPECT_TRUE(ch.ok) << ch.message;
    EXPECT_TRUE(ch.vehicleComplete);
    ASSERT_EQ(ch.received.size(), 8u);
    EXPECT_NEAR(ch.received[7].lat, 39.8890 + 0.007, 1e-7);
    EXPECT_EQ(ch.up.retransmissions(), 0);
    EXPECT_DOUBLE_EQ(ch.up.progress(), 1.0);
}

TEST(MissionProtocol, RecoversFromPacketLossInBothDirections) {
    Channel ch;
    // Belirlenimci kayıp: her yönde kabaca her 3. paket düşer.
    ch.dropToVehicle = [](int i) { return i % 3 == 1; };
    ch.dropToGcs = [](int i) { return i % 4 == 2; };
    ch.up.setConfig({300, 6, 1, 1});
    ch.rx.timeoutMs = 400;
    ASSERT_TRUE(ch.up.start(sampleMission(12), 0));
    ch.run(60000);
    EXPECT_TRUE(ch.ok) << ch.message;
    ASSERT_EQ(ch.received.size(), 12u);
    for (int i = 0; i < 12; ++i) EXPECT_FLOAT_EQ(ch.received[i].alt, 40.f + i);
    EXPECT_GT(ch.up.retransmissions(), 0);
}

TEST(MissionProtocol, LostFinalAckIsRecoveredByResendingLastItem) {
    Channel ch;
    // Aracın göndereceği 4. mesaj (3 istekten sonra ACK) düşer.
    ch.dropToGcs = [](int i) { return i == 3; };
    ch.up.setConfig({200, 4, 1, 1});
    ASSERT_TRUE(ch.up.start(sampleMission(3), 0));
    ch.run(10000);
    EXPECT_TRUE(ch.ok) << ch.message;
}

TEST(MissionProtocol, TimesOutWhenVehicleSilent) {
    Channel ch;
    ch.dropToGcs = [](int) { return true; };
    ch.up.setConfig({100, 3, 1, 1});
    ASSERT_TRUE(ch.up.start(sampleMission(2), 0));
    ch.run(5000);
    EXPECT_TRUE(ch.finished);
    EXPECT_FALSE(ch.ok);
    EXPECT_EQ(ch.up.state(), MissionUploader::State::Failed);
    EXPECT_NE(ch.message.find("zaman aşımı"), std::string::npos);
}

TEST(MissionProtocol, VehicleRejectsTooManyItems) {
    Channel ch;
    ch.rx.maxItems = 5;
    ASSERT_TRUE(ch.up.start(sampleMission(6), 0));
    ch.run(5000);
    EXPECT_FALSE(ch.ok);
    EXPECT_NE(ch.message.find("reddetti"), std::string::npos);
}

TEST(MissionProtocol, InvalidSequenceRequestFails) {
    MissionUploader up;
    std::vector<mav::MissionAck> acks;
    up.sendAck = [&](const mav::MissionAck& a) { acks.push_back(a); };
    bool ok = true;
    up.onFinished = [&](bool success, const std::string&) { ok = success; };
    up.start(sampleMission(2), 0);
    up.handleRequest(5, 10);
    EXPECT_FALSE(ok);
    ASSERT_EQ(acks.size(), 1u);
    EXPECT_EQ(acks[0].type, mav::mission_result::InvalidSequence);
}

TEST(MissionProtocol, PrematureAcceptIsTreatedAsFailure) {
    MissionUploader up;
    bool ok = true;
    up.onFinished = [&](bool success, const std::string&) { ok = success; };
    up.start(sampleMission(3), 0);
    up.handleRequest(0, 1);
    mav::MissionAck a;
    a.type = mav::mission_result::Accepted;
    up.handleAck(a, 2);
    EXPECT_FALSE(ok);
}

TEST(MissionProtocol, EmptyMissionClearsVehicle) {
    Channel ch;
    ASSERT_TRUE(ch.up.start({}, 0));
    ch.run(1000);
    EXPECT_TRUE(ch.ok);
    EXPECT_TRUE(ch.vehicleComplete);
    EXPECT_TRUE(ch.received.empty());
}

TEST(MissionProtocol, CancelSendsAckAndRejectsLateMessages) {
    MissionUploader up;
    std::vector<mav::MissionAck> acks;
    up.sendAck = [&](const mav::MissionAck& a) { acks.push_back(a); };
    up.start(sampleMission(2), 0);
    EXPECT_FALSE(up.start(sampleMission(1), 0));  // devam eden yükleme varken ikinci başlatma yok
    up.cancel();
    ASSERT_EQ(acks.size(), 1u);
    EXPECT_EQ(acks[0].type, mav::mission_result::OperationCancelled);
    EXPECT_EQ(up.state(), MissionUploader::State::Failed);
    up.handleRequest(0, 5);  // iptal sonrası gelen istek yok sayılır
    EXPECT_EQ(up.highestRequested(), -1);
}

TEST(MissionProtocol, ReceiverRestartsOnRepeatedCount) {
    Channel ch;
    // İlk MISSION_REQUEST(0) düşer; GCS MISSION_COUNT'u yeniden yollar, alıcı baştan başlar.
    ch.dropToGcs = [](int i) { return i == 0; };
    ch.up.setConfig({200, 4, 1, 1});
    ch.rx.timeoutMs = 10000;  // alıcı kendi zaman aşımıyla toparlamasın
    ASSERT_TRUE(ch.up.start(sampleMission(2), 0));
    ch.run(5000);
    EXPECT_TRUE(ch.ok) << ch.message;
    EXPECT_EQ(ch.received.size(), 2u);
}
