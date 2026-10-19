// UdpLink'in iş parçacığı davranışı: soket worker iş parçacığında, çerçeveler
// ana iş parçacığında teslim; gerçek UDP döngü (loopback) üzerinden.

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QThread>
#include <QUdpSocket>
#include <QtTest>
#include <atomic>
#include <memory>

#include "gcs/mavlink/codec.hpp"
#include "gcs/mavlink/messages.hpp"
#include "link/UdpLink.h"

using namespace gcs;

namespace {
QByteArray heartbeatDatagram(std::uint8_t seq) {
    mav::Heartbeat hb;
    hb.type = mav::TypeQuadrotor;
    const auto b = mav::encode(hb, mav::Header{seq, 1, 1});
    return QByteArray(reinterpret_cast<const char*>(b.data()), static_cast<qsizetype>(b.size()));
}

int openLink(UdpLink& link) {
    QSignalSpy opened(&link, &UdpLink::openChanged);
    link.open(0);  // işletim sistemi boş port seçer
    if (!opened.wait(3000) || !link.isOpen()) return -1;
    return link.localPort();
}
}  // namespace

class TestUdpLink : public QObject {
    Q_OBJECT
private slots:
    void socketLivesInWorkerAndFramesArriveOnMainThread();
    void sendsToLearnedRemote();
    void countsCrcErrorsAndSequenceLoss();
    void burstUnderLoadDeliversInOrder();
    void repeatedCreateDestroyWhileTrafficFlows();
};

void TestUdpLink::socketLivesInWorkerAndFramesArriveOnMainThread() {
    UdpLink link;
    const int port = openLink(link);
    QVERIFY(port > 0);
    QVERIFY(link.workerThread() != QThread::currentThread());
    QVERIFY(link.workerThread()->isRunning());

    std::atomic<int> wrongThread{0};
    int received = 0;
    QObject ctx;  // ana iş parçacığında yaşayan bağlam nesnesi
    connect(&link, &LinkInterface::frameReceived, &ctx, [&](const mav::Frame&) {
        if (QThread::currentThread() != qApp->thread()) ++wrongThread;
        ++received;
    });

    QUdpSocket sender;
    for (int i = 0; i < 50; ++i) sender.writeDatagram(heartbeatDatagram(static_cast<std::uint8_t>(i)),
                                                      QHostAddress::LocalHost, static_cast<quint16>(port));
    QTRY_COMPARE_WITH_TIMEOUT(received, 50, 5000);
    QCOMPARE(wrongThread.load(), 0);
}

void TestUdpLink::sendsToLearnedRemote() {
    UdpLink link;
    const int port = openLink(link);
    QVERIFY(port > 0);
    QUdpSocket peer;
    QVERIFY(peer.bind(QHostAddress::LocalHost, 0));
    peer.writeDatagram(heartbeatDatagram(0), QHostAddress::LocalHost, static_cast<quint16>(port));
    QSignalSpy frames(&link, &LinkInterface::frameReceived);
    QVERIFY(frames.wait(3000));

    mav::CommandLong c;
    c.command = mav::cmd::ComponentArmDisarm;
    c.target_system = 1;
    c.param1 = 1;
    link.sendMessage(c);

    QTRY_VERIFY_WITH_TIMEOUT(peer.hasPendingDatagrams(), 3000);
    const QByteArray d = peer.receiveDatagram().data();
    mav::Parser p;
    std::optional<mav::CommandLong> got;
    p.feed(reinterpret_cast<const std::uint8_t*>(d.constData()), static_cast<std::size_t>(d.size()),
           [&](const mav::Frame& f) {
               got = mav::decode<mav::CommandLong>(f);
               QCOMPARE(int(f.sysid), int(LinkInterface::kGcsSystemId));
           });
    QVERIFY(got.has_value());
    QCOMPARE(got->command, mav::cmd::ComponentArmDisarm);
}

