#pragma once
// Görev öğesi modeli ve MAVLink görev yükleme protokolü (iki uç).
//
// Protokol (common.xml "Mission Protocol"):
//   GCS  -> MISSION_COUNT(n)
//   Araç -> MISSION_REQUEST_INT(0)   GCS -> MISSION_ITEM_INT(0)
//   ...
//   Araç -> MISSION_REQUEST_INT(n-1) GCS -> MISSION_ITEM_INT(n-1)
//   Araç -> MISSION_ACK(ACCEPTED)
// Her iki uç zaman aşımında son mesajını yeniden gönderir. Zaman dışarıdan
// verilir (nowMs); böylece durum makineleri gerçek saat ve soket olmadan test edilir.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "gcs/mavlink/messages.hpp"

namespace gcs {

struct MissionItem {
    std::uint16_t command = mav::cmd::NavWaypoint;
    double lat = 0;
    double lon = 0;
    float alt = 50.f;     // eve göre bağıl yükseklik, m
    float param1 = 0.f;   // WAYPOINT: bekleme süresi (s)
    float param2 = 0.f;   // WAYPOINT: kabul yarıçapı (m), 0 = araç varsayılanı
    float param3 = 0.f;
    float param4 = 0.f;
};

std::int32_t toDegE7(double deg);
double fromDegE7(std::int32_t v);

mav::MissionItemInt toWire(const MissionItem& item, std::uint16_t seq, std::uint8_t targetSys,
                           std::uint8_t targetComp);
MissionItem fromWire(const mav::MissionItemInt& w);

// Ardışık konumlu öğeler arasındaki toplam yatay yol (m).
double pathLengthM(const std::vector<MissionItem>& items);

// Listede bir öğeyi from'dan to'ya taşır (diğerleri kayar). Geçersiz indekste false.
bool moveItem(std::vector<MissionItem>& items, int from, int to);

// --- GCS tarafı --------------------------------------------------------------
class MissionUploader {
public:
    enum class State { Idle, Sending, Succeeded, Failed };

    struct Config {
        std::int64_t timeoutMs = 1500;
        int maxRetries = 4;
        std::uint8_t targetSystem = 1;
        std::uint8_t targetComponent = mav::CompIdAutopilot;
    };

    std::function<void(const mav::MissionCount&)> sendCount;
    std::function<void(const mav::MissionItemInt&)> sendItem;
    std::function<void(const mav::MissionAck&)> sendAck;
    std::function<void(bool ok, const std::string& message)> onFinished;

    MissionUploader() = default;
    explicit MissionUploader(Config cfg) : cfg_(cfg) {}

    void setConfig(const Config& cfg) { cfg_ = cfg; }
    bool start(std::vector<MissionItem> items, std::int64_t nowMs);
    void handleRequest(std::uint16_t seq, std::int64_t nowMs);
    void handleAck(const mav::MissionAck& ack, std::int64_t nowMs);
    void tick(std::int64_t nowMs);
    void cancel();

    State state() const { return state_; }
    int itemCount() const { return static_cast<int>(items_.size()); }
    int highestRequested() const { return highestRequested_; }
    int retransmissions() const { return retransmissions_; }
    double progress() const;

private:
    void finish(bool ok, const std::string& msg);
    void resendLast(std::int64_t nowMs);

    Config cfg_;
    State state_ = State::Idle;
    std::vector<MissionItem> items_;
    int lastSent_ = -1;  // -1: en son MISSION_COUNT gönderildi
    int highestRequested_ = -1;
    int retries_ = 0;
    int retransmissions_ = 0;
    std::int64_t deadline_ = 0;
};

// --- Araç tarafı (SITL) -----------------------------------------------------
class MissionReceiver {
public:
    std::function<void(const mav::MissionRequestInt&)> sendRequest;
    std::function<void(const mav::MissionAck&)> sendAck;
    std::function<void(std::vector<MissionItem>)> onComplete;

    std::int64_t timeoutMs = 1000;
    int maxRetries = 5;
    std::uint16_t maxItems = 500;

    void handleCount(const mav::MissionCount& c, std::uint8_t gcsSys, std::uint8_t gcsComp,
                     std::int64_t nowMs);
    void handleItem(const mav::MissionItemInt& item, std::int64_t nowMs);
    void tick(std::int64_t nowMs);
    bool active() const { return active_; }

private:
    void request(std::uint16_t seq, std::int64_t nowMs);
    void ack(std::uint8_t type);

    bool active_ = false;
    std::uint16_t count_ = 0;
    std::uint16_t expected_ = 0;
    std::uint16_t lastCompletedCount_ = 0;
    std::uint8_t gcsSys_ = 0, gcsComp_ = 0;
    int retries_ = 0;
    std::int64_t deadline_ = 0;
    std::vector<MissionItem> items_;
};

}  // namespace gcs
