#include "gcs/command_tracker.hpp"

#include <vector>

namespace gcs {

std::string describe(const CommandResult& r) {
    const std::string id = "komut " + std::to_string(r.command);
    switch (r.outcome) {
        case CommandOutcome::Accepted:
            return id + ": kabul edildi";
        case CommandOutcome::Replaced:
            return id + ": yenisiyle değiştirildi";
        case CommandOutcome::TimedOut:
            return id + ": " + std::to_string(r.attempts) + " denemede yanıt yok";
        case CommandOutcome::Rejected:
            break;
    }
    switch (r.mavResult) {
        case mav::result::TemporarilyRejected: return id + ": geçici olarak reddedildi";
        case mav::result::Denied: return id + ": reddedildi";
        case mav::result::Unsupported: return id + ": desteklenmiyor";
        case mav::result::Failed: return id + ": başarısız";
        default: return id + ": sonuç " + std::to_string(r.mavResult);
    }
}

void CommandTracker::submit(mav::CommandLong cmd, std::int64_t nowMs) {
    auto it = pending_.find(cmd.command);
    if (it != pending_.end()) {
        CommandResult replaced{cmd.command, CommandOutcome::Replaced, 0, it->second.attempts};
        pending_.erase(it);
        if (onResult) onResult(replaced);
    }
    cmd.confirmation = 0;
    Pending p{cmd, nowMs + timeoutMs, 1};
    pending_[cmd.command] = p;
    if (send) send(cmd);
}

void CommandTracker::handleAck(const mav::CommandAck& ack, std::uint8_t fromSys, std::int64_t nowMs) {
    auto it = pending_.find(ack.command);
    if (it == pending_.end() || it->second.cmd.target_system != fromSys) return;
    if (ack.result == mav::result::InProgress) {
        it->second.deadline = nowMs + inProgressTimeoutMs;
        return;
    }
    CommandResult r;
    r.command = ack.command;
    r.mavResult = ack.result;
    r.attempts = it->second.attempts;
    r.outcome = ack.result == mav::result::Accepted ? CommandOutcome::Accepted : CommandOutcome::Rejected;
    pending_.erase(it);
    if (onResult) onResult(r);
}

void CommandTracker::tick(std::int64_t nowMs) {
    std::vector<CommandResult> expired;
    for (auto it = pending_.begin(); it != pending_.end();) {
        Pending& p = it->second;
        if (nowMs < p.deadline) {
            ++it;
            continue;
        }
        if (p.attempts >= maxAttempts) {
            expired.push_back({p.cmd.command, CommandOutcome::TimedOut, 0, p.attempts});
            it = pending_.erase(it);
            continue;
        }
        ++p.attempts;
        ++p.cmd.confirmation;
        p.deadline = nowMs + timeoutMs;
        if (send) send(p.cmd);
        ++it;
    }
    // Geri çağrılar haritayı değiştirebilir (yeni submit); döngü dışında çağrılır.
    for (const auto& r : expired) {
        if (onResult) onResult(r);
    }
}

}  // namespace gcs
