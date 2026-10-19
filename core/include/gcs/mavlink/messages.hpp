#pragma once
// MAVLink "common" diyalektinden yer istasyonunun kullandığı mesaj alt kümesi.
// Alan sırası kablo sırasıdır (tip boyutuna göre sıralı, uzantı alanları sonda).
// Kimlikler, CRC_EXTRA ve uzunluklar pymavlink 2.4.49 common.xml'den alındı;
// tests/core/test_golden.cpp aynı değerleri pymavlink çıktısıyla karşılaştırır.

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "gcs/mavlink/wire.hpp"

namespace gcs::mav {

// --- Sabitler (common.xml) ---------------------------------------------------
namespace cmd {
constexpr std::uint16_t NavWaypoint = 16;
constexpr std::uint16_t NavReturnToLaunch = 20;
constexpr std::uint16_t NavLand = 21;
constexpr std::uint16_t NavTakeoff = 22;
constexpr std::uint16_t DoPauseContinue = 193;
constexpr std::uint16_t MissionStart = 300;
constexpr std::uint16_t ComponentArmDisarm = 400;
constexpr std::uint16_t SetCameraZoom = 531;
constexpr std::uint16_t DoGimbalManagerPitchYaw = 1000;
constexpr std::uint16_t ImageStartCapture = 2000;
constexpr std::uint16_t VideoStartCapture = 2500;
constexpr std::uint16_t VideoStopCapture = 2501;
}  // namespace cmd

namespace result {
constexpr std::uint8_t Accepted = 0;
constexpr std::uint8_t TemporarilyRejected = 1;
constexpr std::uint8_t Denied = 2;
constexpr std::uint8_t Unsupported = 3;
constexpr std::uint8_t Failed = 4;
constexpr std::uint8_t InProgress = 5;
}  // namespace result

namespace mission_result {
constexpr std::uint8_t Accepted = 0;
constexpr std::uint8_t Error = 1;
constexpr std::uint8_t NoSpace = 4;
constexpr std::uint8_t InvalidSequence = 13;
constexpr std::uint8_t Denied = 14;
constexpr std::uint8_t OperationCancelled = 15;
}  // namespace mission_result

constexpr std::uint8_t FrameGlobalRelativeAlt = 3;
constexpr std::uint8_t MissionTypeMission = 0;
constexpr std::uint8_t TypeQuadrotor = 2;
constexpr std::uint8_t TypeGcs = 6;
constexpr std::uint8_t AutopilotGeneric = 0;
constexpr std::uint8_t AutopilotInvalid = 8;
constexpr std::uint8_t ModeFlagCustomModeEnabled = 1;
constexpr std::uint8_t ModeFlagAutoEnabled = 4;
constexpr std::uint8_t ModeFlagSafetyArmed = 128;
constexpr std::uint8_t StateStandby = 3;
constexpr std::uint8_t StateActive = 4;
constexpr std::uint8_t SeverityWarning = 4;
constexpr std::uint8_t SeverityInfo = 6;
constexpr std::uint8_t ZoomTypeRange = 2;
constexpr std::uint8_t CompIdAutopilot = 1;
constexpr std::uint8_t CompIdCamera = 100;
constexpr std::uint8_t CompIdGimbal = 154;
constexpr std::uint8_t CompIdMissionPlanner = 190;

// --- Mesajlar ----------------------------------------------------------------

struct Heartbeat {
    static constexpr std::uint32_t ID = 0;
    static constexpr std::uint8_t CRC_EXTRA = 50;
    static constexpr std::size_t LEN = 9;

    std::uint32_t custom_mode = 0;
    std::uint8_t type = 0;
    std::uint8_t autopilot = 0;
    std::uint8_t base_mode = 0;
    std::uint8_t system_status = 0;
    std::uint8_t mavlink_version = 3;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(custom_mode); w.put(type); w.put(autopilot); w.put(base_mode);
        w.put(system_status); w.put(mavlink_version);
    }
    static Heartbeat unpack(const std::uint8_t* in) {
        Reader r(in);
        Heartbeat m;
        m.custom_mode = r.get<std::uint32_t>(); m.type = r.get<std::uint8_t>();
        m.autopilot = r.get<std::uint8_t>(); m.base_mode = r.get<std::uint8_t>();
        m.system_status = r.get<std::uint8_t>(); m.mavlink_version = r.get<std::uint8_t>();
        return m;
    }
};

struct SysStatus {
    static constexpr std::uint32_t ID = 1;
    static constexpr std::uint8_t CRC_EXTRA = 124;
    static constexpr std::size_t LEN = 31;

