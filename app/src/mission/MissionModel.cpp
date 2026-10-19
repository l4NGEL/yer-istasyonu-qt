#include "MissionModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVariantMap>
#include <QUrl>
#include <algorithm>
#include <cmath>

using gcs::MissionItem;
namespace cmd = gcs::mav::cmd;

namespace {

QString toLocalPath(const QString& pathOrUrl) {
    if (pathOrUrl.startsWith(QStringLiteral("file:"))) return QUrl(pathOrUrl).toLocalFile();
    return pathOrUrl;
}

bool hasPosition(std::uint16_t c) { return c == cmd::NavWaypoint || c == cmd::NavLand; }

bool validLatLon(double lat, double lon) {
    return std::isfinite(lat) && std::isfinite(lon) && lat >= -90 && lat <= 90 && lon >= -180 &&
           lon <= 180;
}

}  // namespace

MissionModel::MissionModel(QObject* parent) : QAbstractListModel(parent) {}

QString MissionModel::commandName(std::uint16_t command) {
    switch (command) {
        case cmd::NavWaypoint: return QStringLiteral("Yol noktası");
        case cmd::NavTakeoff: return QStringLiteral("Kalkış");
        case cmd::NavLand: return QStringLiteral("İniş");
        case cmd::NavReturnToLaunch: return QStringLiteral("Eve dönüş");
        default: return QStringLiteral("Komut %1").arg(command);
    }
}

int MissionModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count();
}

QVariant MissionModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const MissionItem& it = items_[static_cast<std::size_t>(index.row())];
    switch (role) {
        case LatitudeRole: return it.lat;
        case LongitudeRole: return it.lon;
        case AltitudeRole: return static_cast<double>(it.alt);
        case CommandRole: return it.command;
        case CommandNameRole: return commandName(it.command);
        case HoldTimeRole: return static_cast<double>(it.param1);
        case SequenceRole: return index.row();
        case HasPositionRole: return hasPosition(it.command);
        default: return {};
    }
}

QHash<int, QByteArray> MissionModel::roleNames() const {
    return {{LatitudeRole, "latitude"},       {LongitudeRole, "longitude"},
            {AltitudeRole, "altitude"},       {CommandRole, "command"},
            {CommandNameRole, "commandName"}, {HoldTimeRole, "holdTime"},
            {SequenceRole, "sequence"},       {HasPositionRole, "hasPosition"}};
}

void MissionModel::insertWaypoint(int row, double lat, double lon) {
    if (!validLatLon(lat, lon)) return;
    row = std::max(0, std::min(row, count()));
    MissionItem it;
    it.command = cmd::NavWaypoint;
    it.lat = lat;
    it.lon = lon;
    it.alt = static_cast<float>(defaultAltitude_);
    beginInsertRows({}, row, row);
    items_.insert(items_.begin() + row, it);
    endInsertRows();
    // Sıra numaraları kaydı; sonraki satırların "sequence" rolü değişti.
    if (row + 1 < count()) emit dataChanged(index(row + 1), index(count() - 1), {SequenceRole});
    emit countChanged();
    emit geometryChanged();
}

void MissionModel::appendWaypoint(double lat, double lon) { insertWaypoint(count(), lat, lon); }

void MissionModel::appendTakeoff(double altitude) {
    MissionItem it;
    it.command = cmd::NavTakeoff;
    it.alt = static_cast<float>(altitude);
    beginInsertRows({}, count(), count());
    items_.push_back(it);
    endInsertRows();
    emit countChanged();
}

void MissionModel::appendReturnToLaunch() {
    MissionItem it;
    it.command = cmd::NavReturnToLaunch;
    it.alt = 0;
    beginInsertRows({}, count(), count());
    items_.push_back(it);
    endInsertRows();
    emit countChanged();
}

