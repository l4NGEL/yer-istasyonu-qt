#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "gcs/mavlink/codec.hpp"
#include "gcs/mavlink/crc.hpp"
#include "gcs/mavlink/messages.hpp"

using namespace gcs::mav;

namespace {

std::vector<Frame> parseAll(Parser& p, const std::vector<std::uint8_t>& bytes) {
    std::vector<Frame> frames;
    p.feed(bytes.data(), bytes.size(), [&](const Frame& f) { frames.push_back(f); });
    return frames;
}

std::vector<std::uint8_t> heartbeatBytes(std::uint8_t seq, int version = 2) {
    Heartbeat hb;
    hb.type = TypeQuadrotor;
    hb.base_mode = ModeFlagCustomModeEnabled;
    hb.custom_mode = 2;
    return encode(hb, Header{seq, 1, 1}, version);
}

}  // namespace

TEST(Crc, Mcrf4xxCheckValue) {
    // CRC-16/MCRF4XX katalog kontrol değeri: "123456789" -> 0x6F91
    const char* s = "123456789";
    EXPECT_EQ(crcCalculate(reinterpret_cast<const std::uint8_t*>(s), std::strlen(s)), 0x6F91);
}

TEST(Codec, V2TruncatesTrailingZerosButKeepsOneByte) {
    Heartbeat hb;
    hb.mavlink_version = 0;  // tüm alanlar sıfır
    auto bytes = encode(hb, Header{0, 1, 1});
    EXPECT_EQ(bytes[1], 1);  // tümü sıfır yük → 1 bayta kırpılır
    Parser p;
    auto frames = parseAll(p, bytes);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].wireLen, 1);
    EXPECT_EQ(frames[0].payload.size(), Heartbeat::LEN);  // ayrıştırıcı sıfırla tamamlar
}

TEST(Codec, V1DoesNotTruncate) {
    Heartbeat hb;
    hb.mavlink_version = 0;
    auto bytes = encode(hb, Header{0, 1, 1}, 1);
    EXPECT_EQ(bytes[0], kStxV1);
    EXPECT_EQ(bytes[1], Heartbeat::LEN);
}

TEST(Parser, ByteByByteFeeding) {
    auto bytes = heartbeatBytes(5);
    Parser p;
    int count = 0;
    for (std::uint8_t b : bytes) {
        p.feed(&b, 1, [&](const Frame& f) {
            ++count;
            EXPECT_EQ(f.seq, 5);
            auto hb = decode<Heartbeat>(f);
            ASSERT_TRUE(hb.has_value());
            EXPECT_EQ(hb->custom_mode, 2u);
        });
    }
    EXPECT_EQ(count, 1);
    EXPECT_EQ(p.stats().framesOk, 1u);
}

TEST(Parser, SkipsGarbageAndCountsDiscardedBytes) {
    std::vector<std::uint8_t> stream = {0x00, 0x11, 0x22, 0x33};
    auto hb = heartbeatBytes(1);
    stream.insert(stream.end(), hb.begin(), hb.end());
    Parser p;
    EXPECT_EQ(parseAll(p, stream).size(), 1u);
    EXPECT_EQ(p.stats().bytesDiscarded, 4u);
}

TEST(Parser, CorruptCrcIsRejectedAndNextFrameStillParsed) {
    auto bad = heartbeatBytes(1);
    bad.back() ^= 0xFF;
    auto good = heartbeatBytes(2);
    std::vector<std::uint8_t> stream = bad;
    stream.insert(stream.end(), good.begin(), good.end());
    Parser p;
    auto frames = parseAll(p, stream);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].seq, 2);
    EXPECT_EQ(p.stats().crcErrors, 1u);
}

TEST(Parser, FalseStxInsideGarbageDoesNotSwallowRealFrame) {
    // 0xFD + büyük uzunluk: gerçek çerçeveyi "yutacak" sahte başlık. Ayrıştırıcı
    // CRC tutmayınca bir bayt ilerleyip gerçek çerçeveyi bulmalı.
    std::vector<std::uint8_t> stream = {0xFD, 0x30, 0x00, 0x00};
    auto good = heartbeatBytes(9);
    stream.insert(stream.end(), good.begin(), good.end());
    stream.resize(stream.size() + 60, 0x55);  // sahte çerçevenin boyunu tamamla
    Parser p;
    auto frames = parseAll(p, stream);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].seq, 9);
}

