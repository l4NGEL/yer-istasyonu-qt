#include "VideoReceiver.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <mutex>

#include "PipelineBuilder.h"

#ifdef GCS_HAVE_GSTREAMER
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#endif

namespace {
#ifdef GCS_HAVE_GSTREAMER
void ensureGstInit() {
    static std::once_flag once;
    std::call_once(once, [] { gst_init(nullptr, nullptr); });
}
#endif
}  // namespace

VideoWorker::VideoWorker(VideoReceiver* owner, QString pipeline)
    : owner_(owner), pipeline_(std::move(pipeline)) {
    setObjectName(QStringLiteral("video-rx"));
}

void VideoWorker::run() {
    owner_->workerThreadId_.store(QThread::currentThreadId());
#ifdef GCS_HAVE_GSTREAMER
    auto sleepInterruptible = [this](int ms) {
        for (int waited = 0; waited < ms && !stop_.load(); waited += 50) QThread::msleep(50);
    };
    const QByteArray desc = pipeline_.toUtf8();

    while (!stop_.load()) {
        GError* err = nullptr;
        GstElement* pipeline = gst_parse_launch(desc.constData(), &err);
        if (err != nullptr || pipeline == nullptr) {
            emit statusText(QStringLiteral("Boru hattı kurulamadı: %1")
                                .arg(QString::fromUtf8(err ? err->message : "bilinmeyen hata")),
                            false);
            if (err) g_error_free(err);
            if (pipeline) gst_object_unref(pipeline);
            sleepInterruptible(2000);
            continue;
        }
        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
        GstBus* bus = gst_element_get_bus(pipeline);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        emit statusText(QStringLiteral("Akış bekleniyor…"), false);

        bool streaming = false;
        bool restart = false;
        QElapsedTimer sinceFrame;
        sinceFrame.start();

        while (!stop_.load() && !restart) {
            // Zaman aşımlı çekme: durdurma isteğine en geç 100 ms'de yanıt verilir.
            GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 100 * GST_MSECOND);
            if (sample != nullptr) {
                GstVideoInfo info;
                GstVideoFrame vf;
                if (gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) &&
                    gst_video_frame_map(&vf, &info, gst_sample_get_buffer(sample), GST_MAP_READ)) {
                    const QImage view(static_cast<const uchar*>(GST_VIDEO_FRAME_PLANE_DATA(&vf, 0)),
                                      GST_VIDEO_FRAME_WIDTH(&vf), GST_VIDEO_FRAME_HEIGHT(&vf),
                                      GST_VIDEO_FRAME_PLANE_STRIDE(&vf, 0), QImage::Format_RGBX8888);
                    // Derin kopya: GStreamer tamponu birazdan geri verilecek; QImage
                    // ana iş parçacığına bağımsız bir veriyle geçmeli.
                    owner_->deliverFrame(view.copy());
                    gst_video_frame_unmap(&vf);
                }
                gst_sample_unref(sample);
                if (!streaming) {
                    streaming = true;
                    emit statusText(QStringLiteral("Akış alınıyor"), true);
                }
                sinceFrame.restart();
            } else if (streaming && sinceFrame.elapsed() > 2000) {
                streaming = false;
                emit statusText(QStringLiteral("Sinyal yok: 2 sn'dir kare gelmedi"), false);
            }

            while (GstMessage* msg = gst_bus_pop_filtered(
                       bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS))) {
                if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                    GError* e = nullptr;
                    gst_message_parse_error(msg, &e, nullptr);
                    emit statusText(QStringLiteral("Hata: %1, yeniden deneniyor")
                                        .arg(QString::fromUtf8(e ? e->message : "?")),
                                    false);
                    if (e) g_error_free(e);
                } else {
                    emit statusText(QStringLiteral("Akış bitti, yeniden bağlanılıyor"), false);
                }
                restart = true;
                gst_message_unref(msg);
            }
        }

        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(bus);
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        if (restart) sleepInterruptible(1000);
    }
    emit statusText(QStringLiteral("Durduruldu"), false);
#else
    emit statusText(QStringLiteral("GStreamer desteği olmadan derlendi"), false);
#endif
}

// ---------------------------------------------------------------------------

VideoReceiver::VideoReceiver(QObject* parent) : QObject(parent) {
    statsTimer_.setInterval(500);
    connect(&statsTimer_, &QTimer::timeout, this, [this] {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const quint64 d = decoded_.load();
        if (lastStatsMs_ > 0 && now > lastStatsMs_) {
            fps_ = static_cast<double>(d - lastDecoded_) * 1000.0 / static_cast<double>(now - lastStatsMs_);
        }
        lastDecoded_ = d;
        lastStatsMs_ = now;
        emit statsChanged();
    });
}

VideoReceiver::~VideoReceiver() { stop(); }

bool VideoReceiver::gstreamerAvailable() {
#ifdef GCS_HAVE_GSTREAMER
    return true;
#else
    return false;
#endif
}

void VideoReceiver::setUri(const QString& uri) {
    if (uri == uri_) return;
    const bool wasRunning = running();
    stop();
    uri_ = uri;
    emit uriChanged();
    if (wasRunning) start();
}

bool VideoReceiver::start() {
    if (worker_ != nullptr) return true;
    const auto spec = video::buildPipeline(uri_.toStdString());
    if (!spec) {
        status_ = QStringLiteral("Geçersiz video adresi: %1").arg(uri_);
        emit statusChanged();
        return false;
    }
#ifdef GCS_HAVE_GSTREAMER
    ensureGstInit();
#endif
    worker_ = new VideoWorker(this, QString::fromStdString(spec->description));
    // Worker nesnesi ana iş parçacığında yaşar; sinyali run() içinden yayıldığı
    // için bağlantı kuyrukludur ve lambda ana iş parçacığında çalışır.
    connect(worker_, &VideoWorker::statusText, this, [this](const QString& text, bool streaming) {
        status_ = text;
        streaming_ = streaming;
        emit statusChanged();
    });
    worker_->start();
    lastDecoded_ = decoded_.load();
    lastStatsMs_ = 0;
    statsTimer_.start();
    emit runningChanged();
    return true;
}

void VideoReceiver::stop() {
    if (worker_ == nullptr) return;
    worker_->requestStop();
    worker_->wait();
    // Kuyrukta kalan statusText olayları bundan sonra teslim edilebilir; sıra
    // korunduğu için sonuncusu run()'ın son yaydığı "Durduruldu" olur.
    delete worker_;
    worker_ = nullptr;
    statsTimer_.stop();
    fps_ = 0;
    streaming_ = false;
    status_ = QStringLiteral("Durduruldu");
    emit statusChanged();
    emit statsChanged();
    emit runningChanged();
}

std::optional<QImage> VideoReceiver::takeFrame() { return mailbox_.take(); }

void VideoReceiver::deliverFrame(QImage&& img) {
    decoded_.fetch_add(1);
    width_.store(img.width());
    height_.store(img.height());
    if (mailbox_.publish(std::move(img))) emit frameReady();
}
