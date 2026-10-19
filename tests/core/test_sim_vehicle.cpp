// SITL aracı ile GCS durum makineleri arasında, gerçek MAVLink baytları üzerinden
// (kodla → ayrıştır) soket kullanmadan uçtan uca senaryolar.

#include <gtest/gtest.h>

#include <cmath>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "gcs/command_tracker.hpp"
#include "gcs/geo.hpp"
#include "gcs/mavlink/codec.hpp"
#include "gcs/mission.hpp"
#include "sim_vehicle.hpp"

using namespace gcs;

namespace {

struct Harness {
    sitl::SimVehicle vehicle;
    mav::Parser toVehicleParser, toGcsParser;
    MissionUploader uploader;
    CommandTracker commands;
    std::uint8_t gcsSeq = 0;
    std::int64_t now = 0;
    std::vector<mav::Frame> telemetry;
    std::vector<CommandResult> results;
    std::vector<std::string> statusTexts;
    std::vector<int> reached;
    std::optional<bool> uploadOk;

    Harness() {
        uploader.sendCount = [this](const mav::MissionCount& m) { toVehicle(m); };
        uploader.sendItem = [this](const mav::MissionItemInt& m) { toVehicle(m); };
        uploader.sendAck = [this](const mav::MissionAck& m) { toVehicle(m); };
        uploader.onFinished = [this](bool ok, const std::string&) { uploadOk = ok; };
        commands.send = [this](const mav::CommandLong& c) { toVehicle(c); };
        commands.onResult = [this](const CommandResult& r) { results.push_back(r); };
    }

    template <class M>
    void toVehicle(const M& m) {
        auto bytes = mav::encode(m, mav::Header{gcsSeq++, 255, 190});
        sitl::Outbox out;
        toVehicleParser.feed(bytes.data(), bytes.size(),
                             [&](const mav::Frame& f) { vehicle.handle(f, now, out); });
        deliver(out);
    }

    // GCS tarafı geri çağrıları yeniden toVehicle() çağırabilir; aynı ayrıştırıcıya
    // iç içe feed() olmasın diye paketler kuyruğa alınıp tek döngüde teslim edilir.
    void deliver(const sitl::Outbox& out) {
        pending.insert(pending.end(), out.begin(), out.end());
        if (pumping) return;
        pumping = true;
        while (!pending.empty()) {
            const sitl::Packet pkt = pending.front();
            pending.pop_front();
            toGcsParser.feed(pkt.data(), pkt.size(), [&](const mav::Frame& f) { onGcsFrame(f); });
        }
        pumping = false;
    }
    std::deque<sitl::Packet> pending;
    bool pumping = false;

    void onGcsFrame(const mav::Frame& f) {
        telemetry.push_back(f);
        if (auto r = mav::decode<mav::MissionRequestInt>(f)) uploader.handleRequest(r->seq, now);
        if (auto a = mav::decode<mav::MissionAck>(f)) uploader.handleAck(*a, now);
        if (auto a = mav::decode<mav::CommandAck>(f)) commands.handleAck(*a, f.sysid, now);
        if (auto s = mav::decode<mav::StatusText>(f)) statusTexts.push_back(s->textString());
        if (auto r = mav::decode<mav::MissionItemReached>(f)) reached.push_back(r->seq);
    }

    void command(std::uint16_t id, float p1 = 0, float p2 = 0) {
        mav::CommandLong c;
        c.command = id;
        c.target_system = 1;
        c.target_component = 1;
        c.param1 = p1;
        c.param2 = p2;
        commands.submit(c, now);
    }

    void run(double seconds, double dt = 0.02) {
        const int steps = static_cast<int>(std::lround(seconds / dt));
        for (int i = 0; i < steps; ++i) {
            now += static_cast<std::int64_t>(std::lround(dt * 1000));
            sitl::Outbox out;
            vehicle.step(dt, now, out);
            deliver(out);
            uploader.tick(now);
            commands.tick(now);
        }
    }

