#pragma once
// MAVLink UDP bağlantısı. Soket ve ayrıştırıcı ayrı bir QThread'de çalışan
// UdpLinkWorker'dadır; çözülen çerçeveler kuyruklu sinyal ile ana iş
// parçacığına geçer. Böylece yoğun telemetri ya da bozuk paket seli QML
// olay döngüsünü bloklamaz; ana iş parçacığı yalnız çözülmüş çerçeve görür.

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QThread>

#include "LinkInterface.h"

class QUdpSocket;
class QTimer;

struct LinkStats {
    quint64 framesReceived = 0;
    quint64 crcErrors = 0;
    quint64 rejected = 0;
    quint64 lostPackets = 0;
    quint64 bytesReceived = 0;
    quint64 bytesSent = 0;
    double lossRatio = 0;
    QString remote;
};
Q_DECLARE_METATYPE(LinkStats)

class UdpLinkWorker : public QObject {
    Q_OBJECT
public:
    explicit UdpLinkWorker(QObject* parent = nullptr);

public slots:
    void open(quint16 localPort, const QString& remoteHost, quint16 remotePort);
    void close();
    void send(const QByteArray& bytes);

signals:
    void opened(bool ok, quint16 boundPort, const QString& error);
    void frameReceived(const gcs::mav::Frame& frame);
    void statsUpdated(const LinkStats& stats);

private:
    void onReadyRead();
    void publishStats();

    QUdpSocket* socket_ = nullptr;
    QTimer* statsTimer_ = nullptr;
    QHostAddress remote_;
    quint16 remotePort_ = 0;
    bool fixedRemote_ = false;
    gcs::mav::Parser parser_;
    gcs::mav::SeqTracker seq_;
    LinkStats stats_;
};

class UdpLink : public LinkInterface {
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(int localPort READ localPort NOTIFY openChanged)
    Q_PROPERTY(QString error READ error NOTIFY openChanged)
    Q_PROPERTY(QString remote READ remote NOTIFY statsChanged)
    Q_PROPERTY(quint64 framesReceived READ framesReceived NOTIFY statsChanged)
    Q_PROPERTY(quint64 crcErrors READ crcErrors NOTIFY statsChanged)
    Q_PROPERTY(quint64 lostPackets READ lostPackets NOTIFY statsChanged)
    Q_PROPERTY(double lossPercent READ lossPercent NOTIFY statsChanged)

public:
    explicit UdpLink(QObject* parent = nullptr);
    ~UdpLink() override;

    // remoteHost boşsa ilk paketin kaynağı karşı uç kabul edilir (QGC'deki gibi).
    Q_INVOKABLE void open(int localPort, const QString& remoteHost = {}, int remotePort = 0);
    void sendBytes(const QByteArray& bytes) override;

    bool isOpen() const { return open_; }
    int localPort() const { return localPort_; }
    QString error() const { return error_; }
    QString remote() const { return stats_.remote; }
    quint64 framesReceived() const { return stats_.framesReceived; }
    quint64 crcErrors() const { return stats_.crcErrors; }
    quint64 lostPackets() const { return stats_.lostPackets; }
    double lossPercent() const { return stats_.lossRatio * 100.0; }
    const LinkStats& stats() const { return stats_; }

    // Testler iş parçacığı ayrımını doğrulamak için kullanır.
    const QThread* workerThread() const { return &thread_; }

signals:
    void openChanged();
    void statsChanged();

private:
    QThread thread_;
    UdpLinkWorker* worker_ = nullptr;
    bool open_ = false;
    int localPort_ = 0;
    QString error_;
    LinkStats stats_;
};
