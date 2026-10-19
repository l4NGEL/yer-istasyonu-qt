#pragma once
// Yer istasyonu nesnelerinin (Vehicle, MissionManager, PayloadController) gördüğü
// bağlantı soyutlaması. Gerçek uygulamada UdpLink, testlerde FakeLink kullanılır.
//
// frameReceived her zaman bağlantı nesnesinin yaşadığı (ana) iş parçacığında
// yayılır; dinleyiciler kilit kullanmaz. sendMessage() yalnız ana iş
// parçacığından çağrılmalıdır (dizi numarası sayacı paylaşılmaz).

#include <QByteArray>
#include <QMetaType>
#include <QObject>

#include "gcs/mavlink/codec.hpp"
#include "gcs/mavlink/messages.hpp"

Q_DECLARE_METATYPE(gcs::mav::Frame)

class LinkInterface : public QObject {
    Q_OBJECT
public:
    static constexpr std::uint8_t kGcsSystemId = 255;

    explicit LinkInterface(QObject* parent = nullptr) : QObject(parent) {
        qRegisterMetaType<gcs::mav::Frame>("gcs::mav::Frame");
    }

    template <class M>
    void sendMessage(const M& msg, std::uint8_t compid = gcs::mav::CompIdMissionPlanner) {
        const auto bytes = gcs::mav::encode(msg, gcs::mav::Header{seq_++, kGcsSystemId, compid});
        sendBytes(QByteArray(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<qsizetype>(bytes.size())));
    }

    virtual void sendBytes(const QByteArray& bytes) = 0;

signals:
    void frameReceived(const gcs::mav::Frame& frame);

private:
    std::uint8_t seq_ = 0;
};
