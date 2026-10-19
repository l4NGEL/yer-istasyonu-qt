// Sahte MAVLink aracı: SimVehicle'ı UDP üzerinden yer istasyonuna bağlar (Linux/POSIX).
//
//   gcs-sitl --gcs 127.0.0.1:14550 --listen 14555 [--loss 0.05] [--seed 1]
//
// Telemetriyi --gcs adresine gönderir; GCS aynı sokete (kaynak port) yanıt verir.
// --loss ile giden ve gelen paketlerin bir kısmı rastgele düşürülür; görev yükleme
// protokolünün yeniden deneme yolunu canlı sınamak için.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "gcs/mavlink/codec.hpp"
#include "sim_vehicle.hpp"

namespace {

volatile std::sig_atomic_t gStop = 0;
void onSignal(int) { gStop = 1; }

struct Options {
    std::string gcsHost = "127.0.0.1";
    int gcsPort = 14550;
    int listenPort = 14555;
    double loss = 0.0;
    unsigned seed = 1;
};

bool parseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        if (a == "--gcs") {
            const char* v = next();
            if (!v) return false;
            const std::string s = v;
            const auto colon = s.rfind(':');
            if (colon == std::string::npos) return false;
            o.gcsHost = s.substr(0, colon);
            o.gcsPort = std::atoi(s.c_str() + colon + 1);
        } else if (a == "--listen") {
            const char* v = next();
            if (!v) return false;
            o.listenPort = std::atoi(v);
        } else if (a == "--loss") {
            const char* v = next();
            if (!v) return false;
            o.loss = std::atof(v);
        } else if (a == "--seed") {
            const char* v = next();
            if (!v) return false;
            o.seed = static_cast<unsigned>(std::strtoul(v, nullptr, 10));
        } else {
            return false;
        }
    }
    return true;
}

const char* modeName(gcs::sitl::Mode m) {
    switch (m) {
        case gcs::sitl::Mode::Hold: return "HOLD";
        case gcs::sitl::Mode::Auto: return "AUTO";
        case gcs::sitl::Mode::ReturnToLaunch: return "RTL";
        case gcs::sitl::Mode::Land: return "LAND";
    }
    return "?";
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        std::fprintf(stderr, "kullanım: %s [--gcs host:port] [--listen port] [--loss 0..1] [--seed n]\n",
                     argv[0]);
        return 2;
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { std::perror("socket"); return 1; }
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(static_cast<std::uint16_t>(opt.listenPort));
    if (::bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        std::perror("bind");
        return 1;
    }
    sockaddr_in gcsAddr{};
    gcsAddr.sin_family = AF_INET;
    gcsAddr.sin_port = htons(static_cast<std::uint16_t>(opt.gcsPort));
    if (::inet_pton(AF_INET, opt.gcsHost.c_str(), &gcsAddr.sin_addr) != 1) {
        std::fprintf(stderr, "geçersiz GCS adresi: %s\n", opt.gcsHost.c_str());
        return 2;
    }

    std::mt19937 rng(opt.seed);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    auto dropped = [&]() { return opt.loss > 0 && uni(rng) < opt.loss; };

    gcs::sitl::SimVehicle vehicle;
    gcs::mav::Parser parser;
    gcs::sitl::Outbox out;
    std::uint64_t sent = 0, dropOut = 0, dropIn = 0;

    std::printf("gcs-sitl: %s:%d adresine gönderiliyor, %d dinleniyor, kayıp=%.2f\n",
                opt.gcsHost.c_str(), opt.gcsPort, opt.listenPort, opt.loss);
    std::fflush(stdout);

    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    auto last = t0;
    auto lastLog = t0;

    while (!gStop) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        timeval tv{0, 10000};  // 10 ms → yaklaşık 100 Hz döngü
        const int ready = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
        const auto now = Clock::now();
        const std::int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();

        if (ready > 0 && FD_ISSET(fd, &rfds)) {
            std::uint8_t buf[2048];
            const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n > 0) {
                if (dropped()) {
                    ++dropIn;
                } else {
                    parser.feed(buf, static_cast<std::size_t>(n),
                                [&](const gcs::mav::Frame& f) { vehicle.handle(f, nowMs, out); });
                }
            }
        }

        const double dt = std::chrono::duration<double>(now - last).count();
        if (dt >= 0.01) {
            vehicle.step(dt, nowMs, out);
            last = now;
        }

        for (const auto& pkt : out) {
            if (dropped()) {
                ++dropOut;
                continue;
            }
            ::sendto(fd, pkt.data(), pkt.size(), 0, reinterpret_cast<sockaddr*>(&gcsAddr),
                     sizeof(gcsAddr));
            ++sent;
        }
        out.clear();

        if (now - lastLog > std::chrono::seconds(5)) {
            lastLog = now;
            std::printf("[%7.1fs] %s %s alt=%.1fm hdg=%.0f wp=%d/%zu foto=%d gönderilen=%llu "
                        "düşen(giden/gelen)=%llu/%llu\n",
                        static_cast<double>(nowMs) / 1000.0, vehicle.armed() ? "ARMED" : "DISARMED",
                        modeName(vehicle.mode()), vehicle.relAlt(), vehicle.headingDeg(),
                        vehicle.currentSeq(), vehicle.mission().size(), vehicle.photoCount(),
                        static_cast<unsigned long long>(sent), static_cast<unsigned long long>(dropOut),
                        static_cast<unsigned long long>(dropIn));
            std::fflush(stdout);
        }
    }
    ::close(fd);
    std::printf("gcs-sitl: kapandı\n");
    return 0;
}
