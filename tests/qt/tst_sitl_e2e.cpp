// Uçtan uca: gcs-sitl ayrı bir süreç olarak çalışır, yer istasyonu nesneleri
// gerçek UDP soketleri üzerinden bağlanır. Görev yükleme (kayıplı bağlantı
// dahil), arm, görev uçuşu ve faydalı yük komutları sınanır.

#include <QProcess>
#include <QSignalSpy>
#include <QtTest>
#include <memory>

#include "link/UdpLink.h"
#include "mission/MissionManager.h"
#include "mission/MissionModel.h"
#include "payload/PayloadController.h"
#include "vehicle/Vehicle.h"

#ifndef GCS_SITL_PATH
#error "GCS_SITL_PATH tanımlı değil"
#endif

namespace {

struct Station {
    UdpLink link;
    Vehicle vehicle{&link};
    MissionModel mission;
    MissionManager manager{&link, &vehicle, &mission};
    PayloadController payload{&link, &vehicle};
};

std::unique_ptr<QProcess> startSitl(int gcsPort, int listenPort, double loss) {
    auto p = std::make_unique<QProcess>();
    p->setProgram(QStringLiteral(GCS_SITL_PATH));
    p->setArguments({QStringLiteral("--gcs"), QStringLiteral("127.0.0.1:%1").arg(gcsPort),
                     QStringLiteral("--listen"), QString::number(listenPort), QStringLiteral("--loss"),
                     QString::number(loss), QStringLiteral("--seed"), QStringLiteral("42")});
    p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    p->start();
    return p;
}

void fillSquare(MissionModel& m, double lat, double lon) {
    m.appendTakeoff(15);
    m.setDefaultAltitude(20);
    const double d = 0.0004;  // ~45 m
    m.appendWaypoint(lat + d, lon);
    m.appendWaypoint(lat + d, lon + d);
    m.appendWaypoint(lat, lon + d);
    m.appendReturnToLaunch();
}

}  // namespace

class TestSitlE2E : public QObject {
    Q_OBJECT
private slots:
    void uploadArmFlyOverRealUdp();
    void uploadSurvivesPacketLoss();
};

void TestSitlE2E::uploadArmFlyOverRealUdp() {
    Station gcs;
    QSignalSpy opened(&gcs.link, &UdpLink::openChanged);
    gcs.link.open(0);
    QVERIFY(opened.wait(3000));
    auto sitl = startSitl(gcs.link.localPort(), 24555, 0.0);
    QVERIFY(sitl->waitForStarted(5000));

    QTRY_VERIFY_WITH_TIMEOUT(gcs.vehicle.connected(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(gcs.vehicle.positionValid(), 3000);
    fillSquare(gcs.mission, gcs.vehicle.latitude(), gcs.vehicle.longitude());

    QSignalSpy uploaded(&gcs.manager, &MissionManager::uploadFinished);
    QVERIFY(gcs.manager.upload());
    QVERIFY(uploaded.wait(10000));
    QVERIFY2(uploaded.at(0).at(0).toBool(), qPrintable(uploaded.at(0).at(1).toString()));
    QTRY_COMPARE_WITH_TIMEOUT(gcs.vehicle.missionTotal(), 5, 3000);

    gcs.vehicle.arm();
    QTRY_VERIFY_WITH_TIMEOUT(gcs.vehicle.armed(), 3000);
    gcs.vehicle.startMission();
    QTRY_COMPARE_WITH_TIMEOUT(gcs.vehicle.modeName(), QStringLiteral("GÖREV"), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(gcs.vehicle.relativeAltitude() > 10.0, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(gcs.vehicle.missionCurrent() >= 2, 20000);

    gcs.payload.setGimbal(-40, 10);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(gcs.payload.gimbalPitch() + 40) < 0.5, 5000);
    gcs.payload.takePhoto();
    QTRY_COMPARE_WITH_TIMEOUT(gcs.payload.photoCount(), 1, 3000);

    gcs.vehicle.returnToLaunch();
    QTRY_COMPARE_WITH_TIMEOUT(gcs.vehicle.modeName(), QStringLiteral("EVE DÖNÜŞ"), 3000);

    sitl->terminate();
    QVERIFY(sitl->waitForFinished(5000));
    QCOMPARE(gcs.link.crcErrors(), quint64(0));
}

void TestSitlE2E::uploadSurvivesPacketLoss() {
    Station gcs;
    QSignalSpy opened(&gcs.link, &UdpLink::openChanged);
    gcs.link.open(0);
    QVERIFY(opened.wait(3000));
    // Her iki yönde %20 rastgele kayıp
    auto sitl = startSitl(gcs.link.localPort(), 24556, 0.2);
    QVERIFY(sitl->waitForStarted(5000));
    QTRY_VERIFY_WITH_TIMEOUT(gcs.vehicle.connected(), 8000);
    QTRY_VERIFY_WITH_TIMEOUT(gcs.vehicle.positionValid(), 5000);

    for (int i = 0; i < 15; ++i) {
        gcs.mission.appendWaypoint(gcs.vehicle.latitude() + 0.0002 * i, gcs.vehicle.longitude());
    }
    gcs.manager.setTimeoutMs(400);
    QSignalSpy uploaded(&gcs.manager, &MissionManager::uploadFinished);
    QVERIFY(gcs.manager.upload());
    QVERIFY(uploaded.wait(30000));
    QVERIFY2(uploaded.at(0).at(0).toBool(), qPrintable(uploaded.at(0).at(1).toString()));
    QVERIFY(gcs.manager.retransmissions() > 0);  // kayıp gerçekten yaşandı ve toparlandı
    QTRY_COMPARE_WITH_TIMEOUT(gcs.vehicle.missionTotal(), 15, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(gcs.link.lostPackets() > 0, 3000);

    sitl->terminate();
    QVERIFY(sitl->waitForFinished(5000));
}

QTEST_GUILESS_MAIN(TestSitlE2E)
#include "tst_sitl_e2e.moc"
