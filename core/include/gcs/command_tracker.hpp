#pragma once
// COMMAND_LONG gönderip COMMAND_ACK bekleyen takipçi.
//
// MAVLink'te onay komut kimliğiyle eşleşir; bu yüzden aynı kimlikten aynı anda
// tek bekleyen komut tutulur ve yenisi eskisinin yerini alır (ör. gimbal
// kaydırıcısı sürüklenirken yalnız son açı önemlidir). Yanıt gelmezse
// confirmation alanı artırılarak yeniden gönderilir.

#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "gcs/mavlink/messages.hpp"

namespace gcs {

enum class CommandOutcome { Accepted, Rejected, TimedOut, Replaced };

struct CommandResult {
    std::uint16_t command = 0;
    CommandOutcome outcome = CommandOutcome::TimedOut;
    std::uint8_t mavResult = 0;  // Accepted/Rejected için MAV_RESULT
    int attempts = 0;
};

std::string describe(const CommandResult& r);

class CommandTracker {
public:
    std::function<void(const mav::CommandLong&)> send;
    std::function<void(const CommandResult&)> onResult;

    std::int64_t timeoutMs = 1000;
    std::int64_t inProgressTimeoutMs = 5000;  // IN_PROGRESS gelince süre uzar
    int maxAttempts = 3;

    void submit(mav::CommandLong cmd, std::int64_t nowMs);
    void handleAck(const mav::CommandAck& ack, std::uint8_t fromSys, std::int64_t nowMs);
    void tick(std::int64_t nowMs);

    bool isPending(std::uint16_t command) const { return pending_.count(command) != 0; }
    std::size_t pendingCount() const { return pending_.size(); }

private:
    struct Pending {
        mav::CommandLong cmd;
        std::int64_t deadline = 0;
        int attempts = 0;
    };
    std::map<std::uint16_t, Pending> pending_;
};

}  // namespace gcs
