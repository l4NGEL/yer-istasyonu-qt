#include "VideoItem.h"

#include <QPainter>

VideoItem::VideoItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setOpaquePainting(true);
    setFillColor(Qt::black);
}

void VideoItem::setSource(VideoReceiver* source) {
    if (source_ == source) return;
    if (source_) disconnect(source_, nullptr, this, nullptr);
    source_ = source;
    if (source_) connect(source_, &VideoReceiver::frameReady, this, &VideoItem::onFrameReady);
    emit sourceChanged();
}

void VideoItem::onFrameReady() {
    if (!source_) return;
    auto img = source_->takeFrame();
    if (!img) return;
    const bool first = frame_.isNull();
    frame_ = std::move(*img);
    if (first) emit hasFrameChanged();
    update();
}

void VideoItem::paint(QPainter* painter) {
    if (frame_.isNull()) return;
    const QSizeF target = QSizeF(frame_.size()).scaled(size(), Qt::KeepAspectRatio);
    const QRectF r((width() - target.width()) / 2, (height() - target.height()) / 2, target.width(),
                   target.height());
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(r, frame_);
}
