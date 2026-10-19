#include "gcs/mavlink/codec.hpp"

#include <algorithm>

#include "gcs/mavlink/crc.hpp"
#include "gcs/mavlink/messages.hpp"

namespace gcs::mav {

namespace {

template <class M>
constexpr MsgInfo info() { return MsgInfo{M::ID, M::CRC_EXTRA, M::LEN}; }

constexpr MsgInfo kTable[] = {
    info<Heartbeat>(),      info<SysStatus>(),         info<GpsRawInt>(),
    info<Attitude>(),       info<GlobalPositionInt>(), info<MissionRequest>(),
    info<MissionCurrent>(), info<MissionCount>(),      info<MissionItemReached>(),
    info<MissionAck>(),     info<MissionRequestInt>(), info<MissionItemInt>(),
    info<VfrHud>(),         info<CommandLong>(),       info<CommandAck>(),
    info<StatusText>(),     info<GimbalDeviceAttitudeStatus>(),
};

}  // namespace

const MsgInfo* findMsgInfo(std::uint32_t msgid) {
    for (const auto& m : kTable) {
        if (m.id == msgid) return &m;
    }
    return nullptr;
}

std::vector<std::uint8_t> encodeFrame(const Header& h, std::uint32_t msgid, std::uint8_t crcExtra,
                                      const std::uint8_t* payload, std::size_t len, int version) {
    std::vector<std::uint8_t> out;
    if (version == 1) {
        out.reserve(6 + len + 2);
        out.push_back(kStxV1);
        out.push_back(static_cast<std::uint8_t>(len));
        out.push_back(h.seq);
        out.push_back(h.sysid);
        out.push_back(h.compid);
        out.push_back(static_cast<std::uint8_t>(msgid));
    } else {
        while (len > 1 && payload[len - 1] == 0) --len;
        out.reserve(10 + len + 2);
        out.push_back(kStxV2);
        out.push_back(static_cast<std::uint8_t>(len));
        out.push_back(0);  // incompat_flags
        out.push_back(0);  // compat_flags
        out.push_back(h.seq);
        out.push_back(h.sysid);
        out.push_back(h.compid);
        out.push_back(static_cast<std::uint8_t>(msgid & 0xFF));
        out.push_back(static_cast<std::uint8_t>((msgid >> 8) & 0xFF));
        out.push_back(static_cast<std::uint8_t>((msgid >> 16) & 0xFF));
    }
    out.insert(out.end(), payload, payload + len);
    std::uint16_t crc = crcCalculate(out.data() + 1, out.size() - 1);
    crcAccumulate(crcExtra, crc);
    out.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    out.push_back(static_cast<std::uint8_t>(crc >> 8));
    return out;
}

void Parser::reset() {
    buf_.clear();
    stats_ = {};
}

void Parser::feed(const std::uint8_t* data, std::size_t len, const Callback& onFrame) {
    buf_.insert(buf_.end(), data, data + len);
    std::size_t pos = 0;

    while (pos < buf_.size()) {
        if (buf_[pos] != kStxV1 && buf_[pos] != kStxV2) {
            ++pos;
            ++stats_.bytesDiscarded;
            continue;
        }
        const std::size_t avail = buf_.size() - pos;
        const std::uint8_t* p = buf_.data() + pos;
        const bool v2 = p[0] == kStxV2;
        const std::size_t hdrLen = v2 ? 10 : 6;
        if (avail < 3) break;

        const std::uint8_t payloadLen = p[1];
        if (v2 && (p[2] & ~kIncompatSigned) != 0) {
            // Anlamadığımız bir incompat bayrağı: spesifikasyona göre çerçeve işlenmemeli.
            ++stats_.unknownOrBad;
            ++pos;
            continue;
        }
        const bool isSigned = v2 && (p[2] & kIncompatSigned);
        const std::size_t total = hdrLen + payloadLen + 2 + (isSigned ? 13 : 0);
        if (avail < total) break;  // çerçevenin geri kalanını bekle

        const std::uint32_t msgid =
            v2 ? (std::uint32_t(p[7]) | (std::uint32_t(p[8]) << 8) | (std::uint32_t(p[9]) << 16))
               : p[5];
        const MsgInfo* mi = findMsgInfo(msgid);
        if (mi == nullptr) {
            ++stats_.unknownOrBad;
            ++pos;
            continue;
        }

        std::uint16_t crc = crcCalculate(p + 1, hdrLen - 1 + payloadLen);
        crcAccumulate(mi->crcExtra, crc);
        const std::uint16_t wireCrc =
            static_cast<std::uint16_t>(p[hdrLen + payloadLen] | (p[hdrLen + payloadLen + 1] << 8));
        if (crc != wireCrc) {
            ++stats_.crcErrors;
            ++pos;
            continue;
        }

        Frame f;
        f.version = v2 ? 2 : 1;
        f.seq = v2 ? p[4] : p[2];
        f.sysid = v2 ? p[5] : p[3];
        f.compid = v2 ? p[6] : p[4];
        f.msgid = msgid;
        f.isSigned = isSigned;
        f.wireLen = payloadLen;
        f.payload.assign(mi->len, 0);
        std::copy_n(p + hdrLen, std::min<std::size_t>(payloadLen, mi->len), f.payload.begin());

        ++stats_.framesOk;
        pos += total;
        onFrame(f);
    }

    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(pos));
}

std::uint32_t SeqTracker::observe(std::uint8_t sysid, std::uint8_t compid, std::uint8_t seq) {
    ++received_;
    const std::uint16_t key = static_cast<std::uint16_t>((sysid << 8) | compid);
    auto it = next_.find(key);
    std::uint32_t gap = 0;
    if (it != next_.end()) {
        const std::uint8_t d = static_cast<std::uint8_t>(seq - it->second);
        // Yarım turdan büyük fark yeniden sıralama ya da karşı tarafın yeniden
        // başlaması sayılır; kayıp olarak yazılmaz.
        if (d < 128) gap = d;
    }
    lost_ += gap;
    next_[key] = static_cast<std::uint8_t>(seq + 1);
    return gap;
}

double SeqTracker::lossRatio() const {
    const std::uint64_t expected = received_ + lost_;
    return expected == 0 ? 0.0 : static_cast<double>(lost_) / static_cast<double>(expected);
}

}  // namespace gcs::mav