TEST(Parser, MixedV1AndV2Stream) {
    auto a = heartbeatBytes(1, 1);
    auto b = heartbeatBytes(2, 2);
    std::vector<std::uint8_t> stream = a;
    stream.insert(stream.end(), b.begin(), b.end());
    Parser p;
    auto frames = parseAll(p, stream);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].version, 1);
    EXPECT_EQ(frames[1].version, 2);
}

TEST(Parser, UnknownMessageIdIsRejected) {
    std::uint8_t payload[4] = {1, 2, 3, 4};
    auto bytes = encodeFrame(Header{0, 1, 1}, 9999, 0, payload, sizeof(payload));
    Parser p;
    EXPECT_TRUE(parseAll(p, bytes).empty());
    EXPECT_GE(p.stats().unknownOrBad, 1u);
}

TEST(Parser, UnsupportedIncompatFlagIsRejected) {
    auto bytes = heartbeatBytes(1);
    bytes[2] = 0x02;  // tanımsız incompat bayrağı
    Parser p;
    EXPECT_TRUE(parseAll(p, bytes).empty());
}

TEST(Parser, SignedFrameAcceptedWithSignatureSkipped) {
    // İmza doğrulanmaz (anahtar yönetimi kapsam dışı), ama çerçeve sınırı doğru
    // hesaplanmalı: imzalı çerçeveden sonraki çerçeve de okunmalı.
    Heartbeat hb;
    hb.custom_mode = 7;
    std::uint8_t pl[Heartbeat::LEN];
    hb.pack(pl);
    std::vector<std::uint8_t> f = {kStxV2, Heartbeat::LEN, kIncompatSigned, 0, 3, 1, 1, 0, 0, 0};
    f.insert(f.end(), pl, pl + Heartbeat::LEN);
    std::uint16_t crc = crcCalculate(f.data() + 1, f.size() - 1);
    crcAccumulate(Heartbeat::CRC_EXTRA, crc);
    f.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    f.push_back(static_cast<std::uint8_t>(crc >> 8));
    f.resize(f.size() + 13, 0xAB);  // link id + zaman damgası + imza
    auto next = heartbeatBytes(4);
    f.insert(f.end(), next.begin(), next.end());

    Parser p;
    auto frames = parseAll(p, f);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_TRUE(frames[0].isSigned);
    EXPECT_EQ(decode<Heartbeat>(frames[0])->custom_mode, 7u);
    EXPECT_EQ(frames[1].seq, 4);
}

TEST(Parser, ExtensionFieldsZeroFilledWhenSenderIsOlder) {
    // Eski bir gönderici COMMAND_ACK'i yalnızca temel alanlarla (3 bayt) yollar.
    std::uint8_t base[3] = {0x90, 0x01, 0x00};  // command=400, result=0
    auto bytes = encodeFrame(Header{0, 1, 1}, CommandAck::ID, CommandAck::CRC_EXTRA, base, 3);
    Parser p;
    auto frames = parseAll(p, bytes);
    ASSERT_EQ(frames.size(), 1u);
    auto ack = decode<CommandAck>(frames[0]);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->command, 400);
    EXPECT_EQ(ack->target_system, 0);
}

TEST(Parser, DecodeRejectsWrongType) {
    Parser p;
    auto frames = parseAll(p, heartbeatBytes(0));
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_FALSE(decode<Attitude>(frames[0]).has_value());
}

TEST(SeqTracker, CountsGapsAndWraparound) {
    SeqTracker t;
    EXPECT_EQ(t.observe(1, 1, 250), 0u);
    EXPECT_EQ(t.observe(1, 1, 251), 0u);
    EXPECT_EQ(t.observe(1, 1, 254), 2u);  // 252, 253 kayıp
    EXPECT_EQ(t.observe(1, 1, 1), 2u);    // 255, 0 kayıp (taşma)
    EXPECT_EQ(t.lost(), 4u);
    EXPECT_EQ(t.received(), 4u);
    EXPECT_NEAR(t.lossRatio(), 0.5, 1e-9);
}

TEST(SeqTracker, ReorderIsNotCountedAsLoss) {
    SeqTracker t;
    t.observe(1, 1, 10);
    t.observe(1, 1, 11);
    EXPECT_EQ(t.observe(1, 1, 9), 0u);  // geride kalan paket
    EXPECT_EQ(t.lost(), 0u);
}

TEST(SeqTracker, SeparateStreamsPerComponent) {
    SeqTracker t;
    t.observe(1, 1, 0);
    t.observe(1, 154, 100);
    EXPECT_EQ(t.observe(1, 1, 1), 0u);
    EXPECT_EQ(t.observe(1, 154, 101), 0u);
}
