#include "sim_vehicle.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace gcs::sitl {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr float kDisarmMagic = 21196.f;  // MAVLink: "zorla disarm" sihirli değeri

double clamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

double approach(double current, double target, double maxStep) {
    const double d = target - current;
    if (std::fabs(d) <= maxStep) return target;
    return current + (d > 0 ? maxStep : -maxStep);
}

// MISSION_STATE (common.xml)
constexpr std::uint8_t kMissionStateNoMission = 1;
constexpr std::uint8_t kMissionStateNotStarted = 2;
constexpr std::uint8_t kMissionStateActive = 3;
constexpr std::uint8_t kMissionStatePaused = 4;
constexpr std::uint8_t kMissionStateComplete = 5;
}  // namespace

SimVehicle::SimVehicle(SimConfig cfg) : cfg_(cfg), pos_(cfg.home) {
    receiver_.sendRequest = [this](const mav::MissionRequestInt& r) {
        if (receiverOut_ != nullptr) emit(r, *receiverOut_);
    };
    receiver_.sendAck = [this](const mav::MissionAck& a) {
        if (receiverOut_ != nullptr) emit(a, *receiverOut_);
    };
    receiver_.onComplete = [this](std::vector<MissionItem> items) {
        mission_ = std::move(items);
        currentSeq_ = 0;
        if (mode_ == Mode::Auto) mode_ = Mode::Hold;
        if (receiverOut_ != nullptr) {
            const std::string msg = "Gorev alindi: " + std::to_string(mission_.size()) + " oge";
            status(msg.c_str(), mav::SeverityInfo, *receiverOut_);
        }
    };
}

template <class M>
void SimVehicle::emit(const M& msg, Outbox& out, std::uint8_t compid) {
    out.push_back(mav::encode(msg, mav::Header{seq_[compid]++, cfg_.sysid, compid}));
}

void SimVehicle::status(const char* text, std::uint8_t severity, Outbox& out) {
    mav::StatusText st;
    st.severity = severity;
    st.setText(text);
    emit(st, out);
}

void SimVehicle::ack(std::uint16_t command, std::uint8_t result, const mav::Frame& to, Outbox& out) {
    mav::CommandAck a;
    a.command = command;
    a.result = result;
    a.target_system = to.sysid;
    a.target_component = to.compid;
    emit(a, out);
}

void SimVehicle::handle(const mav::Frame& f, std::int64_t nowMs, Outbox& out) {
    receiverOut_ = &out;
    switch (f.msgid) {
        case mav::CommandLong::ID: {
            auto c = mav::decode<mav::CommandLong>(f);
            if (c && (c->target_system == cfg_.sysid || c->target_system == 0)) {
                handleCommand(*c, f, nowMs, out);
            }
            break;
        }
        case mav::MissionCount::ID: {
            auto c = mav::decode<mav::MissionCount>(f);
            if (c && c->target_system == cfg_.sysid) receiver_.handleCount(*c, f.sysid, f.compid, nowMs);
            break;
        }
        case mav::MissionItemInt::ID: {
            auto it = mav::decode<mav::MissionItemInt>(f);
            if (it && it->target_system == cfg_.sysid) receiver_.handleItem(*it, nowMs);
            break;
        }
        default:
            break;  // GCS HEARTBEAT'i vb. burada yok sayılır
    }
    receiverOut_ = nullptr;
}

