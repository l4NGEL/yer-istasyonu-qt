#include "PayloadController.h"

#include <algorithm>
#include <cmath>

#include "gcs/geo.hpp"
#include "vehicle/Vehicle.h"

using namespace gcs;

PayloadController::PayloadController(LinkInterface* link, Vehicle* vehicle, QObject* parent)
    : QObject(parent), vehicle_(vehicle) {
    connect(link, &LinkInterface::frameReceived, this, &PayloadController::handleFrame);
    connect(vehicle_, &Vehicle::commandFinished, this, &PayloadController::onCommandFinished);
    sendTimer_.setInterval(100);
    connect(&sendTimer_, &QTimer::timeout, this, &PayloadController::flushGimbal);
    sendTimer_.start();
}

void PayloadController::setGimbal(double pitchDeg, double yawDeg) {
    const double p = std::clamp(pitchDeg, -90.0, 30.0);
    const double y = std::clamp(yawDeg, -180.0, 180.0);
    if (qFuzzyCompare(p + 1000, targetPitch_ + 1000) && qFuzzyCompare(y + 1000, targetYaw_ + 1000) &&
        !gimbalDirty_) {
        return;
    }
    targetPitch_ = p;
    targetYaw_ = y;
    gimbalDirty_ = true;
    emit targetChanged();
}

void PayloadController::flushGimbal() {
    if (!gimbalDirty_ || !vehicle_->connected()) return;
    gimbalDirty_ = false;
    // MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW: p1 pitch, p2 yaw (derece), p3/p4 hız
    // (NaN = kullanma), p5 bayraklar, p7 gimbal cihaz kimliği (0 = birincil).
    vehicle_->sendCommand(mav::cmd::DoGimbalManagerPitchYaw, static_cast<float>(targetPitch_),
                          static_cast<float>(targetYaw_), std::nanf(""), std::nanf(""), 0, 0, 0,
                          mav::CompIdAutopilot);
}

void PayloadController::setZoom(double percent) {
    zoom_ = std::clamp(percent, 0.0, 100.0);
    emit zoomChanged();
    vehicle_->sendCommand(mav::cmd::SetCameraZoom, mav::ZoomTypeRange, static_cast<float>(zoom_), 0, 0,
                          0, 0, 0, mav::CompIdCamera);
}

void PayloadController::takePhoto() {
    // p1 kamera kimliği (0 = tümü), p2 aralık (s), p3 toplam görüntü, p4 sıra no
    vehicle_->sendCommand(mav::cmd::ImageStartCapture, 0, 0, 1, static_cast<float>(photoCount_ + 1), 0,
                          0, 0, mav::CompIdCamera);
}

void PayloadController::toggleRecording() {
    recordingRequested_ = !recording_;
    vehicle_->sendCommand(recordingRequested_ ? mav::cmd::VideoStartCapture : mav::cmd::VideoStopCapture,
                          0, 0, 0, 0, 0, 0, 0, mav::CompIdCamera);
}

void PayloadController::onCommandFinished(int command, bool accepted, const QString& text) {
    switch (command) {
        case mav::cmd::ImageStartCapture:
            if (accepted) ++photoCount_;
            break;
        case mav::cmd::VideoStartCapture:
        case mav::cmd::VideoStopCapture:
            if (accepted) recording_ = command == mav::cmd::VideoStartCapture;
            break;
        case mav::cmd::SetCameraZoom:
        case mav::cmd::DoGimbalManagerPitchYaw:
            if (accepted) return;  // başarılı açı/zoom onayı paneli kalabalıklaştırmasın
            break;
        default:
            return;
    }
    lastResult_ = text;
    emit cameraChanged();
}

void PayloadController::handleFrame(const mav::Frame& f) {
    if (f.msgid != mav::GimbalDeviceAttitudeStatus::ID || f.sysid != vehicle_->systemId()) return;
    const auto g = mav::decode<mav::GimbalDeviceAttitudeStatus>(f);
    const auto e = geo::eulerFromQuat({g->q[0], g->q[1], g->q[2], g->q[3]});
    gimbalPitch_ = e.pitch;
    gimbalYaw_ = e.yaw;
    gimbalReporting_ = true;
    emit gimbalAttitudeChanged();
}
