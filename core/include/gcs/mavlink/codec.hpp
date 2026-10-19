#pragma once
// MAVLink v1/v2 çerçeveleme: kodlama, akış ayrıştırıcı ve dizi numarası takibi.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace gcs::mav {

constexpr std::uint8_t kStxV1 = 0xFE;
constexpr std::uint8_t kStxV2 = 0xFD;
constexpr std::uint8_t kIncompatSigned = 0x01;
constexpr std::size_t kMaxFrameLen = 10 + 255 + 2 + 13;

struct MsgInfo {
    std::uint32_t id;
    std::uint8_t crcExtra;
    std::size_t len;  // uzantılar dahil tam yük uzunluğu
};

// Bilinen mesajlar tablosu (messages.hpp ile aynı küme). Bilinmeyen mesajın
// CRC'si doğrulanamayacağı için ayrıştırıcı onu reddeder.
const MsgInfo* findMsgInfo(std::uint32_t msgid);

struct Frame {
    std::uint8_t version = 2;
    std::uint8_t seq = 0;
    std::uint8_t sysid = 0;
    std::uint8_t compid = 0;
    std::uint32_t msgid = 0;
    bool isSigned = false;
    std::uint8_t wireLen = 0;           // kablodaki (kırpılmış olabilecek) yük uzunluğu
    std::vector<std::uint8_t> payload;  // sıfırla tam uzunluğa tamamlanmış yük
};

struct Header {
    std::uint8_t seq = 0;
    std::uint8_t sysid = 255;
    std::uint8_t compid = 190;
};

// Ham yükü çerçeveler. v2'de sondaki sıfır baytlar kırpılır (en az 1 bayt kalır),
// MAVLink C kütüphanesi ve pymavlink ile aynı davranış.
std::vector<std::uint8_t> encodeFrame(const Header& h, std::uint32_t msgid, std::uint8_t crcExtra,
                                      const std::uint8_t* payload, std::size_t len, int version = 2);

template <class M>
std::vector<std::uint8_t> encode(const M& msg, const Header& h, int version = 2) {
    std::uint8_t buf[M::LEN];
    msg.pack(buf);
    return encodeFrame(h, M::ID, M::CRC_EXTRA, buf, M::LEN, version);
}

template <class M>
std::optional<M> decode(const Frame& f) {
    if (f.msgid != M::ID || f.payload.size() < M::LEN) return std::nullopt;
    return M::unpack(f.payload.data());
}

struct ParserStats {
    std::uint64_t framesOk = 0;
    std::uint64_t crcErrors = 0;      // bilinen mesaj, yanlış CRC
    std::uint64_t unknownOrBad = 0;   // bilinmeyen msgid ya da geçersiz başlık
    std::uint64_t bytesDiscarded = 0; // STX aranırken atlanan baytlar
};

// Bayt akışından çerçeve çıkarır. Bozuk bir aday çerçevede tamponu atmaz,
// bir bayt ilerleyip yeniden STX arar; böylece çöp içindeki gerçek çerçeve kaybolmaz.
// İş parçacığı güvenli değildir: her bağlantının kendi ayrıştırıcısı olmalı.
// Geri çağrının içinden aynı ayrıştırıcıya feed() çağrılmamalıdır.
class Parser {
public:
    using Callback = std::function<void(const Frame&)>;

    void feed(const std::uint8_t* data, std::size_t len, const Callback& onFrame);
    const ParserStats& stats() const { return stats_; }
    void reset();

private:
    std::vector<std::uint8_t> buf_;
    ParserStats stats_;
};

// (sysid, compid) başına dizi numarası boşluğundan paket kaybı tahmini.
class SeqTracker {
public:
    // Bu çerçeveden önce kaç paketin kaybolduğunu döndürür.
    std::uint32_t observe(std::uint8_t sysid, std::uint8_t compid, std::uint8_t seq);
    std::uint64_t received() const { return received_; }
    std::uint64_t lost() const { return lost_; }
    double lossRatio() const;

private:
    std::unordered_map<std::uint16_t, std::uint8_t> next_;
    std::uint64_t received_ = 0;
    std::uint64_t lost_ = 0;
};

}  // namespace gcs::mav
