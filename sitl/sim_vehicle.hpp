#pragma once
// Basit kinematik çok rotorlu araç benzetimi. Gerçek uçuş dinamiği değildir:
// amaç yer istasyonunun telemetri, görev, komut ve faydalı yük yollarını gerçek
// MAVLink trafiğiyle uçtan uca sınamak.
//
// Soketten bağımsızdır: gelen çerçeveyi handle()'a verir, step() ile zamanı
// ilerletir, gönderilecek paketleri çıkış listesine ekler. Ağ döngüsü
// sitl_main.cpp'de, testler doğrudan bu sınıfı kullanır.

#include <array>
#include <cstdint>
#include <vector>

#include "gcs/geo.hpp"
#include "gcs/mavlink/codec.hpp"
#include "gcs/mavlink/messages.hpp"
#include "gcs/mission.hpp"

namespace gcs::sitl {

using Packet = std::vector<std::uint8_t>;
using Outbox = std::vector<Packet>;

// Bu benzeticiye özgü özel kipler (MAV_AUTOPILOT_GENERIC'te custom_mode araca özeldir).
enum class Mode : std::uint32_t { Hold = 0, Auto = 1, ReturnToLaunch = 2, Land = 3 };

struct SimConfig {
    std::uint8_t sysid = 1;
    geo::LatLon home{39.8890, 32.7785};  // Ankara batısı, rastgele bir açık alan
    double homeAltMsl = 950.0;
    double cruiseSpeed = 12.0;   // m/s
    double climbRate = 3.0;      // m/s
    double defaultAcceptRadius = 3.0;
    double gimbalSlewDegPerS = 60.0;
};

class SimVehicle {
public:
    explicit SimVehicle(SimConfig cfg = {});

    void handle(const mav::Frame& f, std::int64_t nowMs, Outbox& out);
    void step(double dt, std::int64_t nowMs, Outbox& out);

    // Test ve günlük için durum
    bool armed() const { return armed_; }
    Mode mode() const { return mode_; }
    geo::LatLon position() const { return pos_; }
    double relAlt() const { return relAlt_; }
    double headingDeg() const { return heading_; }
    const std::vector<MissionItem>& mission() const { return mission_; }
    int currentSeq() const { return currentSeq_; }
    int photoCount() const { return photos_; }
    bool recording() const { return recording_; }
    double gimbalPitch() const { return gimbalPitch_; }
    double gimbalYaw() const { return gimbalYaw_; }
    float zoom() const { return zoom_; }

private:
    template <class M>
    void emit(const M& msg, Outbox& out, std::uint8_t compid = mav::CompIdAutopilot);
    void ack(std::uint16_t command, std::uint8_t result, const mav::Frame& to, Outbox& out);
    void status(const char* text, std::uint8_t severity, Outbox& out);
    void handleCommand(const mav::CommandLong& c, const mav::Frame& f, std::int64_t nowMs, Outbox& out);
    void physics(double dt, Outbox& out);
    void flyTowards(const geo::LatLon& target, double targetAlt, double dt, double& distOut);
    void telemetry(std::int64_t nowMs, Outbox& out);

    SimConfig cfg_;
    // MAVLink dizi numarası her gönderen bileşen için ayrı sayılır
    std::array<std::uint8_t, 256> seq_{};
    std::uint32_t bootMs_ = 0;

    bool armed_ = false;
    Mode mode_ = Mode::Hold;
    geo::LatLon pos_;
    double relAlt_ = 0;
    double heading_ = 0;
    double groundSpeed_ = 0;
    double climb_ = 0;
    double roll_ = 0, pitch_ = 0;
    double armedSeconds_ = 0;

    std::vector<MissionItem> mission_;
    int currentSeq_ = 0;
    double holdUntil_ = -1;
    double simTime_ = 0;
    MissionReceiver receiver_;
    Outbox* receiverOut_ = nullptr;

    double gimbalPitch_ = 0, gimbalYaw_ = 0;
    double gimbalPitchTarget_ = 0, gimbalYawTarget_ = 0;
    float zoom_ = 0;
    int photos_ = 0;
    bool recording_ = false;

    struct Stream {
        std::int64_t periodMs;
        std::int64_t next;
    };
    Stream hb_{1000, 0}, sys_{1000, 0}, gps_{200, 0}, gpi_{100, 0}, att_{50, 0}, hud_{200, 0},
        cur_{1000, 0}, gimbal_{100, 0};
};

}  // namespace gcs::sitl
