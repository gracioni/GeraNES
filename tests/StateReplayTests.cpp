#include <catch2/catch_test_macros.hpp>

#include "StateReplayTest.h"
#include "TestSupport.h"

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
