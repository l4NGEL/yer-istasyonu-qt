#include <QSignalSpy>
#include <QtTest>
#include <cmath>

#include "FakeLink.h"
#include "gcs/geo.hpp"
#include "payload/PayloadController.h"
#include "vehicle/Vehicle.h"

using namespace gcs;

namespace {
constexpr double kPi = 3.14159265358979323846;
}

class TestVehiclePayload : public QObject {
    Q_OBJECT
private slots:
    void heartbeatConnectsAndLocksSystemId();
    void ignoresOtherGcsHeartbeats();
    void heartbeatTimeoutDisconnects();
    void decodesTelemetry();
    void armSendsCommandAndReportsAck();
    void commandTimesOutWithoutAck();
    void gimbalCommandsAreCoalesced();
    void gimbalAttitudeFromQuaternion();
    void photoCountFollowsAcceptedAcks();
};

void TestVehiclePayload::heartbeatConnectsAndLocksSystemId() {
    FakeLink link;
    Vehicle v(&link);
    QSignalSpy conn(&v, &Vehicle::connectionChanged);
    link.injectHeartbeat(true, 1, 7);
    QVERIFY(v.connected());
    QCOMPARE(v.systemId(), 7);
    QVERIFY(v.armed());
    QCOMPARE(v.modeName(), QStringLiteral("GÖREV"));
    QCOMPARE(conn.count(), 1);
    // Başka sistemin telemetrisi yok sayılır
    mav::GlobalPositionInt p;
    p.lat = 100000000;
    link.inject(p, 9);
    QVERIFY(!v.positionValid());
}

void TestVehiclePayload::ignoresOtherGcsHeartbeats() {
    FakeLink link;
    Vehicle v(&link);
    mav::Heartbeat gcsHb;
    gcsHb.type = mav::TypeGcs;
    gcsHb.autopilot = mav::AutopilotInvalid;
    link.inject(gcsHb, 250);
    QVERIFY(!v.connected());
    QCOMPARE(v.systemId(), 0);
}

void TestVehiclePayload::heartbeatTimeoutDisconnects() {
    FakeLink link;
    Vehicle v(&link);
    v.setHeartbeatTimeoutMs(150);
    link.injectHeartbeat();
    QVERIFY(v.connected());
    QTRY_VERIFY_WITH_TIMEOUT(!v.connected(), 2000);
    QVERIFY(v.statusLog().first().contains(QStringLiteral("zaman aşımı")));
}

void TestVehiclePayload::decodesTelemetry() {
    FakeLink link;
    Vehicle v(&link);
    link.injectHeartbeat();
    mav::GlobalPositionInt p;
    p.lat = 398890000;
    p.lon = 327785000;
    p.alt = 975500;
    p.relative_alt = 25500;
    p.hdg = 9050;
    link.inject(p);
    QVERIFY(v.positionValid());
    QCOMPARE(v.latitude(), 39.889);
    QCOMPARE(v.relativeAltitude(), 25.5);
    QCOMPARE(v.heading(), 90.5);
    QVERIFY(v.homeValid());  // ilk konum ev olarak alınır

    mav::Attitude a;
    a.roll = static_cast<float>(10.0 * kPi / 180.0);
    a.pitch = static_cast<float>(-5.0 * kPi / 180.0);
    link.inject(a);
    QVERIFY(qAbs(v.roll() - 10.0) < 1e-4);
    QVERIFY(qAbs(v.pitch() + 5.0) < 1e-4);

    mav::SysStatus s;
    s.voltage_battery = 15800;
    s.battery_remaining = 64;
    link.inject(s);
    QCOMPARE(v.batteryVoltage(), 15.8);
    QCOMPARE(v.batteryRemaining(), 64);

    mav::StatusText t;
    t.setText("Fotograf #1 kaydedildi");
    link.inject(t);
    QVERIFY(v.statusLog().first().endsWith(QStringLiteral("Fotograf #1 kaydedildi")));
}