    std::uint32_t sensors_present = 0;
    std::uint32_t sensors_enabled = 0;
    std::uint32_t sensors_health = 0;
    std::uint16_t load = 0;             // %0.1
    std::uint16_t voltage_battery = 0;  // mV
    std::int16_t current_battery = -1;  // cA, -1 = bilinmiyor
    std::uint16_t drop_rate_comm = 0;
    std::uint16_t errors_comm = 0;
    std::uint16_t errors_count1 = 0, errors_count2 = 0, errors_count3 = 0, errors_count4 = 0;
    std::int8_t battery_remaining = -1;  // %, -1 = bilinmiyor

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(sensors_present); w.put(sensors_enabled); w.put(sensors_health);
        w.put(load); w.put(voltage_battery); w.put(current_battery); w.put(drop_rate_comm);
        w.put(errors_comm); w.put(errors_count1); w.put(errors_count2); w.put(errors_count3);
        w.put(errors_count4); w.put(battery_remaining);
    }
    static SysStatus unpack(const std::uint8_t* in) {
        Reader r(in);
        SysStatus m;
        m.sensors_present = r.get<std::uint32_t>(); m.sensors_enabled = r.get<std::uint32_t>();
        m.sensors_health = r.get<std::uint32_t>(); m.load = r.get<std::uint16_t>();
        m.voltage_battery = r.get<std::uint16_t>(); m.current_battery = r.get<std::int16_t>();
        m.drop_rate_comm = r.get<std::uint16_t>(); m.errors_comm = r.get<std::uint16_t>();
        m.errors_count1 = r.get<std::uint16_t>(); m.errors_count2 = r.get<std::uint16_t>();
        m.errors_count3 = r.get<std::uint16_t>(); m.errors_count4 = r.get<std::uint16_t>();
        m.battery_remaining = r.get<std::int8_t>();
        return m;
    }
};

struct GpsRawInt {
    static constexpr std::uint32_t ID = 24;
    static constexpr std::uint8_t CRC_EXTRA = 24;
    static constexpr std::size_t LEN = 52;

    std::uint64_t time_usec = 0;
    std::int32_t lat = 0, lon = 0, alt = 0;  // degE7, degE7, mm
    std::uint16_t eph = UINT16_MAX, epv = UINT16_MAX, vel = UINT16_MAX, cog = UINT16_MAX;
    std::uint8_t fix_type = 0;
    std::uint8_t satellites_visible = 255;
    // uzantılar
    std::int32_t alt_ellipsoid = 0;
    std::uint32_t h_acc = 0, v_acc = 0, vel_acc = 0, hdg_acc = 0;
    std::uint16_t yaw = 0;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(time_usec); w.put(lat); w.put(lon); w.put(alt); w.put(eph); w.put(epv);
        w.put(vel); w.put(cog); w.put(fix_type); w.put(satellites_visible);
        w.put(alt_ellipsoid); w.put(h_acc); w.put(v_acc); w.put(vel_acc); w.put(hdg_acc);
        w.put(yaw);
    }
    static GpsRawInt unpack(const std::uint8_t* in) {
        Reader r(in);
        GpsRawInt m;
        m.time_usec = r.get<std::uint64_t>(); m.lat = r.get<std::int32_t>();
        m.lon = r.get<std::int32_t>(); m.alt = r.get<std::int32_t>();
        m.eph = r.get<std::uint16_t>(); m.epv = r.get<std::uint16_t>();
        m.vel = r.get<std::uint16_t>(); m.cog = r.get<std::uint16_t>();
        m.fix_type = r.get<std::uint8_t>(); m.satellites_visible = r.get<std::uint8_t>();
        m.alt_ellipsoid = r.get<std::int32_t>(); m.h_acc = r.get<std::uint32_t>();
        m.v_acc = r.get<std::uint32_t>(); m.vel_acc = r.get<std::uint32_t>();
        m.hdg_acc = r.get<std::uint32_t>(); m.yaw = r.get<std::uint16_t>();
        return m;
    }
};

struct Attitude {
    static constexpr std::uint32_t ID = 30;
    static constexpr std::uint8_t CRC_EXTRA = 39;
    static constexpr std::size_t LEN = 28;

    std::uint32_t time_boot_ms = 0;
    float roll = 0, pitch = 0, yaw = 0;  // rad
    float rollspeed = 0, pitchspeed = 0, yawspeed = 0;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(time_boot_ms); w.put(roll); w.put(pitch); w.put(yaw);
        w.put(rollspeed); w.put(pitchspeed); w.put(yawspeed);
    }
    static Attitude unpack(const std::uint8_t* in) {
        Reader r(in);
        Attitude m;
        m.time_boot_ms = r.get<std::uint32_t>(); m.roll = r.get<float>();
        m.pitch = r.get<float>(); m.yaw = r.get<float>(); m.rollspeed = r.get<float>();
        m.pitchspeed = r.get<float>(); m.yawspeed = r.get<float>();
        return m;
    }
};

