#include <catch2/catch_test_macros.hpp>

#include "GeraNES/InputBuffer.h"
#include "StateReplayTest.h"
#include "TestSupport.h"

TEST_CASE("InputBuffer rejects backfilling an earlier frame when a later replay frame is already queued", "[state-replay][input-buffer]")
{
    InputBuffer buffer;

    InputFrame later;
    later.frame = 11;
    later.timelineEpoch = 7;
    REQUIRE(buffer.push(later) == InputBuffer::EnqueueResult::Inserted);

    InputFrame current;
    current.frame = 10;
    current.timelineEpoch = 7;
    REQUIRE(buffer.push(current) == InputBuffer::EnqueueResult::RejectedOutOfSequence);
}

TEST_CASE("State replay remains deterministic from saved snapshots", "[state-replay]")
{
    GeraNESTestSupport::requireRomFixture();

    StateReplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.frames = 60;
    options.replayHorizon = 3;
    options.probeStride = 2;
    options.reportPath = GeraNESTestSupport::reportPath("state_replay.json").string();

    REQUIRE(StateReplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
}

TEST_CASE("State replay robust matrix stays green", "[state-replay][robust]")
{
    GeraNESTestSupport::requireRomFixture();

    StateReplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.frames = 40;
    options.replayHorizon = 2;
    options.robust = true;
    options.reportPath = GeraNESTestSupport::reportPath("state_replay_robust.json").string();

    REQUIRE(StateReplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
}

TEST_CASE("State replay remains deterministic across deep late-frame probes", "[state-replay][deep]")
{
    GeraNESTestSupport::requireRomFixture();

    StateReplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.frames = 180;
    options.replayHorizon = 12;
    options.extraReplayHorizons = {1, 4, 24, 48};
    options.extraSeeds = {1u, 0xDEADBEEFu, 0xCAFEBABEu};
    options.probeStride = 7;
    options.reportPath = GeraNESTestSupport::reportPath("state_replay_deep.json").string();

    REQUIRE(StateReplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("cases").size() >= 12);
}

TEST_CASE("State replay remains deterministic from a very late snapshot", "[state-replay][late]")
{
    GeraNESTestSupport::requireRomFixture();

    StateReplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.frames = 200;
    options.fromFrame = 160;
    options.replayHorizon = 32;
    options.extraReplayHorizons = {8, 16};
    options.extraSeeds = {0x24681357u};
    options.reportPath = GeraNESTestSupport::reportPath("state_replay_late.json").string();

    REQUIRE(StateReplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("fromFrame") == 160);
}
