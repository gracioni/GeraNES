#include <catch2/catch_test_macros.hpp>

#include <winsock2.h>
#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif

#include "NetplayTest.h"
#include "TestSupport.h"

TEST_CASE("Netplay auto settings probe behaves as expected", "[netplay][autosettings]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.autoSettingsProbe = true;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_auto_settings.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("probe") == "netplay_auto_settings");
}

TEST_CASE("Netplay runtime flow advances under prediction", "[netplay][runtime]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 40;
    options.inputDelayFrames = 0;
    options.predictFrames = 2;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_flow.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
    REQUIRE(report.at("targetHostFrame").get<uint32_t>() >= report.at("startHostFrame").get<uint32_t>());
    REQUIRE(report.at("targetClientFrame").get<uint32_t>() >= report.at("startClientFrame").get<uint32_t>());
}

TEST_CASE("Netplay robust matrix stays green", "[netplay][robust]")
{
    GeraNESTestSupport::requireRomFixture();

    const auto rom = GeraNESTestSupport::romPath();
    if(rom.filename() == "ppu_vbl_nmi.nes") {
        SKIP("The default PPU fixture ROM is good for deterministic smoke tests, but it does not reliably exercise the gameplay-heavy robust netplay matrix. Set GERANES_TEST_ROM to a gameplay ROM fixture to enable this test.");
    }

    NetplayTest::Options options;
    options.romPath = rom.string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 120;
    options.robust = true;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_robust.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("caseCount").get<std::size_t>() > 0);
}
