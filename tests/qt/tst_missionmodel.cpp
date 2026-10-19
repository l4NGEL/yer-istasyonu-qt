#include <QAbstractItemModelTester>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "mission/MissionModel.h"

class TestMissionModel : public QObject {
    Q_OBJECT
private slots:
    void editingKeepsModelConsistent();
    void moveUpdatesSequenceRole();
    void rejectsInvalidInput();
    void saveLoadRoundTrip();
    void savedFileIsQgcStyleSimpleItems();
    void loadRejectsUnsupportedContent_data();
    void loadRejectsUnsupportedContent();
};

void TestMissionModel::editingKeepsModelConsistent() {
    MissionModel m;
    // Qt'nin model denetleyicisi her sinyalde model sözleşmesini doğrular
    QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
    m.appendTakeoff(30);
    m.appendWaypoint(39.889, 32.778);
    m.appendWaypoint(39.890, 32.779);
    m.insertWaypoint(1, 39.8895, 32.7785);
    m.appendReturnToLaunch();
    QCOMPARE(m.count(), 5);
    QVERIFY(m.move(4, 0));
    QVERIFY(m.move(0, 4));
    QVERIFY(m.remove(2));
    QCOMPARE(m.count(), 4);
    m.clear();
    QCOMPARE(m.count(), 0);
}

void TestMissionModel::moveUpdatesSequenceRole() {
    MissionModel m;
    m.appendWaypoint(39.0, 32.0);
    m.appendWaypoint(39.1, 32.1);
    m.appendWaypoint(39.2, 32.2);
    QSignalSpy moved(&m, &QAbstractItemModel::rowsMoved);
    QVERIFY(m.move(0, 2));
    QCOMPARE(moved.count(), 1);
    QCOMPARE(m.data(m.index(2), MissionModel::LatitudeRole).toDouble(), 39.0);
    QCOMPARE(m.data(m.index(0), MissionModel::LatitudeRole).toDouble(), 39.1);
    QCOMPARE(m.data(m.index(2), MissionModel::SequenceRole).toInt(), 2);
}

void TestMissionModel::rejectsInvalidInput() {
    MissionModel m;
    m.appendWaypoint(95.0, 32.0);  // geçersiz enlem
    QCOMPARE(m.count(), 0);
    m.appendWaypoint(39.0, 32.0);
    QVERIFY(!m.setCoordinate(0, 39.0, 200.0));
    QVERIFY(!m.setAltitude(0, -5));
    QVERIFY(!m.setAltitude(3, 50));
    QVERIFY(!m.remove(7));
    QVERIFY(!m.move(0, 1));
}

void TestMissionModel::saveLoadRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("gorev.plan"));
    MissionModel a;
    a.appendTakeoff(25);
    a.setDefaultAltitude(55);
    a.appendWaypoint(39.88912346, 32.77851111);
    a.appendWaypoint(39.89, 32.78);
    QVERIFY(a.setHoldTime(1, 4.5));
    a.appendReturnToLaunch();
    QVERIFY2(a.save(path), qPrintable(a.lastError()));

    MissionModel b;
    QVERIFY2(b.load(QUrl::fromLocalFile(path).toString()), qPrintable(b.lastError()));  // file:// URL de kabul
    QCOMPARE(b.count(), 4);
    for (int i = 0; i < 4; ++i) {
        const auto& x = a.items()[static_cast<std::size_t>(i)];
        const auto& y = b.items()[static_cast<std::size_t>(i)];
        QCOMPARE(y.command, x.command);
        QCOMPARE(y.lat, x.lat);  // JSON double tam yazılır
        QCOMPARE(y.lon, x.lon);
        QCOMPARE(y.alt, x.alt);
        QCOMPARE(y.param1, x.param1);
    }
    QVERIFY(qAbs(b.totalDistance() - a.totalDistance()) < 1e-9);
}

void TestMissionModel::savedFileIsQgcStyleSimpleItems() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("x.plan"));
    MissionModel m;
    m.appendWaypoint(39.5, 32.5);
    QVERIFY(m.save(path));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("fileType")).toString(), QStringLiteral("Plan"));
    const QJsonObject item =
        root.value(QStringLiteral("mission")).toObject().value(QStringLiteral("items")).toArray().at(0).toObject();
    QCOMPARE(item.value(QStringLiteral("type")).toString(), QStringLiteral("SimpleItem"));
    QCOMPARE(item.value(QStringLiteral("command")).toInt(), 16);
    QCOMPARE(item.value(QStringLiteral("frame")).toInt(), 3);
    QCOMPARE(item.value(QStringLiteral("params")).toArray().at(4).toDouble(), 39.5);
}

void TestMissionModel::loadRejectsUnsupportedContent_data() {
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("expected");
    QTest::newRow("bozuk JSON") << QByteArray("{ not json") << QStringLiteral("Geçersiz JSON");
    QTest::newRow("tür yanlış") << QByteArray(R"({"fileType":"Fence"})") << QStringLiteral("Plan");
    QTest::newRow("ComplexItem")
        << QByteArray(R"({"fileType":"Plan","mission":{"items":[{"type":"ComplexItem","complexItemType":"survey"}]}})")
        << QStringLiteral("desteklenmiyor");
    QTest::newRow("frame 0")
        << QByteArray(R"({"fileType":"Plan","mission":{"items":[{"type":"SimpleItem","command":16,"frame":0,"params":[0,0,0,0,39,32,50]}]}})")
        << QStringLiteral("frame 0");
    QTest::newRow("eksik params")
        << QByteArray(R"({"fileType":"Plan","mission":{"items":[{"type":"SimpleItem","command":16,"frame":3,"params":[0,0]}]}})")
        << QStringLiteral("7 elemanlı");
    QTest::newRow("geçersiz koordinat")
        << QByteArray(R"({"fileType":"Plan","mission":{"items":[{"type":"SimpleItem","command":16,"frame":3,"params":[0,0,0,0,123,32,50]}]}})")
        << QStringLiteral("koordinat");
}

void TestMissionModel::loadRejectsUnsupportedContent() {
    QFETCH(QByteArray, json);
    QFETCH(QString, expected);
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("bad.plan"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(json);
    f.close();
    MissionModel m;
    m.appendWaypoint(39, 32);  // başarısız yükleme mevcut görevi bozmamalı
    QVERIFY(!m.load(path));
    QVERIFY2(m.lastError().contains(expected), qPrintable(m.lastError()));
    QCOMPARE(m.count(), 1);
}

QTEST_GUILESS_MAIN(TestMissionModel)
#include "tst_missionmodel.moc"
