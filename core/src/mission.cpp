#include "gcs/mission.hpp"

#include <cmath>

#include "gcs/geo.hpp"

namespace gcs {

std::int32_t toDegE7(double deg) { return static_cast<std::int32_t>(std::llround(deg * 1e7)); }
double fromDegE7(std::int32_t v) { return static_cast<double>(v) / 1e7; }

mav::MissionItemInt toWire(const MissionItem& item, std::uint16_t seq, std::uint8_t targetSys,
                           std::uint8_t targetComp) {
    mav::MissionItemInt w;
    w.param1 = item.param1;
    w.param2 = item.param2;
    w.param3 = item.param3;
    w.param4 = item.param4;
    w.x = toDegE7(item.lat);
    w.y = toDegE7(item.lon);
    w.z = item.alt;
    w.seq = seq;
    w.command = item.command;
    w.target_system = targetSys;
    w.target_component = targetComp;
    w.frame = mav::FrameGlobalRelativeAlt;
    w.current = seq == 0 ? 1 : 0;
    w.autocontinue = 1;
    w.mission_type = mav::MissionTypeMission;
    return w;
}

MissionItem fromWire(const mav::MissionItemInt& w) {
    MissionItem m;
    m.command = w.command;
    m.lat = fromDegE7(w.x);
    m.lon = fromDegE7(w.y);
    m.alt = w.z;
    m.param1 = w.param1;
    m.param2 = w.param2;
    m.param3 = w.param3;
    m.param4 = w.param4;
    return m;
}

namespace {
// Kalkış bulunduğu yerden dikey yapılır (lat/lon kullanılmaz), RTL konumsuzdur.
bool hasPosition(const MissionItem& m) {
    return m.command == mav::cmd::NavWaypoint || m.command == mav::cmd::NavLand;
}
}  // namespace

double pathLengthM(const std::vector<MissionItem>& items) {
    double total = 0;
    const MissionItem* prev = nullptr;
    for (const auto& it : items) {
        if (!hasPosition(it)) continue;
        if (prev != nullptr) total += geo::distanceM({prev->lat, prev->lon}, {it.lat, it.lon});
        prev = &it;
    }
    return total;
}

bool moveItem(std::vector<MissionItem>& items, int from, int to) {
    const int n = static_cast<int>(items.size());
    if (from < 0 || from >= n || to < 0 || to >= n) return false;
    if (from == to) return true;
    MissionItem moved = items[static_cast<std::size_t>(from)];
    items.erase(items.begin() + from);
    items.insert(items.begin() + to, moved);
    return true;
}

// --- MissionUploader ---------------------------------------------------------

bool MissionUploader::start(std::vector<MissionItem> items, std::int64_t nowMs) {
    if (state_ == State::Sending) return false;
    if (items.size() > UINT16_MAX) return false;
    items_ = std::move(items);
    state_ = State::Sending;
    lastSent_ = -1;
    highestRequested_ = -1;
    retries_ = 0;
    retransmissions_ = 0;
    mav::MissionCount c;
    c.count = static_cast<std::uint16_t>(items_.size());
    c.target_system = cfg_.targetSystem;
    c.target_component = cfg_.targetComponent;
    if (sendCount) sendCount(c);
    deadline_ = nowMs + cfg_.timeoutMs;
    return true;
}

void MissionUploader::handleRequest(std::uint16_t seq, std::int64_t nowMs) {
    if (state_ != State::Sending) return;
    if (seq >= items_.size()) {
        mav::MissionAck a;
        a.target_system = cfg_.targetSystem;
        a.target_component = cfg_.targetComponent;
        a.type = mav::mission_result::InvalidSequence;
        if (sendAck) sendAck(a);
        finish(false, "araç geçersiz sıra numarası istedi: " + std::to_string(seq));
        return;
    }
    if (static_cast<int>(seq) > highestRequested_) {
        highestRequested_ = seq;
        retries_ = 0;  // ilerleme var, yeniden deneme sayacı sıfırlanır
    }
    if (static_cast<int>(seq) <= lastSent_) ++retransmissions_;  // araç tekrar istedi
    lastSent_ = seq;
    if (sendItem) {
        sendItem(toWire(items_[seq], seq, cfg_.targetSystem, cfg_.targetComponent));
    }
    deadline_ = nowMs + cfg_.timeoutMs;
}

void MissionUploader::handleAck(const mav::MissionAck& ack, std::int64_t) {
    if (state_ != State::Sending) return;
    if (ack.mission_type != mav::MissionTypeMission) return;
    if (ack.type != mav::mission_result::Accepted) {
        finish(false, "araç görevi reddetti (MAV_MISSION_RESULT=" + std::to_string(ack.type) + ")");
        return;
    }
    const int needed = static_cast<int>(items_.size()) - 1;
    if (highestRequested_ < needed) {
        finish(false, "araç tüm öğeleri istemeden onay verdi");
        return;
    }
    finish(true, std::to_string(items_.size()) + " öğe yüklendi");
}

void MissionUploader::tick(std::int64_t nowMs) {
    if (state_ != State::Sending || nowMs < deadline_) return;
    if (retries_ >= cfg_.maxRetries) {
        finish(false, "zaman aşımı: araç yanıt vermiyor");
        return;
    }
    ++retries_;
    resendLast(nowMs);
}

void MissionUploader::resendLast(std::int64_t nowMs) {
    ++retransmissions_;
    if (lastSent_ < 0) {
        mav::MissionCount c;
        c.count = static_cast<std::uint16_t>(items_.size());
        c.target_system = cfg_.targetSystem;
        c.target_component = cfg_.targetComponent;
        if (sendCount) sendCount(c);
    } else if (sendItem) {
        const auto seq = static_cast<std::uint16_t>(lastSent_);
        sendItem(toWire(items_[seq], seq, cfg_.targetSystem, cfg_.targetComponent));
    }
    deadline_ = nowMs + cfg_.timeoutMs;
}

void MissionUploader::cancel() {
    if (state_ != State::Sending) return;
    mav::MissionAck a;
    a.target_system = cfg_.targetSystem;
    a.target_component = cfg_.targetComponent;
    a.type = mav::mission_result::OperationCancelled;
    if (sendAck) sendAck(a);
    finish(false, "kullanıcı iptal etti");
}

double MissionUploader::progress() const {
    if (items_.empty()) return state_ == State::Succeeded ? 1.0 : 0.0;
    return static_cast<double>(highestRequested_ + 1) / static_cast<double>(items_.size());
}

void MissionUploader::finish(bool ok, const std::string& msg) {
    state_ = ok ? State::Succeeded : State::Failed;
    if (onFinished) onFinished(ok, msg);
}

// --- MissionReceiver ---------------------------------------------------------

void MissionReceiver::handleCount(const mav::MissionCount& c, std::uint8_t gcsSys,
                                  std::uint8_t gcsComp, std::int64_t nowMs) {
    if (c.mission_type != mav::MissionTypeMission) return;
    gcsSys_ = gcsSys;
    gcsComp_ = gcsComp;
    if (c.count > maxItems) {
        ack(mav::mission_result::NoSpace);
        active_ = false;
        return;
    }
    items_.clear();
    lastCompletedCount_ = 0;
    count_ = c.count;
    expected_ = 0;
    retries_ = 0;
    if (count_ == 0) {
        active_ = false;
        ack(mav::mission_result::Accepted);
        if (onComplete) onComplete({});
        return;
    }
    active_ = true;
    request(0, nowMs);
}

void MissionReceiver::handleItem(const mav::MissionItemInt& item, std::int64_t nowMs) {
    if (item.mission_type != mav::MissionTypeMission) return;
    if (!active_) {
        // Son ACK kaybolduysa GCS son öğeyi yeniden gönderir; onayı tekrarla.
        if (lastCompletedCount_ > 0 && item.seq == lastCompletedCount_ - 1) {
            ack(mav::mission_result::Accepted);
        }
        return;
    }
    if (item.seq != expected_) {
        request(expected_, nowMs);  // sıra dışı/yinelenen öğe: beklenen öğeyi yeniden iste
        return;
    }
    items_.push_back(fromWire(item));
    ++expected_;
    retries_ = 0;
    if (expected_ == count_) {
        active_ = false;
        lastCompletedCount_ = count_;
        ack(mav::mission_result::Accepted);
        if (onComplete) onComplete(items_);
        return;
    }
    request(expected_, nowMs);
}

void MissionReceiver::tick(std::int64_t nowMs) {
    if (!active_ || nowMs < deadline_) return;
    if (retries_ >= maxRetries) {
        active_ = false;
        ack(mav::mission_result::OperationCancelled);
        return;
    }
    ++retries_;
    request(expected_, nowMs);
}

void MissionReceiver::request(std::uint16_t seq, std::int64_t nowMs) {
    mav::MissionRequestInt r;
    r.seq = seq;
    r.target_system = gcsSys_;
    r.target_component = gcsComp_;
    if (sendRequest) sendRequest(r);
    deadline_ = nowMs + timeoutMs;
}

void MissionReceiver::ack(std::uint8_t type) {
    mav::MissionAck a;
    a.target_system = gcsSys_;
    a.target_component = gcsComp_;
    a.type = type;
    if (sendAck) sendAck(a);
}

}  // namespace gcs
