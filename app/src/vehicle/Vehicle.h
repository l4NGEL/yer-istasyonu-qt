#pragma once
// Bağlantıdan gelen çerçeveleri çözüp araç durumunu QML'e özellik olarak sunar
// ve komutları (COMMAND_LONG + COMMAND_ACK takibi) gönderir.

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "gcs/command_tracker.hpp"
#include "link/LinkInterface.h"

class Vehicle : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(int systemId READ systemId NOTIFY connectionChanged)

    Q_PROPERTY(bool armed READ armed NOTIFY statusChanged)
    Q_PROPERTY(QString modeName READ modeName NOTIFY statusChanged)
    Q_PROPERTY(double batteryVoltage READ batteryVoltage NOTIFY statusChanged)
    Q_PROPERTY(int batteryRemaining READ batteryRemaining NOTIFY statusChanged)
    Q_PROPERTY(int gpsFixType READ gpsFixType NOTIFY statusChanged)
    Q_PROPERTY(int satellites READ satellites NOTIFY statusChanged)

    Q_PROPERTY(bool positionValid READ positionValid NOTIFY positionChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY positionChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY positionChanged)
    Q_PROPERTY(double relativeAltitude READ relativeAltitude NOTIFY positionChanged)
    Q_PROPERTY(double altitudeMsl READ altitudeMsl NOTIFY positionChanged)
    Q_PROPERTY(double heading READ heading NOTIFY positionChanged)
    Q_PROPERTY(double homeLatitude READ homeLatitude NOTIFY homeChanged)
    Q_PROPERTY(double homeLongitude READ homeLongitude NOTIFY homeChanged)
    Q_PROPERTY(bool homeValid READ homeValid NOTIFY homeChanged)

    Q_PROPERTY(double roll READ roll NOTIFY attitudeChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY attitudeChanged)
    Q_PROPERTY(double yaw READ yaw NOTIFY attitudeChanged)

    Q_PROPERTY(double groundSpeed READ groundSpeed NOTIFY hudChanged)
    Q_PROPERTY(double airSpeed READ airSpeed NOTIFY hudChanged)
    Q_PROPERTY(double climbRate READ climbRate NOTIFY hudChanged)
    Q_PROPERTY(int throttle READ throttle NOTIFY hudChanged)

    Q_PROPERTY(int missionCurrent READ missionCurrent NOTIFY missionProgressChanged)
    Q_PROPERTY(int missionTotal READ missionTotal NOTIFY missionProgressChanged)

    Q_PROPERTY(QStringList statusLog READ statusLog NOTIFY statusLogChanged)
    Q_PROPERTY(QString lastCommandResult READ lastCommandResult NOTIFY commandFinished)

public:
    explicit Vehicle(LinkInterface* link, QObject* parent = nullptr);

    void setHeartbeatTimeoutMs(int ms) { heartbeatTimer_.setInterval(ms); }
    void setCommandTimeoutMs(int ms) { commands_.timeoutMs = ms; }

    Q_INVOKABLE void arm();
    Q_INVOKABLE void disarm();
    Q_INVOKABLE void startMission();
    Q_INVOKABLE void pauseMission();
    Q_INVOKABLE void resumeMission();
    Q_INVOKABLE void returnToLaunch();
    Q_INVOKABLE void land();

    // Faydalı yük denetleyicisi de aynı takipçiyi kullanır (ACK'ler komut
    // kimliğine göre eşleştiği için tek takipçi olmalı).
    void sendCommand(std::uint16_t command, float p1 = 0, float p2 = 0, float p3 = 0, float p4 = 0,
                     float p5 = 0, float p6 = 0, float p7 = 0,
                     std::uint8_t targetComponent = gcs::mav::CompIdAutopilot);

    bool connected() const { return connected_; }
    int systemId() const { return sysid_; }
    bool armed() const { return armed_; }
    QString modeName() const;
    double batteryVoltage() const { return batteryVoltage_; }
    int batteryRemaining() const { return batteryRemaining_; }
    int gpsFixType() const { return gpsFix_; }
    int satellites() const { return satellites_; }
    bool positionValid() const { return positionValid_; }
    double latitude() const { return lat_; }
    double longitude() const { return lon_; }
    double relativeAltitude() const { return relAlt_; }
    double altitudeMsl() const { return altMsl_; }
    double heading() const { return heading_; }
    double homeLatitude() const { return homeLat_; }
    double homeLongitude() const { return homeLon_; }
    bool homeValid() const { return homeValid_; }
    double roll() const { return roll_; }
    double pitch() const { return pitch_; }
    double yaw() const { return yaw_; }
    double groundSpeed() const { return groundSpeed_; }
    double airSpeed() const { return airSpeed_; }
    double climbRate() const { return climb_; }
    int throttle() const { return throttle_; }
    int missionCurrent() const { return missionCurrent_; }
    int missionTotal() const { return missionTotal_; }
    QStringList statusLog() const { return statusLog_; }
    QString lastCommandResult() const { return lastCommandResult_; }

signals:
    void connectionChanged();
    void statusChanged();
    void positionChanged();
    void homeChanged();
    void attitudeChanged();
    void hudChanged();
    void missionProgressChanged();
    void statusLogChanged();
    void commandFinished(int command, bool accepted, const QString& text);
    void missionItemReached(int seq);

private:
    void handleFrame(const gcs::mav::Frame& f);
    void appendLog(const QString& line);
    std::int64_t nowMs() const { return clock_.elapsed(); }

    LinkInterface* link_;
    gcs::CommandTracker commands_;
    QElapsedTimer clock_;
    QTimer heartbeatTimer_;
    QTimer tickTimer_;

    bool connected_ = false;
    int sysid_ = 0;
    bool armed_ = false;
    std::uint32_t customMode_ = 0;
    std::uint8_t baseMode_ = 0;
    std::uint8_t autopilot_ = 0;
    double batteryVoltage_ = 0;
    int batteryRemaining_ = -1;
    int gpsFix_ = 0;
    int satellites_ = 0;
    bool positionValid_ = false;
    double lat_ = 0, lon_ = 0, relAlt_ = 0, altMsl_ = 0, heading_ = 0;
    bool homeValid_ = false;
    double homeLat_ = 0, homeLon_ = 0;
    double roll_ = 0, pitch_ = 0, yaw_ = 0;
    double groundSpeed_ = 0, airSpeed_ = 0, climb_ = 0;
    int throttle_ = 0;
    int missionCurrent_ = 0, missionTotal_ = 0;
    QStringList statusLog_;
    QString lastCommandResult_;
};
