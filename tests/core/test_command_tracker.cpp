#include <gtest/gtest.h>

#include <vector>

#include "gcs/command_tracker.hpp"

using namespace gcs;

namespace {
mav::CommandLong makeCmd(std::uint16_t id, float p1 = 0) {
    mav::CommandLong c;
    c.command = id;
    c.target_system = 1;
    c.param1 = p1;
    return c;
}

mav::CommandAck makeAck(std::uint16_t id, std::uint8_t result) {
    mav::CommandAck a;
    a.command = id;
    a.result = result;
    return a;
}

struct Fixture {
    CommandTracker t;
    std::vector<mav::CommandLong> sent;
    std::vector<CommandResult> results;
    Fixture() {
        t.send = [this](const mav::CommandLong& c) { sent.push_back(c); };
        t.onResult = [this](const CommandResult& r) { results.push_back(r); };
    }
};
}  // namespace

TEST(CommandTracker, AcceptedAck) {
    Fixture f;
    f.t.submit(makeCmd(mav::cmd::ComponentArmDisarm, 1), 0);
    ASSERT_EQ(f.sent.size(), 1u);
    EXPECT_TRUE(f.t.isPending(mav::cmd::ComponentArmDisarm));
    f.t.handleAck(makeAck(mav::cmd::ComponentArmDisarm, mav::result::Accepted), 1, 100);
    ASSERT_EQ(f.results.size(), 1u);
    EXPECT_EQ(f.results[0].outcome, CommandOutcome::Accepted);
    EXPECT_EQ(f.t.pendingCount(), 0u);
}

TEST(CommandTracker, RetriesWithIncrementingConfirmationThenTimesOut) {
    Fixture f;
    f.t.timeoutMs = 100;
    f.t.maxAttempts = 3;
    f.t.submit(makeCmd(mav::cmd::MissionStart), 0);
    for (std::int64_t now = 0; now <= 1000; now += 10) f.t.tick(now);
    ASSERT_EQ(f.sent.size(), 3u);
    EXPECT_EQ(f.sent[0].confirmation, 0);
    EXPECT_EQ(f.sent[1].confirmation, 1);
    EXPECT_EQ(f.sent[2].confirmation, 2);
    ASSERT_EQ(f.results.size(), 1u);
    EXPECT_EQ(f.results[0].outcome, CommandOutcome::TimedOut);
    EXPECT_EQ(f.results[0].attempts, 3);
}

TEST(CommandTracker, InProgressExtendsDeadline) {
    Fixture f;
    f.t.timeoutMs = 100;
    f.t.inProgressTimeoutMs = 1000;
    f.t.submit(makeCmd(mav::cmd::NavTakeoff), 0);
    f.t.handleAck(makeAck(mav::cmd::NavTakeoff, mav::result::InProgress), 1, 50);
    for (std::int64_t now = 50; now < 900; now += 10) f.t.tick(now);
    EXPECT_EQ(f.sent.size(), 1u);  // yeniden gönderim yok
    EXPECT_TRUE(f.results.empty());
}

TEST(CommandTracker, RejectedResultIsReported) {
    Fixture f;
    f.t.submit(makeCmd(mav::cmd::MissionStart), 0);
    f.t.handleAck(makeAck(mav::cmd::MissionStart, mav::result::Denied), 1, 10);
    ASSERT_EQ(f.results.size(), 1u);
    EXPECT_EQ(f.results[0].outcome, CommandOutcome::Rejected);
    EXPECT_EQ(f.results[0].mavResult, mav::result::Denied);
    EXPECT_NE(describe(f.results[0]).find("reddedildi"), std::string::npos);
}

TEST(CommandTracker, NewerSameCommandReplacesPending) {
    Fixture f;
    f.t.submit(makeCmd(mav::cmd::DoGimbalManagerPitchYaw, -10), 0);
    f.t.submit(makeCmd(mav::cmd::DoGimbalManagerPitchYaw, -20), 5);
    ASSERT_EQ(f.results.size(), 1u);
    EXPECT_EQ(f.results[0].outcome, CommandOutcome::Replaced);
    EXPECT_EQ(f.t.pendingCount(), 1u);
    f.t.timeoutMs = 10;
    f.t.tick(2000);
    EXPECT_FLOAT_EQ(f.sent.back().param1, -20.f);  // yeniden gönderilen en son değer
}

TEST(CommandTracker, AckFromOtherSystemIgnored) {
    Fixture f;
    f.t.submit(makeCmd(mav::cmd::ComponentArmDisarm, 1), 0);
    f.t.handleAck(makeAck(mav::cmd::ComponentArmDisarm, mav::result::Accepted), 2, 10);
    EXPECT_TRUE(f.results.empty());
    EXPECT_TRUE(f.t.isPending(mav::cmd::ComponentArmDisarm));
}

TEST(CommandTracker, IndependentCommandsTrackedSeparately) {
    Fixture f;
    f.t.submit(makeCmd(mav::cmd::ImageStartCapture), 0);
    f.t.submit(makeCmd(mav::cmd::SetCameraZoom), 0);
    f.t.handleAck(makeAck(mav::cmd::SetCameraZoom, mav::result::Accepted), 1, 10);
    EXPECT_TRUE(f.t.isPending(mav::cmd::ImageStartCapture));
    EXPECT_FALSE(f.t.isPending(mav::cmd::SetCameraZoom));
}
