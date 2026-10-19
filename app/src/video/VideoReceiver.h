#pragma once
// Video akışını ayrı bir iş parçacığında GStreamer ile alıp çözer.
//
// İş parçacığı modeli:
//   video iş parçacığı : appsink'ten kare çeker → QImage'a kopyalar →
//                        LatestMailbox'a koyar; kutu boşsa frameReady yayar.
//   ana iş parçacığı   : frameReady (kuyruklu) → takeFrame() → VideoItem çizer.
// Arayüz yavaşlarsa eski kare ezilir (overwritten sayılır), kuyruk büyümez ve
// gecikme birikmez. Sayaçlar atomik; durum metni kuyruklu sinyalle taşınır.

#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <optional>

#include "gcs/latest_mailbox.hpp"

class VideoReceiver;

class VideoWorker : public QThread {
    Q_OBJECT
public:
    VideoWorker(VideoReceiver* owner, QString pipeline);
    void requestStop() { stop_.store(true); }

signals:
    void statusText(const QString& text, bool streaming);

protected:
    void run() override;

private:
    VideoReceiver* owner_;
    QString pipeline_;
    std::atomic<bool> stop_{false};
};

class VideoReceiver : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString uri READ uri WRITE setUri NOTIFY uriChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double fps READ fps NOTIFY statsChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY statsChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY statsChanged)
    Q_PROPERTY(quint64 framesDecoded READ framesDecoded NOTIFY statsChanged)
    Q_PROPERTY(quint64 framesDropped READ framesDropped NOTIFY statsChanged)

public:
    explicit VideoReceiver(QObject* parent = nullptr);
    ~VideoReceiver() override;

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

    // Ana iş parçacığından çağrılır; yeni kare yoksa boş döner.
    std::optional<QImage> takeFrame();

    QString uri() const { return uri_; }
    void setUri(const QString& uri);
    bool running() const { return worker_ != nullptr; }
    bool streaming() const { return streaming_; }
    QString status() const { return status_; }
    double fps() const { return fps_; }
    int frameWidth() const { return width_.load(); }
    int frameHeight() const { return height_.load(); }
    quint64 framesDecoded() const { return decoded_.load(); }
    quint64 framesDropped() const { return mailbox_.stats().overwritten; }
    gcs::MailboxStats mailboxStats() const { return mailbox_.stats(); }
    Qt::HANDLE workerThreadId() const { return workerThreadId_.load(); }

    static bool gstreamerAvailable();

signals:
    void uriChanged();
    void runningChanged();
    void statusChanged();
    void statsChanged();
    void frameReady();

private:
    friend class VideoWorker;
    // Video iş parçacığından çağrılır.
    void deliverFrame(QImage&& img);

    QString uri_ = QStringLiteral("udp://5600");
    VideoWorker* worker_ = nullptr;
    gcs::LatestMailbox<QImage> mailbox_;
    std::atomic<quint64> decoded_{0};
    std::atomic<int> width_{0}, height_{0};
    std::atomic<Qt::HANDLE> workerThreadId_{nullptr};
    QTimer statsTimer_;
    quint64 lastDecoded_ = 0;
    qint64 lastStatsMs_ = 0;
    double fps_ = 0;
    bool streaming_ = false;
    QString status_ = QStringLiteral("Durduruldu");
};
