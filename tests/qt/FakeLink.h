#pragma once
// Testler için bağlantı: gönderilenleri ayrıştırıp saklar, istenen çerçeveyi enjekte eder.

#include <vector>

#include "gcs/mavlink/codec.hpp"
#include "link/LinkInterface.h"

class FakeLink : public LinkInterface {
    Q_OBJECT
public:
    using LinkInterface::LinkInterface;

    void sendBytes(const QByteArray& bytes) override {
        parser_.feed(reinterpret_cast<const std::uint8_t*>(bytes.constData()),
                     static_cast<std::size_t>(bytes.size()),
                     [this](const gcs::mav::Frame& f) { sent.push_back(f); });
    }

    template <class M>
    void inject(const M& msg, std::uint8_t sysid = 1, std::uint8_t compid = gcs::mav::CompIdAutopilot) {
        const auto bytes = gcs::mav::encode(msg, gcs::mav::Header{seq_++, sysid, compid});
        gcs::mav::Parser p;
        p.feed(bytes.data(), bytes.size(), [this](const gcs::mav::Frame& f) { emit frameReceived(f); });
    }

    void injectHeartbeat(bool armed = false, std::uint32_t mode = 0, std::uint8_t sysid = 1) {
        gcs::mav::Heartbeat hb;
        hb.type = gcs::mav::TypeQuadrotor;
        hb.autopilot = gcs::mav::AutopilotGeneric;
        hb.base_mode = gcs::mav::ModeFlagCustomModeEnabled;
        if (armed) hb.base_mode |= gcs::mav::ModeFlagSafetyArmed;
        hb.custom_mode = mode;
        inject(hb, sysid);
    }

    template <class M>
    std::vector<M> sentOf() const {
        std::vector<M> out;
        for (const auto& f : sent) {
            if (auto m = gcs::mav::decode<M>(f)) out.push_back(*m);
        }
        return out;
    }

    std::vector<gcs::mav::Frame> sent;

private:
    gcs::mav::Parser parser_;
    std::uint8_t seq_ = 0;
};
