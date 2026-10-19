#include "gcs/geo.hpp"

#include <cmath>

namespace gcs::geo {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double toRad(double d) { return d * kPi / 180.0; }
constexpr double toDeg(double r) { return r * 180.0 / kPi; }
}  // namespace

double wrap360(double deg) {
    double r = std::fmod(deg, 360.0);
    return r < 0 ? r + 360.0 : r;
}

double wrap180(double deg) {
    double r = wrap360(deg);
    return r > 180.0 ? r - 360.0 : r;
}

double distanceM(const LatLon& a, const LatLon& b) {
    const double dLat = toRad(b.lat - a.lat);
    const double dLon = toRad(b.lon - a.lon);
    const double h = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(toRad(a.lat)) * std::cos(toRad(b.lat)) * std::sin(dLon / 2) *
                         std::sin(dLon / 2);
    return 2.0 * kEarthRadiusM * std::asin(std::sqrt(std::fmin(1.0, h)));
}

double bearingDeg(const LatLon& from, const LatLon& to) {
    const double phi1 = toRad(from.lat), phi2 = toRad(to.lat);
    const double dLon = toRad(to.lon - from.lon);
    const double y = std::sin(dLon) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(dLon);
    return wrap360(toDeg(std::atan2(y, x)));
}

LatLon destination(const LatLon& from, double bearing, double distance) {
    const double delta = distance / kEarthRadiusM;
    const double theta = toRad(bearing);
    const double phi1 = toRad(from.lat), lambda1 = toRad(from.lon);
    const double phi2 = std::asin(std::sin(phi1) * std::cos(delta) +
                                  std::cos(phi1) * std::sin(delta) * std::cos(theta));
    const double lambda2 = lambda1 + std::atan2(std::sin(theta) * std::sin(delta) * std::cos(phi1),
                                                std::cos(delta) - std::sin(phi1) * std::sin(phi2));
    return {toDeg(phi2), wrap180(toDeg(lambda2))};
}

Quat quatFromEuler(const EulerDeg& e) {
    const double cr = std::cos(toRad(e.roll) / 2), sr = std::sin(toRad(e.roll) / 2);
    const double cp = std::cos(toRad(e.pitch) / 2), sp = std::sin(toRad(e.pitch) / 2);
    const double cy = std::cos(toRad(e.yaw) / 2), sy = std::sin(toRad(e.yaw) / 2);
    return {cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy};
}

EulerDeg eulerFromQuat(const Quat& q) {
    EulerDeg e;
    e.roll = toDeg(std::atan2(2 * (q.w * q.x + q.y * q.z), 1 - 2 * (q.x * q.x + q.y * q.y)));
    const double s = 2 * (q.w * q.y - q.z * q.x);
    e.pitch = toDeg(std::asin(std::fmax(-1.0, std::fmin(1.0, s))));
    e.yaw = toDeg(std::atan2(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z)));
    return e;
}

}  // namespace gcs::geo