struct GlobalPositionInt {
    static constexpr std::uint32_t ID = 33;
    static constexpr std::uint8_t CRC_EXTRA = 104;
    static constexpr std::size_t LEN = 28;

    std::uint32_t time_boot_ms = 0;
    std::int32_t lat = 0, lon = 0;   // degE7
    std::int32_t alt = 0;            // mm, MSL
    std::int32_t relative_alt = 0;   // mm
    std::int16_t vx = 0, vy = 0, vz = 0;  // cm/s, NED
    std::uint16_t hdg = UINT16_MAX;  // cdeg

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(time_boot_ms); w.put(lat); w.put(lon); w.put(alt); w.put(relative_alt);
        w.put(vx); w.put(vy); w.put(vz); w.put(hdg);
    }
    static GlobalPositionInt unpack(const std::uint8_t* in) {
        Reader r(in);
        GlobalPositionInt m;
        m.time_boot_ms = r.get<std::uint32_t>(); m.lat = r.get<std::int32_t>();
        m.lon = r.get<std::int32_t>(); m.alt = r.get<std::int32_t>();
        m.relative_alt = r.get<std::int32_t>(); m.vx = r.get<std::int16_t>();
        m.vy = r.get<std::int16_t>(); m.vz = r.get<std::int16_t>();
        m.hdg = r.get<std::uint16_t>();
        return m;
    }
};

// MISSION_REQUEST (40) ve MISSION_REQUEST_INT (51) aynı düzene sahip.
template <std::uint32_t Id, std::uint8_t Crc>
struct MissionRequestT {
    static constexpr std::uint32_t ID = Id;
    static constexpr std::uint8_t CRC_EXTRA = Crc;
    static constexpr std::size_t LEN = 5;

    std::uint16_t seq = 0;
    std::uint8_t target_system = 0;
    std::uint8_t target_component = 0;
    std::uint8_t mission_type = MissionTypeMission;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(seq); w.put(target_system); w.put(target_component); w.put(mission_type);
    }
    static MissionRequestT unpack(const std::uint8_t* in) {
        Reader r(in);
        MissionRequestT m;
        m.seq = r.get<std::uint16_t>(); m.target_system = r.get<std::uint8_t>();
        m.target_component = r.get<std::uint8_t>(); m.mission_type = r.get<std::uint8_t>();
        return m;
    }
};
using MissionRequest = MissionRequestT<40, 230>;
using MissionRequestInt = MissionRequestT<51, 196>;

struct MissionCurrent {
    static constexpr std::uint32_t ID = 42;
    static constexpr std::uint8_t CRC_EXTRA = 28;
    static constexpr std::size_t LEN = 6;

    std::uint16_t seq = 0;
    std::uint16_t total = UINT16_MAX;
    std::uint8_t mission_state = 0;
    std::uint8_t mission_mode = 0;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(seq); w.put(total); w.put(mission_state); w.put(mission_mode);
    }
    static MissionCurrent unpack(const std::uint8_t* in) {
        Reader r(in);
        MissionCurrent m;
        m.seq = r.get<std::uint16_t>(); m.total = r.get<std::uint16_t>();
        m.mission_state = r.get<std::uint8_t>(); m.mission_mode = r.get<std::uint8_t>();
        return m;
    }
};

struct MissionCount {
    static constexpr std::uint32_t ID = 44;
    static constexpr std::uint8_t CRC_EXTRA = 221;
    static constexpr std::size_t LEN = 5;

    std::uint16_t count = 0;
    std::uint8_t target_system = 0;
    std::uint8_t target_component = 0;
    std::uint8_t mission_type = MissionTypeMission;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(count); w.put(target_system); w.put(target_component); w.put(mission_type);
    }
    static MissionCount unpack(const std::uint8_t* in) {
        Reader r(in);
        MissionCount m;
        m.count = r.get<std::uint16_t>(); m.target_system = r.get<std::uint8_t>();
        m.target_component = r.get<std::uint8_t>(); m.mission_type = r.get<std::uint8_t>();
        return m;
    }
};

struct MissionItemReached {
    static constexpr std::uint32_t ID = 46;
    static constexpr std::uint8_t CRC_EXTRA = 11;
    static constexpr std::size_t LEN = 2;

    std::uint16_t seq = 0;

