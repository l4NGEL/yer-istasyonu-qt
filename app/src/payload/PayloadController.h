#pragma once
// Gimbal ve kamera denetimi (MAVLink gimbal protokolü v2 + kamera protokolü
// komutları). Kaydırıcı sürüklenirken her değişikliği göndermek yerine en son
// istenen açı 10 Hz ile birleştirilerek (coalesce) gönderilir.

#include <QObject>
#include <QTimer>

#include "link/LinkInterface.h"

class Vehicle;

class PayloadController : public QObject {
    Q_OBJECT
    Q_PROPERTY(double targetPitch READ targetPitch NOTIFY targetChanged)
    Q_PROPERTY(double targetYaw READ targetYaw NOTIFY targetChanged)
    Q_PROPERTY(double gimbalPitch READ gimbalPitch NOTIFY gimbalAttitudeChanged)
    Q_PROPERTY(double gimbalYaw READ gimbalYaw NOTIFY gimbalAttitudeChanged)
    Q_PROPERTY(bool gimbalReporting READ gimbalReporting NOTIFY gimbalAttitudeChanged)
    Q_PROPERTY(double zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY cameraChanged)
    Q_PROPERTY(int photoCount READ photoCount NOTIFY cameraChanged)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY cameraChanged)

public:
    PayloadController(LinkInterface* link, Vehicle* vehicle, QObject* parent = nullptr);

    Q_INVOKABLE void setGimbal(double pitchDeg, double yawDeg);
    Q_INVOKABLE void centerGimbal() { setGimbal(0, 0); }
    Q_INVOKABLE void lookDown() { setGimbal(-90, 0); }
    Q_INVOKABLE void setZoom(double percent);
    Q_INVOKABLE void takePhoto();
    Q_INVOKABLE void toggleRecording();

    void setSendIntervalMs(int ms) { sendTimer_.setInterval(ms); }

    double targetPitch() const { return targetPitch_; }
    double targetYaw() const { return targetYaw_; }
    double gimbalPitch() const { return gimbalPitch_; }
    double gimbalYaw() const { return gimbalYaw_; }
    bool gimbalReporting() const { return gimbalReporting_; }
    double zoom() const { return zoom_; }
    bool recording() const { return recording_; }
    int photoCount() const { return photoCount_; }
    QString lastResult() const { return lastResult_; }

signals:
    void targetChanged();
    void gimbalAttitudeChanged();
    void zoomChanged();
    void cameraChanged();

private:
    void flushGimbal();
    void handleFrame(const gcs::mav::Frame& f);
    void onCommandFinished(int command, bool accepted, const QString& text);

    Vehicle* vehicle_;
    QTimer sendTimer_;
    bool gimbalDirty_ = false;
    double targetPitch_ = 0, targetYaw_ = 0;
    double gimbalPitch_ = 0, gimbalYaw_ = 0;
    bool gimbalReporting_ = false;
    double zoom_ = 0;
    bool recording_ = false;
    bool recordingRequested_ = false;
    int photoCount_ = 0;
    QString lastResult_;
};