void SimVehicle::handleCommand(const mav::CommandLong& c, const mav::Frame& f, std::int64_t,
                               Outbox& out) {
    using namespace mav;
    switch (c.command) {
        case cmd::ComponentArmDisarm:
            if (c.param1 > 0.5f) {
                if (!armed_) status("Motorlar devrede", SeverityInfo, out);
                armed_ = true;
                if (mode_ != Mode::Auto) mode_ = Mode::Hold;
                ack(c.command, result::Accepted, f, out);
            } else if (relAlt_ > 0.5 && c.param2 != kDisarmMagic) {
                status("Havadayken disarm reddedildi", SeverityWarning, out);
                ack(c.command, result::Denied, f, out);
            } else {
                armed_ = false;
                mode_ = Mode::Hold;
                ack(c.command, result::Accepted, f, out);
            }
            return;
        case cmd::MissionStart:
            if (!armed_ || mission_.empty()) {
                status(armed_ ? "Yuklu gorev yok" : "Once arm edin", SeverityWarning, out);
                ack(c.command, result::Denied, f, out);
                return;
            }
            currentSeq_ = static_cast<int>(
                clamp(c.param1, 0, static_cast<double>(mission_.size() - 1)));
            holdUntil_ = -1;
            mode_ = Mode::Auto;
            status("Gorev basladi", SeverityInfo, out);
            ack(c.command, result::Accepted, f, out);
            return;
        case cmd::DoPauseContinue:
            if (c.param1 < 0.5f) {
                mode_ = Mode::Hold;
                ack(c.command, result::Accepted, f, out);
            } else if (armed_ && currentSeq_ < static_cast<int>(mission_.size())) {
                mode_ = Mode::Auto;
                ack(c.command, result::Accepted, f, out);
            } else {
                ack(c.command, result::Denied, f, out);
            }
            return;
        case cmd::NavReturnToLaunch:
            if (!armed_) { ack(c.command, result::Denied, f, out); return; }
            mode_ = Mode::ReturnToLaunch;
            status("Eve donus", SeverityInfo, out);
            ack(c.command, result::Accepted, f, out);
            return;
        case cmd::NavLand:
            if (!armed_) { ack(c.command, result::Denied, f, out); return; }
            mode_ = Mode::Land;
            ack(c.command, result::Accepted, f, out);
            return;
        case cmd::DoGimbalManagerPitchYaw:
            // NaN = "bu ekseni değiştirme" (common.xml)
            if (!std::isnan(c.param1)) gimbalPitchTarget_ = clamp(c.param1, -90.0, 30.0);
            if (!std::isnan(c.param2)) gimbalYawTarget_ = clamp(c.param2, -180.0, 180.0);
            ack(c.command, result::Accepted, f, out);
            return;
        case cmd::SetCameraZoom:
            if (static_cast<int>(c.param1) != ZoomTypeRange) {
                ack(c.command, result::Unsupported, f, out);
                return;
            }
            zoom_ = static_cast<float>(clamp(c.param2, 0.0, 100.0));
            ack(c.command, result::Accepted, f, out);
            return;
        case cmd::ImageStartCapture: {
            ++photos_;
            const std::string msg = "Fotograf #" + std::to_string(photos_) + " kaydedildi";
            status(msg.c_str(), SeverityInfo, out);
            ack(c.command, result::Accepted, f, out);
            return;
        }
        case cmd::VideoStartCapture:
            recording_ = true;
            status("Video kaydi basladi", SeverityInfo, out);
            ack(c.command, result::Accepted, f, out);
            return;
        case cmd::VideoStopCapture:
            if (!recording_) { ack(c.command, result::Denied, f, out); return; }
            recording_ = false;
            status("Video kaydi durdu", SeverityInfo, out);
            ack(c.command, result::Accepted, f, out);
            return;
        default:
            ack(c.command, result::Unsupported, f, out);
            return;
    }
}

void SimVehicle::flyTowards(const geo::LatLon& target, double targetAlt, double dt, double& distOut) {
    const double dist = geo::distanceM(pos_, target);
    distOut = dist;
    if (dist > 0.05) {
        const double brg = geo::bearingDeg(pos_, target);
        const double err = geo::wrap180(brg - heading_);
        heading_ = geo::wrap360(heading_ + clamp(err, -90.0 * dt, 90.0 * dt));
        roll_ = clamp(err * 0.4, -25.0, 25.0);
        // Hedefe yaklaşırken yavaşla (yaklaşık 2 m/s² frenleme)
        const double desiredSpeed = std::min(cfg_.cruiseSpeed, std::sqrt(2.0 * 2.0 * dist));
        groundSpeed_ = approach(groundSpeed_, desiredSpeed, 4.0 * dt);
        pos_ = geo::destination(pos_, brg, std::min(groundSpeed_ * dt, dist));
    } else {
        groundSpeed_ = 0;
        roll_ = approach(roll_, 0, 30 * dt);
    }
    const double prevAlt = relAlt_;
    relAlt_ = approach(relAlt_, targetAlt, cfg_.climbRate * dt);
    climb_ = dt > 0 ? (relAlt_ - prevAlt) / dt : 0;
    pitch_ = -8.0 * groundSpeed_ / cfg_.cruiseSpeed;
}

