// pymavlink'in ürettiği baytlarla karşılaştırma (tools/gen_golden.py).
// Hem kodlayıcının bayt düzeyinde aynı çıktıyı verdiğini hem de ayrıştırıcının
// bağımsız bir gerçeklemenin çerçevelerini doğru okuduğunu gösterir.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "gcs/mavlink/codec.hpp"
#include "gcs/mavlink/messages.hpp"
#include "golden_vectors.inc"

using namespace gcs::mav;

namespace {
Frame parseOne(const std::vector<std::uint8_t>& bytes) {
    Parser p;
    std::vector<Frame> frames;
    p.feed(bytes.data(), bytes.size(), [&](const Frame& f) { frames.push_back(f); });
    EXPECT_EQ(frames.size(), 1u);
    return frames.empty() ? Frame{} : frames.front();
}

Heartbeat goldenHeartbeat() {
    Heartbeat hb;
    hb.type = 2;
    hb.autopilot = 0;
    hb.base_mode = 129;
    hb.custom_mode = 1;
    hb.system_status = 4;
    hb.mavlink_version = 3;
    return hb;
}
}  // namespace

TEST(Golden, HeartbeatV2) {
    EXPECT_EQ(encode(goldenHeartbeat(), Header{7, 1, 1}), kHeartbeatV2);
    auto hb = decode<Heartbeat>(parseOne(kHeartbeatV2));
    ASSERT_TRUE(hb);
    EXPECT_EQ(hb->base_mode, 129);
    EXPECT_EQ(hb->system_status, 4);
}

TEST(Golden, HeartbeatV1) {
    EXPECT_EQ(encode(goldenHeartbeat(), Header{7, 1, 1}, 1), kHeartbeatV1);
    EXPECT_EQ(parseOne(kHeartbeatV1).version, 1);
}

TEST(Golden, GlobalPositionInt) {
    GlobalPositionInt m;
    m.time_boot_ms = 123456;
    m.lat = 398890000;
    m.lon = 327785000;
    m.alt = 1000500;
    m.relative_alt = 50500;
    m.vx = -120;
    m.vy = 340;
    m.vz = -25;
    m.hdg = 27050;
    EXPECT_EQ(encode(m, Header{200, 1, 1}), kGlobalPositionIntV2);
    auto d = decode<GlobalPositionInt>(parseOne(kGlobalPositionIntV2));
    ASSERT_TRUE(d);
    EXPECT_EQ(d->lat, 398890000);
    EXPECT_EQ(d->vx, -120);
    EXPECT_EQ(d->hdg, 27050);
}

TEST(Golden, AttitudeTruncated) {
    Attitude m;
    m.time_boot_ms = 1000;
    m.roll = 0.5f;
    m.pitch = -0.25f;
    m.yaw = 1.5f;
    const auto bytes = encode(m, Header{1, 1, 1});
    EXPECT_EQ(bytes, kAttitudeV2);
    EXPECT_EQ(bytes[1], 16);  // sondaki sıfır hızlar kırpıldı
    auto d = decode<Attitude>(parseOne(kAttitudeV2));
    ASSERT_TRUE(d);
    EXPECT_FLOAT_EQ(d->pitch, -0.25f);
    EXPECT_FLOAT_EQ(d->yawspeed, 0.f);
}

TEST(Golden, CommandLongGimbalPitchYaw) {
    CommandLong c;
    c.target_system = 1;
    c.target_component = 154;
    c.command = cmd::DoGimbalManagerPitchYaw;
    c.param1 = -45.f;
    c.param2 = 30.f;
    EXPECT_EQ(encode(c, Header{3, 255, 190}), kCommandLongV2);
}

TEST(Golden, MissionItemInt) {
    MissionItemInt m;
    m.target_system = 1;
    m.target_component = 1;
    m.seq = 2;
    m.frame = 3;
    m.command = 16;
    m.current = 0;
    m.autocontinue = 1;
    m.param1 = 5.f;
    m.param2 = 2.f;
    m.x = 398895000;
    m.y = 327790000;
    m.z = 60.f;
    EXPECT_EQ(encode(m, Header{9, 255, 190}), kMissionItemIntV2);
    auto d = decode<MissionItemInt>(parseOne(kMissionItemIntV2));
    ASSERT_TRUE(d);
    EXPECT_EQ(d->x, 398895000);
    EXPECT_FLOAT_EQ(d->z, 60.f);
    EXPECT_EQ(d->mission_type, 0);
}

TEST(Golden, CommandAckWithExtensions) {
    CommandAck a;
    a.command = 400;
    a.result = 0;
    a.target_system = 255;
    a.target_component = 190;
    EXPECT_EQ(encode(a, Header{11, 1, 1}), kCommandAckV2);
}

TEST(Golden, StatusText) {
    StatusText s;
    s.severity = 6;
    s.setText("Gorev basladi");
    EXPECT_EQ(encode(s, Header{12, 1, 1}), kStatusTextV2);
    auto d = decode<StatusText>(parseOne(kStatusTextV2));
    ASSERT_TRUE(d);
    EXPECT_EQ(d->textString(), "Gorev basladi");
}

TEST(Golden, SysStatus) {
    SysStatus s;
    s.load = 310;
    s.voltage_battery = 16520;
    s.current_battery = 1450;
    s.battery_remaining = 82;
    EXPECT_EQ(encode(s, Header{13, 1, 1}), kSysStatusV2);
}