void TestUdpLink::countsCrcErrorsAndSequenceLoss() {
    UdpLink link;
    const int port = openLink(link);
    QVERIFY(port > 0);
    QUdpSocket sender;
    QByteArray bad = heartbeatDatagram(0);
    bad[bad.size() - 1] = static_cast<char>(bad.at(bad.size() - 1) ^ 0x5A);
    sender.writeDatagram(bad, QHostAddress::LocalHost, static_cast<quint16>(port));
    sender.writeDatagram(heartbeatDatagram(10), QHostAddress::LocalHost, static_cast<quint16>(port));
    sender.writeDatagram(heartbeatDatagram(14), QHostAddress::LocalHost, static_cast<quint16>(port));  // 3 kayıp
    QTRY_COMPARE_WITH_TIMEOUT(link.framesReceived(), quint64(2), 3000);
    QCOMPARE(link.crcErrors(), quint64(1));
    QCOMPARE(link.lostPackets(), quint64(3));
    QVERIFY(qAbs(link.lossPercent() - 60.0) < 1e-9);  // 3 / (2 + 3)
}

void TestUdpLink::burstUnderLoadDeliversInOrder() {
    UdpLink link;
    const int port = openLink(link);
    QVERIFY(port > 0);
    std::vector<int> seqs;
    connect(&link, &LinkInterface::frameReceived, this, [&](const mav::Frame& f) { seqs.push_back(f.seq); });

    // Gönderici ayrı bir iş parçacığında patlama halinde yollar; ana iş parçacığı
    // aynı anda meşgul tutulur. Kuyruklu sinyaller sırayı korumalı.
    constexpr int kCount = 2000;
    std::unique_ptr<QThread> sender(QThread::create([port] {
        QUdpSocket s;
        for (int i = 0; i < kCount; ++i) {
            s.writeDatagram(heartbeatDatagram(static_cast<std::uint8_t>(i)), QHostAddress::LocalHost,
                            static_cast<quint16>(port));
            if (i % 200 == 199) QThread::msleep(2);  // çekirdek tamponunu taşırmamak için
        }
    }));
    sender->start();
    QThread::msleep(100);  // ana iş parçacığını bloke et: çerçeveler kuyrukta birikir
    QVERIFY(sender->wait(10000));
    QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(seqs.size()), kCount, 10000);
    for (int i = 0; i < kCount; ++i) QCOMPARE(seqs[static_cast<std::size_t>(i)], i % 256);
}

void TestUdpLink::repeatedCreateDestroyWhileTrafficFlows() {
    // Yaşam döngüsü yarışları: bağlantı trafik akarken kapatılıp yeniden açılır.
    std::atomic<bool> stop{false};
    std::atomic<int> targetPort{0};
    std::unique_ptr<QThread> sender(QThread::create([&] {
        QUdpSocket s;
        std::uint8_t seq = 0;
        while (!stop.load()) {
            const int p = targetPort.load();
            if (p > 0) s.writeDatagram(heartbeatDatagram(seq++), QHostAddress::LocalHost, static_cast<quint16>(p));
            QThread::usleep(200);
        }
    }));
    sender->start();
    for (int round = 0; round < 15; ++round) {
        int received = 0;
        // Tur başına bağlam nesnesi: tur bitince kuyrukta kalan olaylar da onunla silinir.
        QObject ctx;
        auto link = std::make_unique<UdpLink>();
        connect(link.get(), &LinkInterface::frameReceived, &ctx, [&](const mav::Frame&) { ++received; });
        const int port = openLink(*link);
        QVERIFY(port > 0);
        targetPort.store(port);
        QTRY_VERIFY_WITH_TIMEOUT(received > 5, 3000);
        link.reset();  // yıkıcı worker'ı kapatıp iş parçacığını beklemeli
    }
    stop.store(true);
    QVERIFY(sender->wait(5000));
}

QTEST_GUILESS_MAIN(TestUdpLink)
#include "tst_udplink.moc"