    void pack(std::uint8_t* out) const { Writer(out).put(seq); }
    static MissionItemReached unpack(const std::uint8_t* in) {
        MissionItemReached m;
        m.seq = Reader(in).get<std::uint16_t>();
        return m;
    }
};

struct MissionAck {
    static constexpr std::uint32_t ID = 47;
    static constexpr std::uint8_t CRC_EXTRA = 153;
    static constexpr std::size_t LEN = 4;

    std::uint8_t target_system = 0;
    std::uint8_t target_component = 0;
    std::uint8_t type = 0;  // mission_result
    std::uint8_t mission_type = MissionTypeMission;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(target_system); w.put(target_component); w.put(type); w.put(mission_type);
    }
    static MissionAck unpack(const std::uint8_t* in) {
        Reader r(in);
        MissionAck m;
        m.target_system = r.get<std::uint8_t>(); m.target_component = r.get<std::uint8_t>();
        m.type = r.get<std::uint8_t>(); m.mission_type = r.get<std::uint8_t>();
        return m;
    }
};

struct MissionItemInt {
    static constexpr std::uint32_t ID = 73;
    static constexpr std::uint8_t CRC_EXTRA = 38;
    static constexpr std::size_t LEN = 38;

    float param1 = 0, param2 = 0, param3 = 0, param4 = 0;
    std::int32_t x = 0;  // lat degE7
    std::int32_t y = 0;  // lon degE7
    float z = 0;         // m
    std::uint16_t seq = 0;
    std::uint16_t command = 0;
    std::uint8_t target_system = 0;
    std::uint8_t target_component = 0;
    std::uint8_t frame = FrameGlobalRelativeAlt;
    std::uint8_t current = 0;
    std::uint8_t autocontinue = 1;
    std::uint8_t mission_type = MissionTypeMission;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(param1); w.put(param2); w.put(param3); w.put(param4); w.put(x); w.put(y);
        w.put(z); w.put(seq); w.put(command); w.put(target_system); w.put(target_component);
        w.put(frame); w.put(current); w.put(autocontinue); w.put(mission_type);
    }
    static MissionItemInt unpack(const std::uint8_t* in) {
        Reader r(in);
        MissionItemInt m;
        m.param1 = r.get<float>(); m.param2 = r.get<float>(); m.param3 = r.get<float>();
        m.param4 = r.get<float>(); m.x = r.get<std::int32_t>(); m.y = r.get<std::int32_t>();
        m.z = r.get<float>(); m.seq = r.get<std::uint16_t>(); m.command = r.get<std::uint16_t>();
        m.target_system = r.get<std::uint8_t>(); m.target_component = r.get<std::uint8_t>();
        m.frame = r.get<std::uint8_t>(); m.current = r.get<std::uint8_t>();
        m.autocontinue = r.get<std::uint8_t>(); m.mission_type = r.get<std::uint8_t>();
        return m;
    }
};

struct VfrHud {
    static constexpr std::uint32_t ID = 74;
    static constexpr std::uint8_t CRC_EXTRA = 20;
    static constexpr std::size_t LEN = 20;

    float airspeed = 0, groundspeed = 0, alt = 0, climb = 0;
    std::int16_t heading = 0;     // deg
    std::uint16_t throttle = 0;   // %

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(airspeed); w.put(groundspeed); w.put(alt); w.put(climb); w.put(heading);
        w.put(throttle);
    }
    static VfrHud unpack(const std::uint8_t* in) {
        Reader r(in);
        VfrHud m;
        m.airspeed = r.get<float>(); m.groundspeed = r.get<float>(); m.alt = r.get<float>();
        m.climb = r.get<float>(); m.heading = r.get<std::int16_t>();
        m.throttle = r.get<std::uint16_t>();
        return m;
    }
};

struct CommandLong {
    static constexpr std::uint32_t ID = 76;
    static constexpr std::uint8_t CRC_EXTRA = 152;
    static constexpr std::size_t LEN = 33;

    float param1 = 0, param2 = 0, param3 = 0, param4 = 0, param5 = 0, param6 = 0, param7 = 0;
    std::uint16_t command = 0;
    std::uint8_t target_system = 0;
    std::uint8_t target_component = 0;
    std::uint8_t confirmation = 0;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(param1); w.put(param2); w.put(param3); w.put(param4); w.put(param5);
        w.put(param6); w.put(param7); w.put(command); w.put(target_system);
        w.put(target_component); w.put(confirmation);
    }
    static CommandLong unpack(const std::uint8_t* in) {
        Reader r(in);
        CommandLong m;
        m.param1 = r.get<float>(); m.param2 = r.get<float>(); m.param3 = r.get<float>();
        m.param4 = r.get<float>(); m.param5 = r.get<float>(); m.param6 = r.get<float>();
        m.param7 = r.get<float>(); m.command = r.get<std::uint16_t>();
        m.target_system = r.get<std::uint8_t>(); m.target_component = r.get<std::uint8_t>();
        m.confirmation = r.get<std::uint8_t>();
        return m;
    }
};

