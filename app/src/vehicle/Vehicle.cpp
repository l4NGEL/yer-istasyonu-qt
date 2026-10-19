#include "Vehicle.h"

#include <QTime>
#include <cmath>

#include "gcs/mission.hpp"

using namespace gcs;

namespace {
constexpr double kRadToDeg = 57.29577951308232;
constexpr int kMaxLogLines = 60;

QString commandName(std::uint16_t c) {
    switch (c) {
        case mav::cmd::ComponentArmDisarm: return QStringLiteral("ARM/DISARM");
        case mav::cmd::MissionStart: return QStringLiteral("Görev başlat");
        case mav::cmd::DoPauseContinue: return QStringLiteral("Duraklat/Devam");
        case mav::cmd::NavReturnToLaunch: return QStringLiteral("Eve dön");
        case mav::cmd::NavLand: return QStringLiteral("İniş");
        case mav::cmd::DoGimbalManagerPitchYaw: return QStringLiteral("Gimbal");
        case mav::cmd::SetCameraZoom: return QStringLiteral("Yakınlaştırma");
        case mav::cmd::ImageStartCapture: return QStringLiteral("Fotoğraf");
        case mav::cmd::VideoStartCapture: return QStringLiteral("Kayıt başlat");
        case mav::cmd::VideoStopCapture: return QStringLiteral("Kayıt durdur");
        default: return QStringLiteral("Komut %1").arg(c);
    }
}

QString outcomeText(const CommandResult& r) {
    switch (r.outcome) {
        case CommandOutcome::Accepted: return QStringLiteral("kabul edildi");
        case CommandOutcome::Replaced: return QStringLiteral("yenisiyle değiştirildi");
        case CommandOutcome::TimedOut: return QStringLiteral("yanıt yok (%1 deneme)").arg(r.attempts);
        case CommandOutcome::Rejected: break;
    }
    switch (r.mavResult) {
        case mav::result::TemporarilyRejected: return QStringLiteral("geçici olarak reddedildi");
        case mav::result::Denied: return QStringLiteral("reddedildi");
        case mav::result::Unsupported: return QStringLiteral("desteklenmiyor");
        case mav::result::Failed: return QStringLiteral("başarısız");
        default: return QStringLiteral("sonuç %1").arg(r.mavResult);
    }
}
}  // namespace

Vehicle::Vehicle(LinkInterface* link, QObject* parent) : QObject(parent), link_(link) {
    clock_.start();
    connect(link_, &LinkInterface::frameReceived, this, &Vehicle::handleFrame);

    heartbeatTimer_.setSingleShot(true);
    heartbeatTimer_.setInterval(3000);
    connect(&heartbeatTimer_, &QTimer::timeout, this, [this] {
        connected_ = false;
        appendLog(QStringLiteral("Bağlantı koptu: HEARTBEAT zaman aşımı"));
        emit connectionChanged();
    });

    commands_.send = [this](const mav::CommandLong& c) { link_->sendMessage(c); };
    commands_.onResult = [this](const CommandResult& r) {
        if (r.outcome == CommandOutcome::Replaced) return;  // yalnız son değer önemli
        const bool ok = r.outcome == CommandOutcome::Accepted;
        lastCommandResult_ = commandName(r.command) + QStringLiteral(": ") + outcomeText(r);
        appendLog(lastCommandResult_);
        emit commandFinished(r.command, ok, lastCommandResult_);
    };
    tickTimer_.setInterval(100);
    connect(&tickTimer_, &QTimer::timeout, this, [this] { commands_.tick(nowMs()); });
    tickTimer_.start();
}

void Vehicle::sendCommand(std::uint16_t command, float p1, float p2, float p3, float p4, float p5,
                          float p6, float p7, std::uint8_t targetComponent) {
    if (sysid_ == 0) {
        lastCommandResult_ = commandName(command) + QStringLiteral(": araç bağlı değil");
        emit commandFinished(command, false, lastCommandResult_);
        return;
    }
    mav::CommandLong c;
    c.command = command;
    c.target_system = static_cast<std::uint8_t>(sysid_);
    c.target_component = targetComponent;
    c.param1 = p1; c.param2 = p2; c.param3 = p3; c.param4 = p4;
    c.param5 = p5; c.param6 = p6; c.param7 = p7;
    commands_.submit(c, nowMs());
}

void Vehicle::arm() { sendCommand(mav::cmd::ComponentArmDisarm, 1); }
void Vehicle::disarm() { sendCommand(mav::cmd::ComponentArmDisarm, 0); }
void Vehicle::startMission() { sendCommand(mav::cmd::MissionStart, 0, 0); }
void Vehicle::pauseMission() { sendCommand(mav::cmd::DoPauseContinue, 0); }
void Vehicle::resumeMission() { sendCommand(mav::cmd::DoPauseContinue, 1); }
void Vehicle::returnToLaunch() { sendCommand(mav::cmd::NavReturnToLaunch); }
void Vehicle::land() { sendCommand(mav::cmd::NavLand); }

