#pragma once
// VideoReceiver'ın son karesini en-boy oranını koruyarak çizen QML öğesi.

#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>

#include "VideoReceiver.h"

class VideoItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(VideoReceiver* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)

public:
    explicit VideoItem(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    VideoReceiver* source() const { return source_; }
    void setSource(VideoReceiver* source);
    bool hasFrame() const { return !frame_.isNull(); }

signals:
    void sourceChanged();
    void hasFrameChanged();

private:
    void onFrameReady();

    QPointer<VideoReceiver> source_;
    QImage frame_;
};