struct CommandAck {
    static constexpr std::uint32_t ID = 77;
    static constexpr std::uint8_t CRC_EXTRA = 143;
    static constexpr std::size_t LEN = 10;

    std::uint16_t command = 0;
    std::uint8_t result = 0;
    // uzantılar
    std::uint8_t progress = 0;
    std::int32_t result_param2 = 0;
    std::uint8_t target_system = 0;
    std::uint8_t target_component = 0;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(command); w.put(result); w.put(progress); w.put(result_param2);
        w.put(target_system); w.put(target_component);
    }
    static CommandAck unpack(const std::uint8_t* in) {
        Reader r(in);
        CommandAck m;
        m.command = r.get<std::uint16_t>(); m.result = r.get<std::uint8_t>();
        m.progress = r.get<std::uint8_t>(); m.result_param2 = r.get<std::int32_t>();
        m.target_system = r.get<std::uint8_t>(); m.target_component = r.get<std::uint8_t>();
        return m;
    }
};

struct StatusText {
    static constexpr std::uint32_t ID = 253;
    static constexpr std::uint8_t CRC_EXTRA = 83;
    static constexpr std::size_t LEN = 54;

    std::uint8_t severity = SeverityInfo;
    std::array<char, 50> text{};  // NUL ile bitmek zorunda değil
    std::uint16_t id = 0;
    std::uint8_t chunk_seq = 0;

    void setText(const std::string& s) {
        text.fill('\0');
        std::memcpy(text.data(), s.data(), s.size() < text.size() ? s.size() : text.size());
    }
    std::string textString() const {
        std::size_t n = 0;
        while (n < text.size() && text[n] != '\0') ++n;
        return std::string(text.data(), n);
    }
    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(severity); w.putBytes(text.data(), text.size()); w.put(id); w.put(chunk_seq);
    }
    static StatusText unpack(const std::uint8_t* in) {
        Reader r(in);
        StatusText m;
        m.severity = r.get<std::uint8_t>(); r.getBytes(m.text.data(), m.text.size());
        m.id = r.get<std::uint16_t>(); m.chunk_seq = r.get<std::uint8_t>();
        return m;
    }
};

struct GimbalDeviceAttitudeStatus {
    static constexpr std::uint32_t ID = 285;
    static constexpr std::uint8_t CRC_EXTRA = 137;
    static constexpr std::size_t LEN = 49;

    std::uint32_t time_boot_ms = 0;
    std::array<float, 4> q{1.f, 0.f, 0.f, 0.f};  // w, x, y, z
    float angular_velocity_x = 0, angular_velocity_y = 0, angular_velocity_z = 0;
    std::uint32_t failure_flags = 0;
    std::uint16_t flags = 0;
    std::uint8_t target_system = 0;
    std::uint8_t target_component = 0;
    // uzantılar
    float delta_yaw = 0, delta_yaw_velocity = 0;
    std::uint8_t gimbal_device_id = 0;

    void pack(std::uint8_t* out) const {
        Writer w(out);
        w.put(time_boot_ms);
        for (float v : q) w.put(v);
        w.put(angular_velocity_x); w.put(angular_velocity_y); w.put(angular_velocity_z);
        w.put(failure_flags); w.put(flags); w.put(target_system); w.put(target_component);
        w.put(delta_yaw); w.put(delta_yaw_velocity); w.put(gimbal_device_id);
    }
    static GimbalDeviceAttitudeStatus unpack(const std::uint8_t* in) {
        Reader r(in);
        GimbalDeviceAttitudeStatus m;
        m.time_boot_ms = r.get<std::uint32_t>();
        for (float& v : m.q) v = r.get<float>();
        m.angular_velocity_x = r.get<float>(); m.angular_velocity_y = r.get<float>();
        m.angular_velocity_z = r.get<float>(); m.failure_flags = r.get<std::uint32_t>();
        m.flags = r.get<std::uint16_t>(); m.target_system = r.get<std::uint8_t>();
        m.target_component = r.get<std::uint8_t>(); m.delta_yaw = r.get<float>();
        m.delta_yaw_velocity = r.get<float>(); m.gimbal_device_id = r.get<std::uint8_t>();
        return m;
    }
};

}  // namespace gcs::mav