    template <class M>
    int count() const {
        int n = 0;
        for (const auto& f : telemetry) n += f.msgid == M::ID ? 1 : 0;
        return n;
    }

    bool lastAccepted(std::uint16_t id) const {
        for (auto it = results.rbegin(); it != results.rend(); ++it) {
            if (it->command == id) return it->outcome == CommandOutcome::Accepted;
        }
        return false;
    }
};

std::vector<MissionItem> squareMission() {
    std::vector<MissionItem> m;
    MissionItem takeoff;
    takeoff.command = mav::cmd::NavTakeoff;
    takeoff.alt = 20.f;
    m.push_back(takeoff);
    const geo::LatLon home{39.8890, 32.7785};
    for (double brg : {0.0, 90.0, 180.0}) {
        const auto p = geo::destination(home, brg, 60.0);
        MissionItem wp;
        wp.lat = p.lat;
        wp.lon = p.lon;
        wp.alt = 25.f;
        m.push_back(wp);
    }
    MissionItem rtl;
    rtl.command = mav::cmd::NavReturnToLaunch;
    m.push_back(rtl);
    return m;
}

}  // namespace

TEST(SimVehicle, TelemetryRatesMatchSchedule) {
    Harness h;
    h.run(10.0);
    // 10 s'de: HEARTBEAT 1 Hz, ATTITUDE 20 Hz, GLOBAL_POSITION_INT 10 Hz
    EXPECT_NEAR(h.count<mav::Heartbeat>(), 10, 1);
    EXPECT_NEAR(h.count<mav::Attitude>(), 200, 2);
    EXPECT_NEAR(h.count<mav::GlobalPositionInt>(), 100, 2);
    EXPECT_EQ(h.toGcsParser.stats().crcErrors, 0u);
    // Otopilot ve gimbal bileşenleri ayrı dizi numarası kullanmalı; kayıpsız
    // kanalda alıcı hiç boşluk görmemeli.
    mav::SeqTracker seq;
    for (const auto& f : h.telemetry) seq.observe(f.sysid, f.compid, f.seq);
    EXPECT_EQ(seq.lost(), 0u);
    EXPECT_GT(h.count<mav::GimbalDeviceAttitudeStatus>(), 0);
}

TEST(SimVehicle, FullMissionFlightEndsWithLandingAtHome) {
    Harness h;
    ASSERT_TRUE(h.uploader.start(squareMission(), h.now));
    h.run(1.0);
    ASSERT_TRUE(h.uploadOk.has_value());
    ASSERT_TRUE(*h.uploadOk);
    ASSERT_EQ(h.vehicle.mission().size(), 5u);

    h.command(mav::cmd::MissionStart);
    h.run(0.5);
    EXPECT_FALSE(h.lastAccepted(mav::cmd::MissionStart));  // arm edilmeden reddedilmeli

    h.command(mav::cmd::ComponentArmDisarm, 1);
    h.run(0.5);
    ASSERT_TRUE(h.vehicle.armed());
    h.command(mav::cmd::MissionStart);
    h.run(0.5);
    ASSERT_TRUE(h.lastAccepted(mav::cmd::MissionStart));
    EXPECT_EQ(h.vehicle.mode(), sitl::Mode::Auto);

    h.run(150.0);
    EXPECT_EQ(h.reached, (std::vector<int>{0, 1, 2, 3, 4}));
    EXPECT_FALSE(h.vehicle.armed());  // RTL → iniş → otomatik disarm
    EXPECT_LT(geo::distanceM(h.vehicle.position(), {39.8890, 32.7785}), 3.5);
    EXPECT_DOUBLE_EQ(h.vehicle.relAlt(), 0.0);
}