bool MissionModel::remove(int row) {
    if (row < 0 || row >= count()) return false;
    beginRemoveRows({}, row, row);
    items_.erase(items_.begin() + row);
    endRemoveRows();
    if (row < count()) emit dataChanged(index(row), index(count() - 1), {SequenceRole});
    emit countChanged();
    emit geometryChanged();
    return true;
}

bool MissionModel::move(int from, int to) {
    if (from < 0 || from >= count() || to < 0 || to >= count()) return false;
    if (from == to) return true;
    // Qt'nin hedef indeksi "taşımadan önceki" konumdur: aşağı taşırken +1.
    if (!beginMoveRows({}, from, from, {}, to > from ? to + 1 : to)) return false;
    gcs::moveItem(items_, from, to);
    endMoveRows();
    emit dataChanged(index(std::min(from, to)), index(std::max(from, to)), {SequenceRole});
    emit geometryChanged();
    return true;
}

void MissionModel::clear() {
    if (items_.empty()) return;
    beginResetModel();
    items_.clear();
    endResetModel();
    emit countChanged();
    emit geometryChanged();
}

void MissionModel::notifyRowChanged(int row) { emit dataChanged(index(row), index(row)); }

bool MissionModel::setCoordinate(int row, double lat, double lon) {
    if (row < 0 || row >= count() || !validLatLon(lat, lon)) return false;
    auto& it = items_[static_cast<std::size_t>(row)];
    it.lat = lat;
    it.lon = lon;
    notifyRowChanged(row);
    emit geometryChanged();
    return true;
}

bool MissionModel::setAltitude(int row, double alt) {
    if (row < 0 || row >= count() || !std::isfinite(alt) || alt < 0 || alt > 5000) return false;
    items_[static_cast<std::size_t>(row)].alt = static_cast<float>(alt);
    notifyRowChanged(row);
    return true;
}

bool MissionModel::setHoldTime(int row, double seconds) {
    if (row < 0 || row >= count() || !std::isfinite(seconds) || seconds < 0) return false;
    items_[static_cast<std::size_t>(row)].param1 = static_cast<float>(seconds);
    notifyRowChanged(row);
    return true;
}

QVariantList MissionModel::pathPoints() const {
    QVariantList out;
    for (const auto& it : items_) {
        if (!hasPosition(it.command)) continue;
        out.append(QVariantMap{{QStringLiteral("lat"), it.lat}, {QStringLiteral("lon"), it.lon}});
    }
    return out;
}

double MissionModel::totalDistance() const { return gcs::pathLengthM(items_); }

void MissionModel::setDefaultAltitude(double alt) {
    if (alt <= 0 || qFuzzyCompare(alt, defaultAltitude_)) return;
    defaultAltitude_ = alt;
    emit defaultAltitudeChanged();
}

void MissionModel::setItems(std::vector<MissionItem> items) {
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
    emit countChanged();
    emit geometryChanged();
}

void MissionModel::setError(const QString& e) {
    lastError_ = e;
    emit lastErrorChanged();
}

