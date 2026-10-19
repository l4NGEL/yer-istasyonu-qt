#pragma once
// Küresel yeryüzü yaklaşımıyla mesafe/kerteriz hesapları (görev uzunluğu, SITL hareketi).

namespace gcs::geo {

constexpr double kEarthRadiusM = 6371008.8;  // ortalama yarıçap (IUGG)

struct LatLon {
    double lat = 0;  // derece
    double lon = 0;  // derece
};

double distanceM(const LatLon& a, const LatLon& b);        // haversine
double bearingDeg(const LatLon& from, const LatLon& to);   // 0..360, kuzeyden saat yönü
LatLon destination(const LatLon& from, double bearingDeg, double distanceM);
double wrap360(double deg);
double wrap180(double deg);

// ZYX (yaw-pitch-roll) Euler açıları, derece. Gimbal tutum durumu MAVLink'te
// kuaterniyon (w, x, y, z) olarak taşınır.
struct EulerDeg {
    double roll = 0, pitch = 0, yaw = 0;
};
struct Quat {
    double w = 1, x = 0, y = 0, z = 0;
};
Quat quatFromEuler(const EulerDeg& e);
EulerDeg eulerFromQuat(const Quat& q);

}  // namespace gcs::geo