TEST(SimVehicle, DisarmInFlightIsDeniedUnlessForced) {
    Harness h;
    h.uploader.start(squareMission(), h.now);
    h.run(1.0);
    h.command(mav::cmd::ComponentArmDisarm, 1);
    h.command(mav::cmd::MissionStart);
    h.run(5.0);
    ASSERT_GT(h.vehicle.relAlt(), 5.0);
    h.command(mav::cmd::ComponentArmDisarm, 0);
    h.run(0.2);
    EXPECT_TRUE(h.vehicle.armed());
    EXPECT_FALSE(h.lastAccepted(mav::cmd::ComponentArmDisarm));
    h.command(mav::cmd::ComponentArmDisarm, 0, 21196.f);
    h.run(0.2);
    EXPECT_FALSE(h.vehicle.armed());
}

TEST(SimVehicle, GimbalSlewsAndReportsQuaternion) {
    Harness h;
    h.command(mav::cmd::DoGimbalManagerPitchYaw, -45.f, 30.f);
    h.run(0.4);
    EXPECT_TRUE(h.lastAccepted(mav::cmd::DoGimbalManagerPitchYaw));
    EXPECT_GT(h.vehicle.gimbalPitch(), -45.0);  // 60°/s ile henüz hedefte değil
    h.run(1.0);
    EXPECT_NEAR(h.vehicle.gimbalPitch(), -45.0, 1e-9);
    EXPECT_NEAR(h.vehicle.gimbalYaw(), 30.0, 1e-9);

    std::optional<mav::GimbalDeviceAttitudeStatus> last;
    for (const auto& f : h.telemetry) {
        if (auto g = mav::decode<mav::GimbalDeviceAttitudeStatus>(f)) {
            EXPECT_EQ(f.compid, mav::CompIdGimbal);
            last = g;
        }
    }
    ASSERT_TRUE(last);
    const auto e = geo::eulerFromQuat({last->q[0], last->q[1], last->q[2], last->q[3]});
    EXPECT_NEAR(e.pitch, -45.0, 1e-3);
    EXPECT_NEAR(e.yaw, 30.0, 1e-3);
}

TEST(SimVehicle, GimbalNanParameterLeavesAxisUnchanged) {
    Harness h;
    h.command(mav::cmd::DoGimbalManagerPitchYaw, -30.f, 20.f);
    h.run(1.0);
    h.command(mav::cmd::DoGimbalManagerPitchYaw, std::nanf(""), -10.f);
    h.run(1.0);
    EXPECT_NEAR(h.vehicle.gimbalPitch(), -30.0, 1e-9);
    EXPECT_NEAR(h.vehicle.gimbalYaw(), -10.0, 1e-9);
}

TEST(SimVehicle, CameraCommands) {
    Harness h;
    h.command(mav::cmd::ImageStartCapture, 0, 0);
    h.run(0.1);
    h.command(mav::cmd::ImageStartCapture, 0, 0);
    h.run(0.1);
    EXPECT_EQ(h.vehicle.photoCount(), 2);
    EXPECT_EQ(h.statusTexts.back(), "Fotograf #2 kaydedildi");

    h.command(mav::cmd::VideoStopCapture);
    h.run(0.1);
    EXPECT_FALSE(h.lastAccepted(mav::cmd::VideoStopCapture));  // kayıt yokken durdurma reddi
    h.command(mav::cmd::VideoStartCapture);
    h.run(0.1);
    EXPECT_TRUE(h.vehicle.recording());

    h.command(mav::cmd::SetCameraZoom, mav::ZoomTypeRange, 140.f);
    h.run(0.1);
    EXPECT_FLOAT_EQ(h.vehicle.zoom(), 100.f);  // aralığa kırpılır
    h.command(mav::cmd::SetCameraZoom, 1 /* ZOOM_TYPE_CONTINUOUS */, 1.f);
    h.run(0.1);
    EXPECT_FALSE(h.lastAccepted(mav::cmd::SetCameraZoom));
}

TEST(SimVehicle, UnknownCommandIsUnsupported) {
    Harness h;
    h.command(4242);
    h.run(0.1);
    ASSERT_FALSE(h.results.empty());
    EXPECT_EQ(h.results.back().outcome, CommandOutcome::Rejected);
    EXPECT_EQ(h.results.back().mavResult, mav::result::Unsupported);
}
