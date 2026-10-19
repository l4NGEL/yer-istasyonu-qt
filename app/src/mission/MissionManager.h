#pragma once
// MissionModel'deki görevi bağlı araca MAVLink görev protokolüyle yükler.
// Durum makinesi çekirdekteki gcs::MissionUploader'dır; bu sınıf onu bağlantıya,
// QTimer'a ve QML özelliklerine bağlar.

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include "gcs/mission.hpp"
#include "link/LinkInterface.h"

class MissionModel;
class Vehicle;

class MissionManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY stateChanged)
    Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY stateChanged)
    Q_PROPERTY(int retransmissions READ retransmissions NOTIFY progressChanged)

public:
    MissionManager(LinkInterface* link, Vehicle* vehicle, MissionModel* model,
                   QObject* parent = nullptr);

    Q_INVOKABLE bool upload();
    Q_INVOKABLE void cancel();

    void setTimeoutMs(int ms) { timeoutMs_ = ms; }

    bool busy() const { return uploader_.state() == gcs::MissionUploader::State::Sending; }
    double progress() const { return uploader_.progress(); }
    QString lastResult() const { return lastResult_; }
    bool lastSucceeded() const { return lastSucceeded_; }
    int retransmissions() const { return uploader_.retransmissions(); }

signals:
    void stateChanged();
    void progressChanged();
    void uploadFinished(bool ok, const QString& message);

private:
    void handleFrame(const gcs::mav::Frame& f);

    LinkInterface* link_;
    Vehicle* vehicle_;
    MissionModel* model_;
    gcs::MissionUploader uploader_;
    QElapsedTimer clock_;
    QTimer tick_;
    int timeoutMs_ = 1500;
    QString lastResult_;
    bool lastSucceeded_ = false;
};
