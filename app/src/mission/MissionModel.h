#pragma once
// QML harita ve listenin paylaştığı görev öğesi modeli; .plan dosyasına
// kaydetme/yükleme. Dosya biçimi QGroundControl .plan'ın "SimpleItem" alt
// kümesini izler (ComplexItem, geofence ve rally noktaları desteklenmez).

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <vector>

#include "gcs/mission.hpp"

class MissionModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(double totalDistance READ totalDistance NOTIFY geometryChanged)
    Q_PROPERTY(double defaultAltitude READ defaultAltitude WRITE setDefaultAltitude NOTIFY
                   defaultAltitudeChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Roles {
        LatitudeRole = Qt::UserRole + 1,
        LongitudeRole,
        AltitudeRole,
        CommandRole,
        CommandNameRole,
        HoldTimeRole,
        SequenceRole,
        HasPositionRole,
    };

    explicit MissionModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void appendWaypoint(double lat, double lon);
    Q_INVOKABLE void insertWaypoint(int row, double lat, double lon);
    Q_INVOKABLE void appendTakeoff(double altitude);
    Q_INVOKABLE void appendReturnToLaunch();
    Q_INVOKABLE bool remove(int row);
    Q_INVOKABLE bool move(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool setCoordinate(int row, double lat, double lon);
    Q_INVOKABLE bool setAltitude(int row, double alt);
    Q_INVOKABLE bool setHoldTime(int row, double seconds);

    // Harita çizimi için konumlu öğelerin [{lat, lon}] listesi
    Q_INVOKABLE QVariantList pathPoints() const;

    Q_INVOKABLE bool save(const QString& pathOrUrl);
    Q_INVOKABLE bool load(const QString& pathOrUrl);

    int count() const { return static_cast<int>(items_.size()); }
    double totalDistance() const;
    double defaultAltitude() const { return defaultAltitude_; }
    void setDefaultAltitude(double alt);
    QString lastError() const { return lastError_; }

    const std::vector<gcs::MissionItem>& items() const { return items_; }
    void setItems(std::vector<gcs::MissionItem> items);

    static QString commandName(std::uint16_t command);

signals:
    void countChanged();
    void geometryChanged();
    void defaultAltitudeChanged();
    void lastErrorChanged();

private:
    void setError(const QString& e);
    void notifyRowChanged(int row);

    std::vector<gcs::MissionItem> items_;
    double defaultAltitude_ = 40.0;
    QString lastError_;
};
