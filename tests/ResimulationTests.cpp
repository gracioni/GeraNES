#include <catch2/catch_test_macros.hpp>

#include "ResimulationTest.h"
#include "TestSupport.h"

TEST_CASE("Silent resimulation reaches the exact execution point", "[resimulation]")
{
    GeraNESTestSupport::requireRomFixture();

    ResimulationTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.rollbackFrame = 90;
    options.runtimeMsAfterRollback = 40;
    options.futureInputFrames = 20;
    options.reportPath = GeraNESTestSupport::reportPath("resimulation.json").string();

    REQUIRE(ResimulationTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("exactTargetReached") == true);
    REQUIRE(report.at("audioStayedSilent") == true);
}
