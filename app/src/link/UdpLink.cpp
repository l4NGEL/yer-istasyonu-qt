#include "UdpLink.h"

#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>
#include <QVariant>

UdpLinkWorker::UdpLinkWorker(QObject* parent) : QObject(parent) {}

void UdpLinkWorker::open(quint16 localPort, const QString& remoteHost, quint16 remotePort) {
    close();
    // Soket bu slot içinde, yani worker iş parçacığında oluşturulur; QObject
    // iş parçacığı yakınlığı (affinity) böylece doğru olur.
    socket_ = new QUdpSocket(this);
    if (!socket_->bind(QHostAddress::AnyIPv4, localPort)) {
        const QString err = socket_->errorString();
        delete socket_;
        socket_ = nullptr;
        emit opened(false, 0, err);
        return;
    }
    // Telemetri patlamalarında çekirdek tamponu taşmasın (sistem sınırına kırpılır).
    socket_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, QVariant(1 << 20));
    fixedRemote_ = !remoteHost.isEmpty() && remotePort != 0;
    remote_ = fixedRemote_ ? QHostAddress(remoteHost) : QHostAddress();
    remotePort_ = fixedRemote_ ? remotePort : 0;
    parser_.reset();
    seq_ = {};
    stats_ = {};
    connect(socket_, &QUdpSocket::readyRead, this, &UdpLinkWorker::onReadyRead);

    statsTimer_ = new QTimer(this);
    statsTimer_->setInterval(500);
    connect(statsTimer_, &QTimer::timeout, this, &UdpLinkWorker::publishStats);
    statsTimer_->start();
    emit opened(true, socket_->localPort(), {});
}

void UdpLinkWorker::close() {
    if (statsTimer_ != nullptr) {
        statsTimer_->stop();
        delete statsTimer_;
        statsTimer_ = nullptr;
    }
    if (socket_ != nullptr) {
        socket_->close();
        delete socket_;
        socket_ = nullptr;
    }
}

void UdpLinkWorker::send(const QByteArray& bytes) {
    if (socket_ == nullptr || remotePort_ == 0) return;  // karşı uç henüz bilinmiyor
    const qint64 n = socket_->writeDatagram(bytes, remote_, remotePort_);
    if (n > 0) stats_.bytesSent += static_cast<quint64>(n);
}

void UdpLinkWorker::onReadyRead() {
    while (socket_->hasPendingDatagrams()) {
        const QNetworkDatagram dg = socket_->receiveDatagram();
        if (!dg.isValid()) continue;
        if (!fixedRemote_ && (dg.senderAddress() != remote_ || dg.senderPort() != remotePort_)) {
            remote_ = dg.senderAddress();
            remotePort_ = static_cast<quint16>(dg.senderPort());
            stats_.remote = QStringLiteral("%1:%2").arg(remote_.toString()).arg(remotePort_);
        }
        const QByteArray data = dg.data();
        stats_.bytesReceived += static_cast<quint64>(data.size());
        parser_.feed(reinterpret_cast<const std::uint8_t*>(data.constData()),
                     static_cast<std::size_t>(data.size()), [this](const gcs::mav::Frame& f) {
                         seq_.observe(f.sysid, f.compid, f.seq);
                         emit frameReceived(f);
                     });
    }
}

void UdpLinkWorker::publishStats() {
    const auto& ps = parser_.stats();
    stats_.framesReceived = ps.framesOk;
    stats_.crcErrors = ps.crcErrors;
    stats_.rejected = ps.unknownOrBad;
    stats_.lostPackets = seq_.lost();
    stats_.lossRatio = seq_.lossRatio();
    if (fixedRemote_ && stats_.remote.isEmpty()) {
        stats_.remote = QStringLiteral("%1:%2").arg(remote_.toString()).arg(remotePort_);
    }
    emit statsUpdated(stats_);
}

// ---------------------------------------------------------------------------

UdpLink::UdpLink(QObject* parent) : LinkInterface(parent) {
    qRegisterMetaType<LinkStats>("LinkStats");
    worker_ = new UdpLinkWorker;  // ebeveynsiz: başka iş parçacığına taşınacak
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    // Farklı iş parçacıkları arası bağlantılar otomatik olarak kuyruklu olur.
    connect(worker_, &UdpLinkWorker::frameReceived, this, &LinkInterface::frameReceived);
    connect(worker_, &UdpLinkWorker::statsUpdated, this, [this](const LinkStats& s) {
        stats_ = s;
        emit statsChanged();
    });
    connect(worker_, &UdpLinkWorker::opened, this,
            [this](bool ok, quint16 port, const QString& err) {
                open_ = ok;
                localPort_ = port;
                error_ = err;
                emit openChanged();
            });
    thread_.setObjectName(QStringLiteral("mavlink-udp"));
    thread_.start();
}

UdpLink::~UdpLink() {
    // close() worker iş parçacığında çalışmalı; bitmesini bekleyip iş parçacığını durdur.
    QMetaObject::invokeMethod(worker_, &UdpLinkWorker::close, Qt::BlockingQueuedConnection);
    thread_.quit();
    thread_.wait();
}

void UdpLink::open(int localPort, const QString& remoteHost, int remotePort) {
    QMetaObject::invokeMethod(
        worker_,
        [w = worker_, localPort, remoteHost, remotePort] {
            w->open(static_cast<quint16>(localPort), remoteHost, static_cast<quint16>(remotePort));
        },
        Qt::QueuedConnection);
}

void UdpLink::sendBytes(const QByteArray& bytes) {
    QMetaObject::invokeMethod(
        worker_, [w = worker_, bytes] { w->send(bytes); }, Qt::QueuedConnection);
}