void SimVehicle::physics(double dt, Outbox& out) {
    simTime_ += dt;
    gimbalPitch_ = approach(gimbalPitch_, gimbalPitchTarget_, cfg_.gimbalSlewDegPerS * dt);
    gimbalYaw_ = approach(gimbalYaw_, gimbalYawTarget_, cfg_.gimbalSlewDegPerS * dt);
    if (!armed_) {
        groundSpeed_ = 0;
        climb_ = 0;
        roll_ = pitch_ = 0;
        return;
    }
    armedSeconds_ += dt;
    double dist = 0;

    switch (mode_) {
        case Mode::Hold:
            flyTowards(pos_, relAlt_, dt, dist);
            break;
        case Mode::Auto: {
            if (currentSeq_ >= static_cast<int>(mission_.size())) {
                mode_ = Mode::Hold;
                status("Gorev tamamlandi", mav::SeverityInfo, out);
                break;
            }
            const MissionItem& it = mission_[static_cast<std::size_t>(currentSeq_)];
            bool reached = false;
            switch (it.command) {
                case mav::cmd::NavTakeoff:
                    flyTowards(pos_, it.alt, dt, dist);
                    reached = std::fabs(relAlt_ - it.alt) < 0.5;
                    break;
                case mav::cmd::NavWaypoint: {
                    flyTowards({it.lat, it.lon}, it.alt, dt, dist);
                    const double radius = it.param2 > 0 ? it.param2 : cfg_.defaultAcceptRadius;
                    if (dist < radius && std::fabs(relAlt_ - it.alt) < 1.0) {
                        if (it.param1 <= 0) {
                            reached = true;
                        } else if (holdUntil_ < 0) {
                            holdUntil_ = simTime_ + it.param1;
                        } else if (simTime_ >= holdUntil_) {
                            reached = true;
                        }
                    }
                    break;
                }
                case mav::cmd::NavLand:
                    if (geo::distanceM(pos_, {it.lat, it.lon}) > cfg_.defaultAcceptRadius) {
                        flyTowards({it.lat, it.lon}, relAlt_, dt, dist);
                    } else {
                        mode_ = Mode::Land;
                        reached = true;
                    }
                    break;
                case mav::cmd::NavReturnToLaunch:
                    mode_ = Mode::ReturnToLaunch;
                    reached = true;
                    break;
                default:
                    reached = true;  // desteklenmeyen öğe atlanır
                    break;
            }
            if (reached) {
                mav::MissionItemReached r;
                r.seq = static_cast<std::uint16_t>(currentSeq_);
                emit(r, out);
                ++currentSeq_;
                holdUntil_ = -1;
            }
            break;
        }
        case Mode::ReturnToLaunch:
            flyTowards(cfg_.home, std::max(relAlt_, 30.0), dt, dist);
            if (dist < cfg_.defaultAcceptRadius) mode_ = Mode::Land;
            break;
        case Mode::Land: {
            groundSpeed_ = approach(groundSpeed_, 0, 4.0 * dt);
            const double prev = relAlt_;
            relAlt_ = std::max(0.0, relAlt_ - 0.5 * cfg_.climbRate * dt);
            climb_ = dt > 0 ? (relAlt_ - prev) / dt : 0;
            if (relAlt_ <= 0.0) {
                armed_ = false;
                mode_ = Mode::Hold;
                status("Inis tamamlandi, disarm", mav::SeverityInfo, out);
            }
            break;
        }
    }
}

void SimVehicle::step(double dt, std::int64_t nowMs, Outbox& out) {
    bootMs_ = static_cast<std::uint32_t>(nowMs);
    physics(dt, out);
    receiverOut_ = &out;
    receiver_.tick(nowMs);
    receiverOut_ = nullptr;
    telemetry(nowMs, out);
}

