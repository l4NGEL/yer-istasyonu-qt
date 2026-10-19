#include "MissionManager.h"

#include "MissionModel.h"
#include "vehicle/Vehicle.h"

using namespace gcs;

MissionManager::MissionManager(LinkInterface* link, Vehicle* vehicle, MissionModel* model,
                               QObject* parent)
    : QObject(parent), link_(link), vehicle_(vehicle), model_(model) {
    clock_.start();
    uploader_.sendCount = [this](const mav::MissionCount& c) { link_->sendMessage(c); };
    uploader_.sendItem = [this](const mav::MissionItemInt& i) {
        link_->sendMessage(i);
        emit progressChanged();
    };
    uploader_.sendAck = [this](const mav::MissionAck& a) { link_->sendMessage(a); };
    uploader_.onFinished = [this](bool ok, const std::string& msg) {
        tick_.stop();
        lastSucceeded_ = ok;
        lastResult_ = QString::fromStdString(msg);
        emit progressChanged();
        emit stateChanged();
        emit uploadFinished(ok, lastResult_);
    };
    tick_.setInterval(50);
    connect(&tick_, &QTimer::timeout, this, [this] { uploader_.tick(clock_.elapsed()); });
    connect(link_, &LinkInterface::frameReceived, this, &MissionManager::handleFrame);
}

bool MissionManager::upload() {
    if (busy()) return false;
    if (!vehicle_->connected()) {
        lastSucceeded_ = false;
        lastResult_ = QStringLiteral("araç bağlı değil");
        emit stateChanged();
        return false;
    }
    MissionUploader::Config cfg;
    cfg.timeoutMs = timeoutMs_;
    cfg.targetSystem = static_cast<std::uint8_t>(vehicle_->systemId());
    cfg.targetComponent = mav::CompIdAutopilot;
    uploader_.setConfig(cfg);
    if (!uploader_.start(model_->items(), clock_.elapsed())) return false;
    tick_.start();
    lastResult_ = QStringLiteral("yükleniyor…");
    emit stateChanged();
    emit progressChanged();
    return true;
}

void MissionManager::cancel() { uploader_.cancel(); }

void MissionManager::handleFrame(const mav::Frame& f) {
    if (!busy() || f.sysid != vehicle_->systemId()) return;
    const std::int64_t now = clock_.elapsed();
    // Eski otopilotlar MISSION_REQUEST (40) ile ister; aynı şekilde yanıtlanır.
    if (auto r = mav::decode<mav::MissionRequestInt>(f)) {
        if (r->target_system == LinkInterface::kGcsSystemId && r->mission_type == mav::MissionTypeMission)
            uploader_.handleRequest(r->seq, now);
    } else if (auto r1 = mav::decode<mav::MissionRequest>(f)) {
        if (r1->target_system == LinkInterface::kGcsSystemId && r1->mission_type == mav::MissionTypeMission)
            uploader_.handleRequest(r1->seq, now);
    } else if (auto a = mav::decode<mav::MissionAck>(f)) {
        if (a->target_system == LinkInterface::kGcsSystemId) uploader_.handleAck(*a, now);
    }
}