QString Vehicle::modeName() const {
    if (!connected_) return QStringLiteral("—");
    // custom_mode araca/otopilota özeldir. Burada yalnızca bu projedeki SITL'in
    // (MAV_AUTOPILOT_GENERIC) kipleri çözülür; ArduPilot/PX4 tabloları eklenmedi.
    if (autopilot_ == mav::AutopilotGeneric && (baseMode_ & mav::ModeFlagCustomModeEnabled)) {
        switch (customMode_) {
            case 0: return QStringLiteral("BEKLE");
            case 1: return QStringLiteral("GÖREV");
            case 2: return QStringLiteral("EVE DÖNÜŞ");
            case 3: return QStringLiteral("İNİŞ");
            default: break;
        }
    }
    return QStringLiteral("KİP %1").arg(customMode_);
}

void Vehicle::appendLog(const QString& line) {
    statusLog_.prepend(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")) + QStringLiteral("  ") + line);
    while (statusLog_.size() > kMaxLogLines) statusLog_.removeLast();
    emit statusLogChanged();
}

void Vehicle::handleFrame(const mav::Frame& f) {
    if (f.msgid == mav::Heartbeat::ID) {
        const auto hb = mav::decode<mav::Heartbeat>(f);
        if (!hb || hb->type == mav::TypeGcs || hb->autopilot == mav::AutopilotInvalid) return;
        if (sysid_ == 0) {
            sysid_ = f.sysid;  // ilk görülen araca kilitlen
            appendLog(QStringLiteral("Araç bulundu: sistem %1").arg(sysid_));
        }
        if (f.sysid != sysid_ || f.compid != mav::CompIdAutopilot) return;
        heartbeatTimer_.start();
        const bool wasConnected = connected_;
        connected_ = true;
        const bool nowArmed = (hb->base_mode & mav::ModeFlagSafetyArmed) != 0;
        if (nowArmed != armed_) appendLog(nowArmed ? QStringLiteral("ARMED") : QStringLiteral("DISARMED"));
        if (nowArmed && !armed_ && positionValid_) {
            homeLat_ = lat_;
            homeLon_ = lon_;
            homeValid_ = true;
            emit homeChanged();
        }
        armed_ = nowArmed;
        customMode_ = hb->custom_mode;
        baseMode_ = hb->base_mode;
        autopilot_ = hb->autopilot;
        if (!wasConnected) emit connectionChanged();
        emit statusChanged();
        return;
    }
    if (sysid_ == 0 || f.sysid != sysid_) return;

    switch (f.msgid) {
        case mav::SysStatus::ID: {
            const auto s = mav::decode<mav::SysStatus>(f);
            batteryVoltage_ = s->voltage_battery == UINT16_MAX ? 0 : s->voltage_battery / 1000.0;
            batteryRemaining_ = s->battery_remaining;
            emit statusChanged();
            break;
        }
        case mav::GpsRawInt::ID: {
            const auto g = mav::decode<mav::GpsRawInt>(f);
            gpsFix_ = g->fix_type;
            satellites_ = g->satellites_visible == 255 ? 0 : g->satellites_visible;
            emit statusChanged();
            break;
        }
        case mav::GlobalPositionInt::ID: {
            const auto p = mav::decode<mav::GlobalPositionInt>(f);
            lat_ = fromDegE7(p->lat);
            lon_ = fromDegE7(p->lon);
            relAlt_ = p->relative_alt / 1000.0;
            altMsl_ = p->alt / 1000.0;
            if (p->hdg != UINT16_MAX) heading_ = p->hdg / 100.0;
            positionValid_ = true;
            if (!homeValid_) {
                homeLat_ = lat_;
                homeLon_ = lon_;
                homeValid_ = true;
                emit homeChanged();
            }
            emit positionChanged();
            break;
        }
        case mav::Attitude::ID: {
            const auto a = mav::decode<mav::Attitude>(f);
            roll_ = a->roll * kRadToDeg;
            pitch_ = a->pitch * kRadToDeg;
            yaw_ = a->yaw * kRadToDeg;
            emit attitudeChanged();
            break;
        }
        case mav::VfrHud::ID: {
            const auto h = mav::decode<mav::VfrHud>(f);
            groundSpeed_ = h->groundspeed;
            airSpeed_ = h->airspeed;
            climb_ = h->climb;
            throttle_ = h->throttle;
            emit hudChanged();
            break;
        }
        case mav::MissionCurrent::ID: {
            const auto c = mav::decode<mav::MissionCurrent>(f);
            if (c->seq != missionCurrent_ || c->total != missionTotal_) {
                missionCurrent_ = c->seq;
                missionTotal_ = c->total == UINT16_MAX ? 0 : c->total;
                emit missionProgressChanged();
            }
            break;
        }
        case mav::MissionItemReached::ID: {
            const auto r = mav::decode<mav::MissionItemReached>(f);
            appendLog(QStringLiteral("Görev öğesi %1 tamamlandı").arg(r->seq + 1));
            emit missionItemReached(r->seq);
            break;
        }
        case mav::CommandAck::ID: {
            const auto a = mav::decode<mav::CommandAck>(f);
            commands_.handleAck(*a, f.sysid, nowMs());
            break;
        }
        case mav::StatusText::ID: {
            const auto s = mav::decode<mav::StatusText>(f);
            appendLog(QString::fromUtf8(s->textString().c_str()));
            break;
        }
        default:
            break;
    }
}