void TestVehiclePayload::armSendsCommandAndReportsAck() {
    FakeLink link;
    Vehicle v(&link);
    link.injectHeartbeat();
    QSignalSpy done(&v, &Vehicle::commandFinished);
    v.arm();
    const auto sent = link.sentOf<mav::CommandLong>();
    QCOMPARE(sent.size(), size_t(1));
    QCOMPARE(sent[0].command, mav::cmd::ComponentArmDisarm);
    QCOMPARE(sent[0].param1, 1.f);
    QCOMPARE(int(sent[0].target_system), 1);

    mav::CommandAck ack;
    ack.command = mav::cmd::ComponentArmDisarm;
    ack.result = mav::result::Accepted;
    link.inject(ack);
    QCOMPARE(done.count(), 1);
    QCOMPARE(done.at(0).at(1).toBool(), true);
}

void TestVehiclePayload::commandTimesOutWithoutAck() {
    FakeLink link;
    Vehicle v(&link);
    v.setCommandTimeoutMs(100);
    link.injectHeartbeat();
    QSignalSpy done(&v, &Vehicle::commandFinished);
    v.startMission();
    QVERIFY(done.wait(3000));
    QCOMPARE(done.at(0).at(1).toBool(), false);
    QCOMPARE(link.sentOf<mav::CommandLong>().size(), size_t(3));  // ilk + 2 yeniden deneme
    QVERIFY(v.lastCommandResult().contains(QStringLiteral("yanıt yok")));
}

void TestVehiclePayload::gimbalCommandsAreCoalesced() {
    FakeLink link;
    Vehicle v(&link);
    PayloadController payload(&link, &v);
    link.injectHeartbeat();
    // Kaydırıcı sürükleme: 30 ara değer, gönderim zamanlayıcısı tetiklenmeden
    for (int i = 0; i <= 30; ++i) payload.setGimbal(-i, i / 2.0);
    QCOMPARE(link.sentOf<mav::CommandLong>().size(), size_t(0));
    QTRY_COMPARE_WITH_TIMEOUT(link.sentOf<mav::CommandLong>().size(), size_t(1), 2000);
    const auto c = link.sentOf<mav::CommandLong>().front();
    QCOMPARE(c.command, mav::cmd::DoGimbalManagerPitchYaw);
    QCOMPARE(c.param1, -30.f);
    QCOMPARE(c.param2, 15.f);
    QVERIFY(std::isnan(c.param3) && std::isnan(c.param4));
    // Değişiklik yoksa tekrar gönderilmez
    QTest::qWait(300);
    QCOMPARE(link.sentOf<mav::CommandLong>().size(), size_t(1));
    // Sınır dışı istek kırpılır
    payload.setGimbal(-120, 400);
    QCOMPARE(payload.targetPitch(), -90.0);
    QCOMPARE(payload.targetYaw(), 180.0);
}

void TestVehiclePayload::gimbalAttitudeFromQuaternion() {
    FakeLink link;
    Vehicle v(&link);
    PayloadController payload(&link, &v);
    link.injectHeartbeat();
    mav::GimbalDeviceAttitudeStatus g;
    const auto q = geo::quatFromEuler({0, -35, 20});
    g.q = {float(q.w), float(q.x), float(q.y), float(q.z)};
    link.inject(g, 1, mav::CompIdGimbal);
    QVERIFY(payload.gimbalReporting());
    QVERIFY(qAbs(payload.gimbalPitch() + 35) < 1e-3);
    QVERIFY(qAbs(payload.gimbalYaw() - 20) < 1e-3);
}

void TestVehiclePayload::photoCountFollowsAcceptedAcks() {
    FakeLink link;
    Vehicle v(&link);
    PayloadController payload(&link, &v);
    link.injectHeartbeat();
    payload.takePhoto();
    const auto c = link.sentOf<mav::CommandLong>().back();
    QCOMPARE(c.command, mav::cmd::ImageStartCapture);
    QCOMPARE(int(c.target_component), int(mav::CompIdCamera));
    mav::CommandAck ack;
    ack.command = mav::cmd::ImageStartCapture;
    ack.result = mav::result::Denied;
    link.inject(ack);
    QCOMPARE(payload.photoCount(), 0);
    payload.takePhoto();
    ack.result = mav::result::Accepted;
    link.inject(ack);
    QCOMPARE(payload.photoCount(), 1);
}

QTEST_GUILESS_MAIN(TestVehiclePayload)
#include "tst_vehicle_payload.moc"