void SimVehicle::telemetry(std::int64_t nowMs, Outbox& out) {
    auto due = [nowMs](Stream& s) {
        if (nowMs < s.next) return false;
        s.next = (nowMs - s.next > s.periodMs) ? nowMs + s.periodMs : s.next + s.periodMs;
        return true;
    };
    const double headingRad = geo::wrap180(heading_) * kPi / 180.0;
    const auto cdeg = [](double deg) {
        return static_cast<std::uint16_t>(std::lround(geo::wrap360(deg) * 100) % 36000);
    };

    if (due(hb_)) {
        mav::Heartbeat hb;
        hb.type = mav::TypeQuadrotor;
        hb.autopilot = mav::AutopilotGeneric;
        hb.base_mode = mav::ModeFlagCustomModeEnabled;
        if (armed_) hb.base_mode |= mav::ModeFlagSafetyArmed;
        if (mode_ == Mode::Auto) hb.base_mode |= mav::ModeFlagAutoEnabled;
        hb.custom_mode = static_cast<std::uint32_t>(mode_);
        hb.system_status = armed_ ? mav::StateActive : mav::StateStandby;
        emit(hb, out);
    }
    if (due(sys_)) {
        mav::SysStatus s;
        const double remaining = std::max(0.0, 100.0 - armedSeconds_ * 0.08);
        s.battery_remaining = static_cast<std::int8_t>(std::lround(remaining));
        s.voltage_battery = static_cast<std::uint16_t>(std::lround(14000 + 2800 * remaining / 100.0));
        s.current_battery = static_cast<std::int16_t>(armed_ ? 1200 + groundSpeed_ * 80 : 50);
        s.load = 310;
        emit(s, out);
    }
    if (due(gps_)) {
        mav::GpsRawInt g;
        g.time_usec = static_cast<std::uint64_t>(nowMs) * 1000;
        g.lat = toDegE7(pos_.lat);
        g.lon = toDegE7(pos_.lon);
        g.alt = static_cast<std::int32_t>(std::lround((cfg_.homeAltMsl + relAlt_) * 1000));
        g.eph = 80;
        g.epv = 120;
        g.vel = static_cast<std::uint16_t>(std::lround(groundSpeed_ * 100));
        g.cog = cdeg(heading_);
        g.fix_type = 3;
        g.satellites_visible = 14;
        emit(g, out);
    }
    if (due(gpi_)) {
        mav::GlobalPositionInt p;
        p.time_boot_ms = bootMs_;
        p.lat = toDegE7(pos_.lat);
        p.lon = toDegE7(pos_.lon);
        p.alt = static_cast<std::int32_t>(std::lround((cfg_.homeAltMsl + relAlt_) * 1000));
        p.relative_alt = static_cast<std::int32_t>(std::lround(relAlt_ * 1000));
        p.vx = static_cast<std::int16_t>(std::lround(groundSpeed_ * std::cos(headingRad) * 100));
        p.vy = static_cast<std::int16_t>(std::lround(groundSpeed_ * std::sin(headingRad) * 100));
        p.vz = static_cast<std::int16_t>(std::lround(-climb_ * 100));
        p.hdg = cdeg(heading_);
        emit(p, out);
    }
    if (due(att_)) {
        mav::Attitude a;
        a.time_boot_ms = bootMs_;
        a.roll = static_cast<float>(roll_ * kPi / 180.0);
        a.pitch = static_cast<float>(pitch_ * kPi / 180.0);
        a.yaw = static_cast<float>(headingRad);
        emit(a, out);
    }
    if (due(hud_)) {
        mav::VfrHud h;
        h.airspeed = static_cast<float>(groundSpeed_);
        h.groundspeed = static_cast<float>(groundSpeed_);
        h.alt = static_cast<float>(cfg_.homeAltMsl + relAlt_);
        h.climb = static_cast<float>(climb_);
        h.heading = static_cast<std::int16_t>(std::lround(geo::wrap360(heading_)) % 360);
        h.throttle = static_cast<std::uint16_t>(armed_ ? 45 + groundSpeed_ * 2 : 0);
        emit(h, out);
    }
    if (due(cur_)) {
        mav::MissionCurrent c;
        c.seq = static_cast<std::uint16_t>(std::min(currentSeq_, 0xFFFF));
        c.total = static_cast<std::uint16_t>(mission_.size());
        if (mission_.empty()) c.mission_state = kMissionStateNoMission;
        else if (currentSeq_ >= static_cast<int>(mission_.size())) c.mission_state = kMissionStateComplete;
        else if (mode_ == Mode::Auto) c.mission_state = kMissionStateActive;
        else if (currentSeq_ > 0) c.mission_state = kMissionStatePaused;
        else c.mission_state = kMissionStateNotStarted;
        emit(c, out);
    }
    if (due(gimbal_)) {
        mav::GimbalDeviceAttitudeStatus g;
        g.time_boot_ms = bootMs_;
        const geo::Quat q = geo::quatFromEuler({0.0, gimbalPitch_, gimbalYaw_});
        g.q = {static_cast<float>(q.w), static_cast<float>(q.x), static_cast<float>(q.y),
               static_cast<float>(q.z)};
        g.delta_yaw = static_cast<float>(gimbalYaw_ * kPi / 180.0);
        emit(g, out, mav::CompIdGimbal);
    }
}

}  // namespace gcs::sitl
