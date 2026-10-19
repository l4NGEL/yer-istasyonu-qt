#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <functional>
#include <memory>

#include "gcs/mavlink/messages.hpp"
#include "link/UdpLink.h"
#include "mission/MissionManager.h"
#include "mission/MissionModel.h"
#include "payload/PayloadController.h"
#include "vehicle/Vehicle.h"
#include "video/VideoItem.h"
#include "video/VideoReceiver.h"

namespace {

// --screenshot ile verilen sekmeler sırayla açılıp pencere görüntüsü kaydedilir.
void scheduleScreenshots(QQmlApplicationEngine& engine, const QString& pattern, int delayMs,
                         const QList<int>& tabs) {
    auto index = std::make_shared<int>(0);
    auto shoot = std::make_shared<std::function<void()>>();
    *shoot = [&engine, pattern, tabs, index, shoot] {
        auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
        if (win == nullptr) {
            qCritical("Ekran görüntüsü: pencere yok");
            QCoreApplication::exit(1);
            return;
        }
        if (*index >= tabs.size()) {
            QCoreApplication::quit();
            return;
        }
        win->setProperty("currentTab", tabs[*index]);
        QTimer::singleShot(800, win, [win, pattern, tabs, index, shoot] {
            const QString path = pattern.contains(QStringLiteral("%1"))
                                     ? pattern.arg(tabs[*index])
                                     : pattern;
            const bool ok = win->grabWindow().save(path);
            qInfo("Ekran görüntüsü %s: %s", ok ? "kaydedildi" : "KAYDEDİLEMEDİ", qPrintable(path));
            ++*index;
            (*shoot)();
        });
    };
    QTimer::singleShot(delayMs, *shoot);
}

}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("yer-istasyonu-qt"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("Qt 6 / QML MAVLink yer kontrol istasyonu"));
    cli.addHelpOption();
    QCommandLineOption portOpt(QStringLiteral("port"), QStringLiteral("MAVLink UDP dinleme portu"),
                               QStringLiteral("port"), QStringLiteral("14550"));
    QCommandLineOption remoteOpt(QStringLiteral("remote"),
                                 QStringLiteral("Sabit karşı uç host:port (boşsa ilk paketten öğrenilir)"),
                                 QStringLiteral("host:port"));
    QCommandLineOption videoOpt(QStringLiteral("video"),
                                QStringLiteral("Video kaynağı: udp://5600, rtsp://…, test://"),
                                QStringLiteral("uri"), QStringLiteral("udp://5600"));
    QCommandLineOption missionOpt(QStringLiteral("mission"), QStringLiteral("Açılışta yüklenecek .plan"),
                                  QStringLiteral("dosya"));
    QCommandLineOption demoOpt(QStringLiteral("demo"),
                               QStringLiteral("Bağlanınca görevi yükle, arm et, başlat (gösterim için)"));
    QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                               QStringLiteral("Pencere görüntüsünü kaydedip çık (%1 = sekme)"),
                               QStringLiteral("png"));
    QCommandLineOption shotDelayOpt(QStringLiteral("screenshot-delay"),
                                    QStringLiteral("İlk görüntüden önce bekleme (ms)"), QStringLiteral("ms"),
                                    QStringLiteral("15000"));
    QCommandLineOption shotTabsOpt(QStringLiteral("screenshot-tabs"),
                                   QStringLiteral("Görüntüsü alınacak sekmeler, ör. 0,1,2"),
                                   QStringLiteral("liste"), QStringLiteral("0"));
    cli.addOptions({portOpt, remoteOpt, videoOpt, missionOpt, demoOpt, shotOpt, shotDelayOpt, shotTabsOpt});
    cli.process(app);

    UdpLink link;
    Vehicle vehicle(&link);
    MissionModel mission;
    MissionManager missionManager(&link, &vehicle, &mission);
    PayloadController payload(&link, &vehicle);
    VideoReceiver video;

    QString remoteHost;
    int remotePort = 0;
    if (cli.isSet(remoteOpt)) {
        const QString r = cli.value(remoteOpt);
        remoteHost = r.section(QLatin1Char(':'), 0, 0);
        remotePort = r.section(QLatin1Char(':'), 1, 1).toInt();
    }
    link.open(cli.value(portOpt).toInt(), remoteHost, remotePort);

    if (cli.isSet(missionOpt) && !mission.load(cli.value(missionOpt))) {
        qWarning("Görev dosyası yüklenemedi: %s", qPrintable(mission.lastError()));
    }
    video.setUri(cli.value(videoOpt));
    video.start();

    if (cli.isSet(demoOpt)) {
        auto uploaded = std::make_shared<bool>(false);
        QObject::connect(&vehicle, &Vehicle::connectionChanged, &app, [&, uploaded] {
            if (vehicle.connected() && !*uploaded && mission.count() > 0) {
                *uploaded = true;
                QTimer::singleShot(500, &missionManager, &MissionManager::upload);
            }
        });
        QObject::connect(&missionManager, &MissionManager::uploadFinished, &app,
                         [&](bool ok, const QString&) { if (ok) vehicle.arm(); });
        // ARM'ın onayı (COMMAND_ACK) HEARTBEAT'teki ARMED bayrağından önce gelebilir;
        // görev, araç kendini ARMED bildirdiğinde bir kez başlatılır.
        auto started = std::make_shared<bool>(false);
        QObject::connect(&vehicle, &Vehicle::statusChanged, &app, [&, started] {
            if (vehicle.armed() && !*started) {
                *started = true;
                vehicle.startMission();
                payload.setGimbal(-25, 15);
                QTimer::singleShot(4000, &payload, &PayloadController::takePhoto);
            }
        });
    }

    qmlRegisterSingletonInstance("Gcs", 1, 0, "Link", &link);
    qmlRegisterSingletonInstance("Gcs", 1, 0, "Vehicle", &vehicle);
    qmlRegisterSingletonInstance("Gcs", 1, 0, "Mission", &mission);
    qmlRegisterSingletonInstance("Gcs", 1, 0, "MissionManager", &missionManager);
    qmlRegisterSingletonInstance("Gcs", 1, 0, "Payload", &payload);
    qmlRegisterSingletonInstance("Gcs", 1, 0, "Video", &video);
    qmlRegisterType<VideoItem>("Gcs", 1, 0, "VideoItem");
    qmlRegisterUncreatableType<VideoReceiver>("Gcs", 1, 0, "VideoReceiver",
                                              QStringLiteral("Video tekil nesnesini kullanın"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [](QObject* obj, const QUrl&) { if (obj == nullptr) QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    if (cli.isSet(shotOpt)) {
        QList<int> tabs;
        for (const QString& t : cli.value(shotTabsOpt).split(QLatin1Char(','), Qt::SkipEmptyParts))
            tabs << t.toInt();
        scheduleScreenshots(engine, cli.value(shotOpt), cli.value(shotDelayOpt).toInt(), tabs);
    }
    const int rc = QGuiApplication::exec();
    video.stop();
    return rc;
}
