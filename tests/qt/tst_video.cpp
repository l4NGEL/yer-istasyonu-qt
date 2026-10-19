// Video alım iş parçacığı: boru hattı üretimi, ayrı iş parçacığında kod çözme,
// yavaş tüketicide kare düşürme muhasebesi, tekrarlı başlat/durdur ve gerçek
// RTP/H.264 UDP akışının çözülmesi.

#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include "video/PipelineBuilder.h"
#include "video/VideoReceiver.h"

#ifdef GCS_HAVE_GSTREAMER
#include <gst/gst.h>
#endif

class TestVideo : public QObject {
    Q_OBJECT
private slots:
    void pipelineFromUri_data();
    void pipelineFromUri();
    void decodesOnWorkerThread();
    void slowConsumerDropsFramesWithoutQueueGrowth();
    void repeatedStartStop();
    void decodesRtpH264OverUdp();
};

void TestVideo::pipelineFromUri_data() {
    QTest::addColumn<QString>("uri");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("contains");
    QTest::newRow("udp port") << "udp://5600" << true << "udpsrc address=0.0.0.0 port=5600";
    QTest::newRow("udp adres") << "udp://239.0.0.1:5000" << true << "address=239.0.0.1 port=5000";
    QTest::newRow("rtsp") << "rtsp://10.0.0.5:8554/cam" << true << "rtspsrc location=rtsp://10.0.0.5:8554/cam";
    QTest::newRow("test") << "test://" << true << "videotestsrc";
    QTest::newRow("port sınır dışı") << "udp://70000" << false << "";
    QTest::newRow("port değil") << "udp://abc" << false << "";
    QTest::newRow("enjeksiyon") << "rtsp://x ! filesink location=/tmp/a" << false << "";
    QTest::newRow("tırnak") << "rtsp://x\"y" << false << "";
    QTest::newRow("bilinmeyen şema") << "http://x" << false << "";
}

void TestVideo::pipelineFromUri() {
    QFETCH(QString, uri);
    QFETCH(bool, valid);
    QFETCH(QString, contains);
    const auto spec = video::buildPipeline(uri.toStdString());
    QCOMPARE(spec.has_value(), valid);
    if (valid) {
        QVERIFY(QString::fromStdString(spec->description).contains(contains));
        QVERIFY(QString::fromStdString(spec->description).endsWith(
            QStringLiteral("appsink name=sink max-buffers=1 drop=true sync=false emit-signals=false")));
    }
}

void TestVideo::decodesOnWorkerThread() {
    if (!VideoReceiver::gstreamerAvailable()) QSKIP("GStreamer olmadan derlendi");
    VideoReceiver rx;
    rx.setUri(QStringLiteral("test://"));
    int frames = 0;
    QSize size;
    connect(&rx, &VideoReceiver::frameReady, this, [&] {
        QCOMPARE(QThread::currentThread(), qApp->thread());  // teslim ana iş parçacığında
        if (auto f = rx.takeFrame()) {
            ++frames;
            size = f->size();
        }
    });
    QVERIFY(rx.start());
    QTRY_VERIFY_WITH_TIMEOUT(frames >= 15, 10000);
    QVERIFY(rx.workerThreadId() != nullptr);
    QVERIFY(rx.workerThreadId() != QThread::currentThreadId());  // çözme ayrı iş parçacığında
    QCOMPARE(size, QSize(640, 360));
    QTRY_VERIFY_WITH_TIMEOUT(rx.streaming(), 3000);
    rx.stop();
    QVERIFY(!rx.running());
}

void TestVideo::slowConsumerDropsFramesWithoutQueueGrowth() {
    if (!VideoReceiver::gstreamerAvailable()) QSKIP("GStreamer olmadan derlendi");
    VideoReceiver rx;
    rx.setUri(QStringLiteral("test://"));
    QSignalSpy ready(&rx, &VideoReceiver::frameReady);
    QVERIFY(rx.start());
    QTRY_VERIFY_WITH_TIMEOUT(rx.framesDecoded() > 0, 10000);
    // Ana iş parçacığı 600 ms bloke: 30 fps kaynakta ~18 kare gelir, kutuda yalnız 1 kalır.
    QThread::msleep(600);
    const auto s = rx.mailboxStats();
    QVERIFY2(s.overwritten >= 5, qPrintable(QString::number(s.overwritten)));
    QCoreApplication::processEvents();
    // Bloke kalınan sürede birikmiş "kare hazır" olayı en fazla bir tane olmalı
    // (kutu boş→dolu geçişinde uyarılır); kuyruk kare sayısıyla büyümez.
    QVERIFY2(ready.count() <= 3, qPrintable(QString::number(ready.count())));
    rx.stop();
    const auto end = rx.mailboxStats();
    QCOMPARE(end.published, end.taken + end.overwritten + (rx.takeFrame() ? 1u : 0u));
}

void TestVideo::repeatedStartStop() {
    if (!VideoReceiver::gstreamerAvailable()) QSKIP("GStreamer olmadan derlendi");
    VideoReceiver rx;
    rx.setUri(QStringLiteral("test://"));
    for (int i = 0; i < 6; ++i) {
        QElapsedTimer t;
        t.start();
        QVERIFY(rx.start());
        QTest::qWait(i % 2 ? 50 : 300);
        rx.stop();
        QVERIFY2(t.elapsed() < 5000, "durdurma takıldı");
        QCOMPARE(rx.status(), QStringLiteral("Durduruldu"));
    }
}

void TestVideo::decodesRtpH264OverUdp() {
#ifndef GCS_HAVE_GSTREAMER
    QSKIP("GStreamer olmadan derlendi");
#else
    gst_init(nullptr, nullptr);
    GstElementFactory* enc = gst_element_factory_find("x264enc");
    if (enc == nullptr) QSKIP("x264enc yok (gstreamer1.0-plugins-ugly)");
    gst_object_unref(enc);

    constexpr int kPort = 25600;
    VideoReceiver rx;
    rx.setUri(QStringLiteral("udp://127.0.0.1:%1").arg(kPort));
    int frames = 0;
    QSize size;
    connect(&rx, &VideoReceiver::frameReady, this, [&] {
        if (auto f = rx.takeFrame()) { ++frames; size = f->size(); }
    });
    QVERIFY(rx.start());

    // SITL video betiğiyle aynı gönderici boru hattı
    GError* err = nullptr;
    GstElement* sender = gst_parse_launch(
        "videotestsrc is-live=true pattern=smpte num-buffers=200 ! video/x-raw,width=320,height=240,framerate=25/1"
        " ! x264enc tune=zerolatency speed-preset=ultrafast key-int-max=25 ! rtph264pay config-interval=1 pt=96"
        " ! udpsink host=127.0.0.1 port=25600",
        &err);
    QVERIFY2(err == nullptr && sender != nullptr, err ? err->message : "boru hattı yok");
    gst_element_set_state(sender, GST_STATE_PLAYING);
    QTRY_VERIFY_WITH_TIMEOUT(frames >= 20, 15000);
    QCOMPARE(size, QSize(320, 240));
    QVERIFY(rx.streaming());
    gst_element_set_state(sender, GST_STATE_NULL);
    gst_object_unref(sender);
    rx.stop();
#endif
}

QTEST_GUILESS_MAIN(TestVideo)
#include "tst_video.moc"