bool MissionModel::save(const QString& pathOrUrl) {
    QJsonArray jsonItems;
    int jumpId = 1;
    for (const auto& it : items_) {
        QJsonObject o;
        o[QStringLiteral("type")] = QStringLiteral("SimpleItem");
        o[QStringLiteral("autoContinue")] = true;
        o[QStringLiteral("command")] = it.command;
        o[QStringLiteral("doJumpId")] = jumpId++;
        o[QStringLiteral("frame")] = gcs::mav::FrameGlobalRelativeAlt;
        o[QStringLiteral("params")] = QJsonArray{it.param1, it.param2, it.param3, it.param4,
                                                 it.lat,    it.lon,    it.alt};
        jsonItems.append(o);
    }
    QJsonObject mission;
    mission[QStringLiteral("version")] = 2;
    mission[QStringLiteral("firmwareType")] = 0;
    mission[QStringLiteral("vehicleType")] = 2;
    mission[QStringLiteral("items")] = jsonItems;
    if (!items_.empty()) {
        // QGC planlanan ev konumunu zorunlu tutar; ilk konumlu öğe kullanılır.
        for (const auto& it : items_) {
            if (hasPosition(it.command)) {
                mission[QStringLiteral("plannedHomePosition")] = QJsonArray{it.lat, it.lon, 0};
                break;
            }
        }
    }
    QJsonObject root;
    root[QStringLiteral("fileType")] = QStringLiteral("Plan");
    root[QStringLiteral("groundStation")] = QStringLiteral("yer-istasyonu-qt");
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("mission")] = mission;
    root[QStringLiteral("geoFence")] =
        QJsonObject{{QStringLiteral("circles"), QJsonArray{}}, {QStringLiteral("polygons"), QJsonArray{}},
                    {QStringLiteral("version"), 2}};
    root[QStringLiteral("rallyPoints")] =
        QJsonObject{{QStringLiteral("points"), QJsonArray{}}, {QStringLiteral("version"), 2}};

    QSaveFile f(toLocalPath(pathOrUrl));  // atomik yazım: yarım dosya kalmaz
    if (!f.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Dosya açılamadı: %1").arg(f.errorString()));
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        setError(QStringLiteral("Dosya yazılamadı: %1").arg(f.errorString()));
        return false;
    }
    setError({});
    return true;
}

bool MissionModel::load(const QString& pathOrUrl) {
    QFile f(toLocalPath(pathOrUrl));
    if (!f.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Dosya açılamadı: %1").arg(f.errorString()));
        return false;
    }
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(QStringLiteral("Geçersiz JSON: %1").arg(perr.errorString()));
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("fileType")).toString() != QStringLiteral("Plan")) {
        setError(QStringLiteral("fileType \"Plan\" değil"));
        return false;
    }
    const QJsonValue itemsVal = root.value(QStringLiteral("mission")).toObject().value(QStringLiteral("items"));
    if (!itemsVal.isArray()) {
        setError(QStringLiteral("mission.items dizisi yok"));
        return false;
    }

    std::vector<MissionItem> loaded;
    const QJsonArray arr = itemsVal.toArray();
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonObject o = arr.at(i).toObject();
        const QString type = o.value(QStringLiteral("type")).toString();
        if (type != QStringLiteral("SimpleItem")) {
            setError(QStringLiteral("Öğe %1: \"%2\" türü desteklenmiyor (yalnız SimpleItem)")
                         .arg(i + 1).arg(type));
            return false;
        }
        const int frame = o.value(QStringLiteral("frame")).toInt(-1);
        if (frame != gcs::mav::FrameGlobalRelativeAlt) {
            setError(QStringLiteral("Öğe %1: frame %2 desteklenmiyor (yalnız 3, eve bağıl)")
                         .arg(i + 1).arg(frame));
            return false;
        }
        const QJsonArray p = o.value(QStringLiteral("params")).toArray();
        if (p.size() != 7) {
            setError(QStringLiteral("Öğe %1: params 7 elemanlı olmalı").arg(i + 1));
            return false;
        }
        MissionItem it;
        it.command = static_cast<std::uint16_t>(o.value(QStringLiteral("command")).toInt());
        // QGC "değiştirme" anlamında null yazabilir; 0 kabul edilir.
        it.param1 = static_cast<float>(p.at(0).toDouble(0));
        it.param2 = static_cast<float>(p.at(1).toDouble(0));
        it.param3 = static_cast<float>(p.at(2).toDouble(0));
        it.param4 = static_cast<float>(p.at(3).toDouble(0));
        it.lat = p.at(4).toDouble(0);
        it.lon = p.at(5).toDouble(0);
        it.alt = static_cast<float>(p.at(6).toDouble(0));
        if (hasPosition(it.command) && !validLatLon(it.lat, it.lon)) {
            setError(QStringLiteral("Öğe %1: geçersiz koordinat").arg(i + 1));
            return false;
        }
        loaded.push_back(it);
    }
    setItems(std::move(loaded));
    setError({});
    return true;
}
