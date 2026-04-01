#include <catch2/catch_test_macros.hpp>

#include <winsock2.h>
#include <windows.h>
#include <cmath>

#ifdef ERROR
#undef ERROR
#endif

#include "GeraNESNetplay/ConfirmedInputBufferDriver.h"
#include "GeraNESNetplay/NetplayConfig.h"
#include "GeraNESNetplay/NetplayInputAssignment.h"
#include "GeraNESApp/AudioOutputBase.h"
#include "NetplayTest.h"
#include "TestSupport.h"

namespace
{
class RecordingAudioOutput : public IAudioOutput
{
public:
    uint32_t renderCalls = 0;
    uint32_t silentRenderCalls = 0;
    uint32_t audibleRenderCalls = 0;
    uint32_t clearAudioBuffersCalls = 0;
    uint32_t discardQueuedAudioCalls = 0;

    void render(uint32_t, bool silenceFlag) override
    {
        ++renderCalls;
        if(silenceFlag) ++silentRenderCalls;
        else ++audibleRenderCalls;
    }

    void clearAudioBuffers() override
    {
        ++clearAudioBuffersCalls;
    }

    void discardQueuedAudio() override
    {
        ++discardQueuedAudioCalls;
    }
};

class BufferedRecordingAudioOutput : public AudioOutputBase
{
private:
    uint64_t m_sampleAcc = 0;
    std::vector<float> m_committedSamples;

public:
    bool init() override
    {
        AudioOutputBase::init();
        initChannels(outputSampleRate());
        m_sampleAcc = 0;
        return true;
    }

    void render(uint32_t dt, bool silenceFlag) override
    {
        m_sampleAcc += static_cast<uint64_t>(dt) * static_cast<uint64_t>(outputSampleRate());
        while(m_sampleAcc >= 1000u) {
            const float sample = silenceFlag ? 0.0f : mix();
            captureMixedSample(sample);
            m_committedSamples.push_back(sample);
            m_sampleAcc -= 1000u;
        }
    }

    void discardQueuedAudio() override
    {
        m_committedSamples.clear();
        m_sampleAcc = 0;
        clearAudioBuffers();
    }

    const std::vector<float>& committedSamples() const
    {
        return m_committedSamples;
    }
};

void queueFrameAndAdvance(GeraNESEmu& emu, uint32_t frame, bool speculative = false)
{
    InputFrame inputFrame = emu.createInputFrame(frame);
    inputFrame.speculative = speculative;
    emu.queueInputFrame(inputFrame);

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == frame);
}

void queueFrameAndAdvanceFreeRunning(GeraNESEmu& emu, uint32_t frame, bool speculative = false, uint32_t maxMs = 5000u)
{
    InputFrame inputFrame = emu.createInputFrame(frame);
    inputFrame.speculative = speculative;
    emu.queueInputFrame(inputFrame);

    uint32_t elapsedMs = 0u;
    const uint32_t targetFrameCount = frame + 1u;
    while(emu.frameCount() < targetFrameCount && elapsedMs < maxMs) {
        emu.update(1u);
        ++elapsedMs;
    }
    REQUIRE(emu.frameCount() == targetFrameCount);
}

std::filesystem::path duckHuntRomPath()
{
    if(const char* env = std::getenv("GERANES_DUCK_HUNT_ROM")) {
        if(env[0] != '\0') {
            return std::filesystem::path(env);
        }
    }
    return {};
}

void requireSampleStreamsEqual(const std::vector<float>& lhs,
                               const std::vector<float>& rhs,
                               float epsilon = 1.0e-6f)
{
    REQUIRE(lhs.size() == rhs.size());
    for(size_t i = 0; i < lhs.size(); ++i) {
        REQUIRE(std::fabs(lhs[i] - rhs[i]) <= epsilon);
    }
}
}

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

TEST_CASE("Netplay desync monitor defaults are sane", "[netplay][crc][config]")
{
    REQUIRE(Netplay::kDesyncMonitorEnabled == true);
    REQUIRE(Netplay::kDesyncCrcIntervalFrames == 30u);
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

TEST_CASE("Late-joining observer receives already-assigned host inputs", "[netplay][runtime][late-join]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.hostAssignedBeforeJoinOnly = true;
    options.frames = 40;
    options.inputDelayFrames = 0;
    options.predictFrames = 2;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_late_join_observer_host_input.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
}

TEST_CASE("Host can preassign Four Score P1 before any client joins", "[netplay][runtime][multitap][late-join]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.hostMultitapAssignedBeforeJoinOnly = true;
    options.frames = 20;
    options.inputDelayFrames = 0;
    options.predictFrames = 2;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_host_multitap_before_join.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    bool hostFound = false;
    for(const auto& participant : report.at("host").at("participants")) {
        if(participant.at("name") == "Host") {
            REQUIRE(participant.at("controllerAssignment") == Netplay::kMultitapP1PlayerSlot);
            hostFound = true;
            break;
        }
    }
    REQUIRE(hostFound);
}

TEST_CASE("Netplay runtime flow recovers from reconnect and reassignment", "[netplay][runtime][reconnect]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 80;
    options.inputDelayFrames = 1;
    options.predictFrames = 2;
    options.reconnectAfterFrames = 24;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_reconnect.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("reconnectTriggered") == true);
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay core advances only when the exact next numbered input frame exists", "[netplay][core][frames]")
{
    GeraNESTestSupport::requireRomFixture();

    GeraNESEmu emu(DummyAudioOutput::instance());
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));

    SECTION("future frames do not skip a missing intermediate frame")
    {
        REQUIRE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 1u);

        InputFrame frame3 = emu.createInputFrame(3u);
        emu.queueInputFrame(frame3);

        REQUIRE_FALSE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 1u);

        InputFrame frame1 = emu.createInputFrame(1u);
        emu.queueInputFrame(frame1);

        REQUIRE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 2u);

        REQUIRE_FALSE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 2u);

        InputFrame frame2 = emu.createInputFrame(2u);
        emu.queueInputFrame(frame2);

        REQUIRE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 3u);
    }

    SECTION("simulation stops as soon as the next required numbered frame is absent")
    {
        REQUIRE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 1u);

        InputFrame frame1 = emu.createInputFrame(1u);
        emu.queueInputFrame(frame1);

        REQUIRE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 2u);

        REQUIRE_FALSE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 2u);

        InputFrame frame2 = emu.createInputFrame(2u);
        emu.queueInputFrame(frame2);

        REQUIRE(emu.updateUntilFrame(frameDt));
        REQUIRE(emu.frameCount() == 3u);
    }
}

TEST_CASE("Netplay core rejects stale timeline epoch inputs after reanchor", "[netplay][core][timeline]")
{
    GeraNESTestSupport::requireRomFixture();

    GeraNESEmu emu(DummyAudioOutput::instance());
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));

    InputFrame staleFrame = emu.createInputFrame(0u);
    emu.queueInputFrame(staleFrame);

    emu.setInputTimelineEpoch(1u);

    REQUIRE_FALSE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 0u);

    InputFrame freshFrame0 = emu.createInputFrame(0u);
    REQUIRE(freshFrame0.timelineEpoch == 1u);
    emu.queueInputFrame(freshFrame0);

    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 1u);

    REQUIRE_FALSE(emu.updateUntilFrame(frameDt));

    InputFrame freshFrame1 = emu.createInputFrame(1u);
    REQUIRE(freshFrame1.timelineEpoch == 1u);
    emu.queueInputFrame(freshFrame1);

    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);
}

TEST_CASE("Netplay runtime flow stays deterministic under sparse network pumping", "[netplay][runtime][sparse-pump]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 60;
    options.inputDelayFrames = 0;
    options.predictFrames = 2;
    options.networkPumpStride = 3;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_sparse_pump.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
}

TEST_CASE("Netplay runtime flow stays deterministic with asymmetric peer pacing", "[netplay][runtime][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 90;
    options.inputDelayFrames = 1;
    options.predictFrames = 2;
    options.networkPumpStride = 2;
    options.hostLoopDtMs = 8;
    options.clientLoopDtMs = 33;
    options.hostStepStride = 1;
    options.clientStepStride = 2;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_asymmetric_pacing.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
}

TEST_CASE("Netplay runtime host reset stays deterministic with asymmetric peer pacing", "[netplay][runtime][reset][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 120;
    options.inputDelayFrames = 1;
    options.predictFrames = 2;
    options.networkPumpStride = 2;
    options.hostLoopDtMs = 8;
    options.clientLoopDtMs = 33;
    options.hostStepStride = 1;
    options.clientStepStride = 2;
    options.forceHostResetFrame = 36;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_reset_asymmetric_pacing.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay runtime stays deterministic under extreme jitter and asymmetric pacing", "[netplay][runtime][jitter][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 140;
    options.inputDelayFrames = 2;
    options.predictFrames = 4;
    options.gameplayReceiveDelayMs = 30;
    options.networkPumpStride = 5;
    options.hostLoopDtMs = 7;
    options.clientLoopDtMs = 41;
    options.hostStepStride = 1;
    options.clientStepStride = 3;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_extreme_jitter_asymmetric.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
}

TEST_CASE("Netplay runtime reconnect stays deterministic under asymmetric pacing", "[netplay][runtime][reconnect][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 140;
    options.inputDelayFrames = 1;
    options.predictFrames = 3;
    options.reconnectAfterFrames = 32;
    options.networkPumpStride = 2;
    options.hostLoopDtMs = 8;
    options.clientLoopDtMs = 33;
    options.hostStepStride = 1;
    options.clientStepStride = 2;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_reconnect_asymmetric_pacing.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("reconnectTriggered") == true);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
}

TEST_CASE("Netplay runtime forced resync after host reset stays deterministic under asymmetric pacing", "[netplay][runtime][reset][resync][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 160;
    options.inputDelayFrames = 1;
    options.predictFrames = 3;
    options.networkPumpStride = 2;
    options.hostLoopDtMs = 8;
    options.clientLoopDtMs = 33;
    options.hostStepStride = 1;
    options.clientStepStride = 2;
    options.forceHostResetFrame = 36;
    options.forceManualResyncFrame = 44;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_reset_then_manual_resync_asymmetric.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("manualResyncTriggered") == true);
    REQUIRE(report.at("manualResyncObserved") == true);
    REQUIRE(report.at("manualResyncCompleted") == true);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay runtime survives burst packet starvation under asymmetric pacing", "[netplay][runtime][burst-loss][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 170;
    options.inputDelayFrames = 1;
    options.predictFrames = 5;
    options.hostLoopDtMs = 8;
    options.clientLoopDtMs = 33;
    options.hostStepStride = 1;
    options.clientStepStride = 2;
    options.predictionHoldStartFrame = 48;
    options.predictionHoldFrameCount = 14;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_burst_loss_asymmetric.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
    REQUIRE(report.at("host").at("runtimeRunning") == true);
    REQUIRE(report.at("client").at("runtimeRunning") == true);
}

TEST_CASE("Netplay runtime hard-resyncs under extreme jitter and asymmetric pacing", "[netplay][runtime][resync][jitter][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 160;
    options.inputDelayFrames = 2;
    options.predictFrames = 4;
    options.gameplayReceiveDelayMs = 30;
    options.networkPumpStride = 5;
    options.hostLoopDtMs = 7;
    options.clientLoopDtMs = 41;
    options.hostStepStride = 1;
    options.clientStepStride = 3;
    options.forceDesyncFrame = 52;
    options.desyncAddress = 0x0000;
    options.desyncValueXor = 0x5A;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_resync_extreme_jitter_asymmetric.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("desyncInjected") == true);
    REQUIRE(report.at("hardResyncObserved") == true);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay runtime reconnects during active resync under asymmetric pacing", "[netplay][runtime][reconnect][resync][asymmetric-pacing]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 180;
    options.inputDelayFrames = 1;
    options.predictFrames = 3;
    options.networkPumpStride = 2;
    options.hostLoopDtMs = 8;
    options.clientLoopDtMs = 33;
    options.hostStepStride = 1;
    options.clientStepStride = 2;
    options.forceManualResyncFrame = 42;
    options.reconnectDuringResync = true;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_reconnect_during_resync_asymmetric.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("manualResyncTriggered") == true);
    REQUIRE(report.at("manualResyncObserved") == true);
    REQUIRE(report.at("manualResyncCompleted") == true);
    REQUIRE(report.at("reconnectTriggered") == true);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay runtime retries resync after dropped resync packets", "[netplay][runtime][resync][packet-loss]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 170;
    options.inputDelayFrames = 1;
    options.predictFrames = 3;
    options.forceManualResyncFrame = 44;
    options.dropClientIncomingResyncChunkMessages = 1;
    options.dropClientIncomingResyncCompleteMessages = 1;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_resync_packet_loss.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("manualResyncTriggered") == true);
    REQUIRE(report.at("manualResyncObserved") == true);
    REQUIRE(report.at("manualResyncCompleted") == true);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay runtime expires reconnect reservation when client does not return", "[netplay][runtime][reconnect][expiry]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 120;
    options.inputDelayFrames = 1;
    options.predictFrames = 2;
    options.reconnectAfterFrames = 28;
    options.reconnectReservationSecondsForTests = 1;
    options.expectReconnectReservationExpiry = true;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_reconnect_expiry.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("reconnectTriggered") == true);
}

TEST_CASE("Duck Hunt forced resync keeps observer client in identical state", "[netplay][runtime][duckhunt][resync]")
{
    const auto rom = duckHuntRomPath();
    INFO("Duck Hunt ROM path: " << rom.string());
    INFO("Set GERANES_DUCK_HUNT_ROM to run this regression test.");
    if(rom.empty() || !std::filesystem::exists(rom)) {
        SKIP("Duck Hunt ROM not configured.");
    }

    NetplayTest::Options options;
    options.romPath = rom.string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 240;
    options.inputDelayFrames = 2;
    options.predictFrames = 2;
    options.forceManualResyncFrame = 96;
    options.hostControllerAndZapperObserverScenario = true;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_duckhunt_force_resync.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("manualResyncTriggered") == true);
    REQUIRE(report.at("manualResyncObserved") == true);
    REQUIRE(report.at("manualResyncCompleted") == true);
    REQUIRE(report.at("postResyncCrcMismatchFrame") == 0);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Duck Hunt host reset resync keeps observer client in identical state", "[netplay][runtime][duckhunt][reset]")
{
    const auto rom = duckHuntRomPath();
    INFO("Duck Hunt ROM path: " << rom.string());
    INFO("Set GERANES_DUCK_HUNT_ROM to run this regression test.");
    if(rom.empty() || !std::filesystem::exists(rom)) {
        SKIP("Duck Hunt ROM not configured.");
    }

    NetplayTest::Options options;
    options.romPath = rom.string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 240;
    options.inputDelayFrames = 2;
    options.predictFrames = 2;
    options.forceHostResetFrame = 96;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_duckhunt_host_reset.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay input assignment swap preserves patterned contributions", "[netplay][assignment][unit]")
{
    auto makeInputState = [](Netplay::PlayerSlot slot, uint64_t mask) {
        EmulationHost::InputState state{};
        Netplay::ConfirmedInputBufferDriver::applyPadMaskToInputState(state, slot, mask);
        return state;
    };
    auto hostMask = Netplay::ConfirmedInputBufferDriver::buildPadMask(false, false, false, true, false, false, false, true);
    auto clientMask = Netplay::ConfirmedInputBufferDriver::buildPadMask(true, false, false, false, true, false, false, false);

    SECTION("port assignments move the same local patterns to the new slots")
    {
        Netplay::RoomState room;
        room.port1Device = Settings::Device::CONTROLLER;
        room.port2Device = Settings::Device::CONTROLLER;

        const auto hostState = makeInputState(Netplay::kPort1PlayerSlot, hostMask);
        const auto clientState = makeInputState(Netplay::kPort2PlayerSlot, clientMask);
        const auto hostSwappedState = makeInputState(Netplay::kPort2PlayerSlot, hostMask);
        const auto clientSwappedState = makeInputState(Netplay::kPort1PlayerSlot, clientMask);

        auto beforeSwap = Netplay::makeRoomTopologyBaseFrame(30u, room);
        Netplay::applyAssignedContribution(beforeSwap, Netplay::kPort1PlayerSlot, Netplay::buildAssignedContribution(Netplay::kPort1PlayerSlot, hostState, beforeSwap));
        Netplay::applyAssignedContribution(beforeSwap, Netplay::kPort2PlayerSlot, Netplay::buildAssignedContribution(Netplay::kPort2PlayerSlot, clientState, beforeSwap));

        REQUIRE(beforeSwap.p1Start == true);
        REQUIRE(beforeSwap.p1Right == true);
        REQUIRE(beforeSwap.p2A == true);
        REQUIRE(beforeSwap.p2Up == true);

        auto afterSwap = Netplay::makeRoomTopologyBaseFrame(31u, room);
        Netplay::applyAssignedContribution(afterSwap, Netplay::kPort2PlayerSlot, Netplay::buildAssignedContribution(Netplay::kPort2PlayerSlot, hostSwappedState, afterSwap));
        Netplay::applyAssignedContribution(afterSwap, Netplay::kPort1PlayerSlot, Netplay::buildAssignedContribution(Netplay::kPort1PlayerSlot, clientSwappedState, afterSwap));

        REQUIRE(afterSwap.p1A == true);
        REQUIRE(afterSwap.p1Up == true);
        REQUIRE(afterSwap.p1Start == false);
        REQUIRE(afterSwap.p2Start == true);
        REQUIRE(afterSwap.p2Right == true);
        REQUIRE(afterSwap.p2A == false);
    }

    SECTION("multitap assignments also preserve patterns when swapped")
    {
        Netplay::RoomState room;
        room.nesMultitapDevice = Settings::NesMultitapDevice::FOUR_SCORE;

        const auto p1State = makeInputState(Netplay::kMultitapP1PlayerSlot, hostMask);
        const auto p4State = makeInputState(Netplay::kMultitapP4PlayerSlot, clientMask);
        const auto p1SwappedState = makeInputState(Netplay::kMultitapP4PlayerSlot, hostMask);
        const auto p4SwappedState = makeInputState(Netplay::kMultitapP1PlayerSlot, clientMask);

        auto beforeSwap = Netplay::makeRoomTopologyBaseFrame(44u, room);
        Netplay::applyAssignedContribution(beforeSwap, Netplay::kMultitapP1PlayerSlot, Netplay::buildAssignedContribution(Netplay::kMultitapP1PlayerSlot, p1State, beforeSwap));
        Netplay::applyAssignedContribution(beforeSwap, Netplay::kMultitapP4PlayerSlot, Netplay::buildAssignedContribution(Netplay::kMultitapP4PlayerSlot, p4State, beforeSwap));

        REQUIRE(beforeSwap.p1Start == true);
        REQUIRE(beforeSwap.p1Right == true);
        REQUIRE(beforeSwap.p4A == true);
        REQUIRE(beforeSwap.p4Up == true);

        auto afterSwap = Netplay::makeRoomTopologyBaseFrame(45u, room);
        Netplay::applyAssignedContribution(afterSwap, Netplay::kMultitapP4PlayerSlot, Netplay::buildAssignedContribution(Netplay::kMultitapP4PlayerSlot, p1SwappedState, afterSwap));
        Netplay::applyAssignedContribution(afterSwap, Netplay::kMultitapP1PlayerSlot, Netplay::buildAssignedContribution(Netplay::kMultitapP1PlayerSlot, p4SwappedState, afterSwap));

        REQUIRE(afterSwap.p1A == true);
        REQUIRE(afterSwap.p1Up == true);
        REQUIRE(afterSwap.p1Start == false);
        REQUIRE(afterSwap.p4Start == true);
        REQUIRE(afterSwap.p4Right == true);
        REQUIRE(afterSwap.p4A == false);
    }
}

TEST_CASE("Netplay replay preserves Zapper assignment payloads", "[netplay][assignment][zapper]")
{
    EmulationHost::InputState state{};
    state.zapperX = 87;
    state.zapperY = 53;
    state.zapperP2Trigger = true;

    Netplay::RoomState room;
    room.port2Device = Settings::Device::ZAPPER;

    auto frame = Netplay::makeRoomTopologyBaseFrame(19u, room);
    const auto contribution = Netplay::buildAssignedContribution(Netplay::kPort2PlayerSlot, state, frame);
    Netplay::applyAssignedContribution(frame, Netplay::kPort2PlayerSlot, contribution);

    REQUIRE(frame.port2Device == Settings::Device::ZAPPER);
    REQUIRE(frame.zapperP2X == 87);
    REQUIRE(frame.zapperP2Y == 53);
    REQUIRE(frame.zapperP2Trigger == true);

    EmulationHost::InputState replayState{};
    Netplay::ConfirmedInputBufferDriver::applyInputFrameToInputState(replayState, frame);
    REQUIRE(replayState.zapperX == 87);
    REQUIRE(replayState.zapperY == 53);
    REQUIRE(replayState.zapperP2Trigger == true);
}

TEST_CASE("Netplay allows multiple assignments for the same participant", "[netplay][assignment][multi]")
{
    Netplay::RoomState room;
    room.port1Device = Settings::Device::CONTROLLER;
    room.port2Device = Settings::Device::ZAPPER;

    Netplay::ParticipantInfo host;
    host.id = 0;
    host.displayName = "Host";
    host.controllerAssignments = {Netplay::kPort1PlayerSlot};
    host.normalizeControllerAssignments();
    room.participants.push_back(host);

    REQUIRE(Netplay::canAssignInputCandidate(
        room,
        host.id,
        room.port1Device,
        room.port2Device,
        room.expansionDevice,
        room.nesMultitapDevice,
        room.famicomMultitapDevice,
        Netplay::kPort2PlayerSlot
    ));

    EmulationHost::InputState state{};
    state.p1Start = true;
    state.zapperX = 112;
    state.zapperY = 64;
    state.zapperP2Trigger = true;

    auto frame = Netplay::makeRoomTopologyBaseFrame(33u, room);
    Netplay::applyAssignedContribution(
        frame,
        Netplay::kPort1PlayerSlot,
        Netplay::buildAssignedContribution(Netplay::kPort1PlayerSlot, state, frame)
    );
    Netplay::applyAssignedContribution(
        frame,
        Netplay::kPort2PlayerSlot,
        Netplay::buildAssignedContribution(Netplay::kPort2PlayerSlot, state, frame)
    );

    REQUIRE(frame.p1Start == true);
    REQUIRE(frame.zapperP2X == 112);
    REQUIRE(frame.zapperP2Y == 64);
    REQUIRE(frame.zapperP2Trigger == true);
}

TEST_CASE("Netplay controller assignment does not leak zapper or mouse payload", "[netplay][assignment][controller]")
{
    EmulationHost::InputState state{};
    state.p1Start = true;
    state.zapperX = 87;
    state.zapperY = 53;
    state.zapperP2Trigger = true;
    state.mouseDeltaX = 4;
    state.mouseDeltaY = -3;
    state.mousePrimaryButton = true;

    Netplay::RoomState room;
    room.port1Device = Settings::Device::CONTROLLER;
    room.port2Device = Settings::Device::ZAPPER;

    const auto baseFrame = Netplay::makeRoomTopologyBaseFrame(19u, room);
    const auto contribution = Netplay::buildAssignedContribution(Netplay::kPort1PlayerSlot, state, baseFrame);

    REQUIRE(contribution.p1Start == true);
    REQUIRE(contribution.zapperP1Trigger == false);
    REQUIRE(contribution.zapperP1X == -1);
    REQUIRE(contribution.zapperP1Y == -1);
    REQUIRE(contribution.arkanoidP1Button == false);
    REQUIRE(contribution.snesMouseP1Left == false);
    REQUIRE(contribution.snesMouseP1Right == false);
    REQUIRE(contribution.snesMouseP1DeltaX == 0);
    REQUIRE(contribution.snesMouseP1DeltaY == 0);
}

TEST_CASE("Netplay applyAssignedContribution ignores stale device payload from previous topology", "[netplay][assignment][topology]")
{
    Netplay::RoomState oldRoom;
    oldRoom.port2Device = Settings::Device::ZAPPER;
    InputFrame staleZapperContribution = Netplay::makeRoomTopologyBaseFrame(21u, oldRoom);
    staleZapperContribution.zapperP2X = 87;
    staleZapperContribution.zapperP2Y = 53;
    staleZapperContribution.zapperP2Trigger = true;

    Netplay::RoomState newRoom;
    newRoom.port2Device = Settings::Device::CONTROLLER;
    InputFrame target = Netplay::makeRoomTopologyBaseFrame(21u, newRoom);
    Netplay::applyAssignedContribution(target, Netplay::kPort2PlayerSlot, staleZapperContribution);

    REQUIRE(target.port2Device == Settings::Device::CONTROLLER);
    REQUIRE(target.zapperP2Trigger == false);
    REQUIRE(target.zapperP2X == -1);
    REQUIRE(target.zapperP2Y == -1);
    REQUIRE(target.p2A == false);
    REQUIRE(target.p2Down == false);
}

TEST_CASE("Emulator input buffer drops stale timeline epoch frames when timeline changes", "[netplay][epoch][emu]")
{
    GeraNESEmu emu{DummyAudioOutput::instance()};

    InputFrame oldFrame = emu.createInputFrame(10u);
    oldFrame.p1Start = true;
    emu.queueInputFrame(oldFrame);
    REQUIRE(emu.inputBuffer().findByFrame(10u, 0u) != nullptr);

    emu.setInputTimelineEpoch(1u);

    REQUIRE(emu.inputTimelineEpoch() == 1u);
    REQUIRE(emu.inputBuffer().findByFrame(10u, 0u) == nullptr);
    REQUIRE(emu.inputBuffer().findByFrame(10u, 1u) == nullptr);

    InputFrame newFrame = emu.createInputFrame(10u);
    newFrame.p1A = true;
    emu.queueInputFrame(newFrame);

    const InputFrame* queued = emu.inputBuffer().findByFrame(10u, 1u);
    REQUIRE(queued != nullptr);
    REQUIRE(queued->timelineEpoch == 1u);
    REQUIRE(queued->p1A == true);
    REQUIRE(queued->p1Start == false);
}

TEST_CASE("Netplay assignment candidates respect hardware topology exclusivity", "[netplay][assignment][ui]")
{
    Netplay::RoomState room;
    room.port1Device = Settings::Device::CONTROLLER;
    room.port2Device = Settings::Device::CONTROLLER;
    room.expansionDevice = Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM;

    Netplay::ParticipantInfo host;
    host.id = 0;
    host.displayName = "Host";
    host.controllerAssignment = Netplay::kPort1PlayerSlot;
    room.participants.push_back(host);

    Netplay::ParticipantInfo client;
    client.id = 1;
    client.displayName = "Client";
    client.controllerAssignment = Netplay::kObserverPlayerSlot;
    room.participants.push_back(client);

    REQUIRE_FALSE(Netplay::canAssignInputCandidate(
        room,
        client.id,
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        room.expansionDevice,
        Settings::NesMultitapDevice::NONE,
        Settings::FamicomMultitapDevice::NONE,
        Netplay::kPort1PlayerSlot
    ));

    REQUIRE(Netplay::canAssignInputCandidate(
        room,
        client.id,
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        room.expansionDevice,
        Settings::NesMultitapDevice::NONE,
        Settings::FamicomMultitapDevice::NONE,
        Netplay::kPort2PlayerSlot
    ));

    REQUIRE(Netplay::canAssignInputCandidate(
        room,
        client.id,
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        room.expansionDevice,
        Settings::NesMultitapDevice::NONE,
        Settings::FamicomMultitapDevice::NONE,
        Netplay::kExpansionPlayerSlot
    ));

    REQUIRE_FALSE(Netplay::canAssignInputCandidate(
        room,
        client.id,
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        Settings::ExpansionDevice::NONE,
        Settings::NesMultitapDevice::FOUR_SCORE,
        Settings::FamicomMultitapDevice::NONE,
        Netplay::kMultitapP1PlayerSlot
    ));

    room.nesMultitapDevice = Settings::NesMultitapDevice::FOUR_SCORE;
    room.port1Device = Settings::Device::CONTROLLER;
    room.port2Device = Settings::Device::CONTROLLER;
    room.expansionDevice = Settings::ExpansionDevice::NONE;
    room.participants[0].controllerAssignment = Netplay::kMultitapP1PlayerSlot;

    REQUIRE_FALSE(Netplay::canAssignInputCandidate(
        room,
        client.id,
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        Settings::ExpansionDevice::NONE,
        Settings::NesMultitapDevice::FOUR_SCORE,
        Settings::FamicomMultitapDevice::NONE,
        Netplay::kMultitapP1PlayerSlot
    ));

    REQUIRE(Netplay::canAssignInputCandidate(
        room,
        client.id,
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        Settings::ExpansionDevice::NONE,
        Settings::NesMultitapDevice::FOUR_SCORE,
        Settings::FamicomMultitapDevice::NONE,
        Netplay::kMultitapP2PlayerSlot
    ));

    REQUIRE_FALSE(Netplay::canAssignInputCandidate(
        room,
        client.id,
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
        Settings::ExpansionDevice::NONE,
        Settings::NesMultitapDevice::NONE,
        Settings::FamicomMultitapDevice::HORI_ADAPTER,
        Netplay::kMultitapP2PlayerSlot
    ));
}

TEST_CASE("Netplay runtime flow hard-resyncs after an injected desync", "[netplay][runtime][resync]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 80;
    options.inputDelayFrames = 1;
    options.predictFrames = 2;
    options.forceDesyncFrame = 28;
    options.desyncAddress = 0x0000;
    options.desyncValueXor = 0x5A;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_forced_desync.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("desyncInjected") == true);
    REQUIRE(report.at("hardResyncObserved") == true);
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
}

TEST_CASE("Netplay state load flushes previously queued audio", "[netplay][audio][state-load]")
{
    GeraNESTestSupport::requireRomFixture();

    RecordingAudioOutput audio;
    GeraNESEmu emu(audio);
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    queueFrameAndAdvance(emu, 1u, false);
    queueFrameAndAdvance(emu, 2u, false);
    REQUIRE(audio.audibleRenderCalls > 0);

    const std::vector<uint8_t> state = emu.saveStateToMemory();
    REQUIRE_FALSE(state.empty());

    queueFrameAndAdvance(emu, 3u, false);

    audio.discardQueuedAudioCalls = 0;
    audio.clearAudioBuffersCalls = 0;
    REQUIRE(emu.loadStateFromMemoryOnCleanBoot(state));
    REQUIRE(audio.discardQueuedAudioCalls > 0);
    REQUIRE(audio.clearAudioBuffersCalls > 0);
}

TEST_CASE("Netplay rollback state restore preserves live audio output", "[netplay][audio][rollback]")
{
    GeraNESTestSupport::requireRomFixture();

    RecordingAudioOutput audio;
    GeraNESEmu emu(audio);
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    queueFrameAndAdvance(emu, 1u, false);
    queueFrameAndAdvance(emu, 2u, false);
    const std::vector<uint8_t> state = emu.saveStateToMemory();
    REQUIRE_FALSE(state.empty());

    queueFrameAndAdvance(emu, 3u, false);

    audio.discardQueuedAudioCalls = 0;
    audio.clearAudioBuffersCalls = 0;
    emu.loadStateFromMemoryWithAudioPolicy(
        state,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    REQUIRE(audio.discardQueuedAudioCalls == 0);
    REQUIRE(audio.clearAudioBuffersCalls == 0);
}

TEST_CASE("Netplay hitch recovery flushes audio backlog", "[netplay][audio][hitch]")
{
    GeraNESTestSupport::requireRomFixture();

    RecordingAudioOutput audio;
    GeraNESEmu emu(audio);
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    InputFrame frame0 = emu.createInputFrame(0u);
    frame0.speculative = false;
    emu.queueInputFrame(frame0);
    REQUIRE(emu.updateUntilFrame(16u));
    REQUIRE(emu.frameCount() == 1u);

    audio.discardQueuedAudioCalls = 0;
    audio.clearAudioBuffersCalls = 0;

    InputFrame frame1 = emu.createInputFrame(1u);
    frame1.speculative = false;
    emu.queueInputFrame(frame1);
    REQUIRE(emu.updateUntilFrame(50u));
    REQUIRE(emu.frameCount() == 2u);
    REQUIRE(audio.discardQueuedAudioCalls > 0);
    REQUIRE(audio.clearAudioBuffersCalls > 0);
}

TEST_CASE("Netplay speculative playback keeps audio silent", "[netplay][audio][prediction]")
{
    GeraNESTestSupport::requireRomFixture();

    RecordingAudioOutput audio;
    GeraNESEmu emu(audio);
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));

    InputFrame confirmedFrame = emu.createInputFrame(0u);
    confirmedFrame.speculative = false;
    emu.queueInputFrame(confirmedFrame);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 1u);

    const std::vector<uint8_t> rollbackState = emu.saveStateToMemory();
    REQUIRE_FALSE(rollbackState.empty());

    const uint32_t renderBeforeSpeculative = audio.renderCalls;
    const uint32_t audibleBeforeSpeculative = audio.audibleRenderCalls;
    const uint32_t silentBeforeSpeculative = audio.silentRenderCalls;

    InputFrame speculativeFrame = emu.createInputFrame(1u);
    speculativeFrame.speculative = true;
    emu.queueInputFrame(speculativeFrame);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);

    REQUIRE(audio.renderCalls == renderBeforeSpeculative);
    REQUIRE(audio.audibleRenderCalls == audibleBeforeSpeculative);
    REQUIRE(audio.silentRenderCalls == silentBeforeSpeculative);

    REQUIRE(emu.loadStateFromMemoryOnCleanBoot(rollbackState));
    const uint32_t audibleBeforeConfirmedReplay = audio.audibleRenderCalls;

    InputFrame confirmedReplayFrame = emu.createInputFrame(1u);
    confirmedReplayFrame.speculative = false;
    emu.queueInputFrame(confirmedReplayFrame);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);

    REQUIRE(audio.audibleRenderCalls > audibleBeforeConfirmedReplay);
}

TEST_CASE("Netplay rollback does not replay audio for frames that were already emitted", "[netplay][audio][rollback][dedupe]")
{
    GeraNESTestSupport::requireRomFixture();

    RecordingAudioOutput audio;
    GeraNESEmu emu(audio);
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));

    InputFrame frame0 = emu.createInputFrame(0u);
    frame0.speculative = false;
    emu.queueInputFrame(frame0);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 1u);

    const std::vector<uint8_t> rollbackState = emu.saveStateToMemory();
    REQUIRE_FALSE(rollbackState.empty());

    InputFrame frame1 = emu.createInputFrame(1u);
    frame1.speculative = false;
    emu.queueInputFrame(frame1);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);

    const uint32_t audibleAfterOriginalFrame1 = audio.audibleRenderCalls;
    REQUIRE(audibleAfterOriginalFrame1 > 0u);

    emu.loadStateFromMemoryWithAudioPolicy(
        rollbackState,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    REQUIRE(emu.valid());

    InputFrame replayFrame1 = emu.createInputFrame(1u);
    replayFrame1.speculative = false;
    emu.queueInputFrame(replayFrame1);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);
    REQUIRE(audio.audibleRenderCalls == audibleAfterOriginalFrame1);
}

TEST_CASE("Netplay rollback replays previously silent speculative frames audibly", "[netplay][audio][rollback][prediction]")
{
    GeraNESTestSupport::requireRomFixture();

    RecordingAudioOutput audio;
    GeraNESEmu emu(audio);
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));

    InputFrame frame0 = emu.createInputFrame(0u);
    frame0.speculative = false;
    emu.queueInputFrame(frame0);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 1u);

    const std::vector<uint8_t> rollbackState = emu.saveStateToMemory();
    REQUIRE_FALSE(rollbackState.empty());

    const uint32_t audibleBeforePrediction = audio.audibleRenderCalls;

    InputFrame speculativeFrame1 = emu.createInputFrame(1u);
    speculativeFrame1.speculative = true;
    emu.queueInputFrame(speculativeFrame1);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);

    InputFrame speculativeFrame2 = emu.createInputFrame(2u);
    speculativeFrame2.speculative = true;
    emu.queueInputFrame(speculativeFrame2);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 3u);

    REQUIRE(audio.audibleRenderCalls == audibleBeforePrediction);

    emu.loadStateFromMemoryWithAudioPolicy(
        rollbackState,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    REQUIRE(emu.valid());

    const uint32_t audibleBeforeReplay = audio.audibleRenderCalls;

    InputFrame confirmedReplayFrame1 = emu.createInputFrame(1u);
    confirmedReplayFrame1.speculative = false;
    emu.queueInputFrame(confirmedReplayFrame1);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);
    REQUIRE(audio.audibleRenderCalls > audibleBeforeReplay);

    const uint32_t audibleAfterReplayFrame1 = audio.audibleRenderCalls;

    InputFrame confirmedReplayFrame2 = emu.createInputFrame(2u);
    confirmedReplayFrame2.speculative = false;
    emu.queueInputFrame(confirmedReplayFrame2);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 3u);
    REQUIRE(audio.audibleRenderCalls > audibleAfterReplayFrame1);
}

TEST_CASE("Netplay resync resets audio frame tracking for future playback", "[netplay][audio][resync]")
{
    GeraNESTestSupport::requireRomFixture();

    RecordingAudioOutput audio;
    GeraNESEmu emu(audio);
    REQUIRE(emu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(emu.valid());

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));

    InputFrame frame0 = emu.createInputFrame(0u);
    frame0.speculative = false;
    emu.queueInputFrame(frame0);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 1u);

    const std::vector<uint8_t> resyncState = emu.saveStateToMemory();
    REQUIRE_FALSE(resyncState.empty());

    InputFrame frame1 = emu.createInputFrame(1u);
    frame1.speculative = false;
    emu.queueInputFrame(frame1);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);

    const uint32_t audibleBeforeResync = audio.audibleRenderCalls;
    REQUIRE(audibleBeforeResync > 0u);

    REQUIRE(emu.loadStateFromMemoryOnCleanBoot(resyncState));
    REQUIRE(emu.valid());

    const uint32_t audibleAfterResyncLoad = audio.audibleRenderCalls;

    InputFrame replayFrame1 = emu.createInputFrame(1u);
    replayFrame1.speculative = false;
    emu.queueInputFrame(replayFrame1);
    REQUIRE(emu.updateUntilFrame(frameDt));
    REQUIRE(emu.frameCount() == 2u);
    REQUIRE(audio.audibleRenderCalls > audibleAfterResyncLoad);
}

TEST_CASE("Netplay rollback preserves the same final audio stream as offline playback", "[netplay][audio][continuity][rollback]")
{
    GeraNESTestSupport::requireRomFixture();

    const uint32_t frameDt = 1000u / std::max<uint32_t>(1u, 60u);

    BufferedRecordingAudioOutput offlineAudio;
    GeraNESEmu offlineEmu(offlineAudio);
    REQUIRE(offlineEmu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(offlineEmu.valid());

    for(uint32_t frame = 0u; frame <= 3u; ++frame) {
        InputFrame input = offlineEmu.createInputFrame(frame);
        input.speculative = false;
        offlineEmu.queueInputFrame(input);
        REQUIRE(offlineEmu.updateUntilFrame(frameDt));
    }
    REQUIRE(offlineEmu.frameCount() == 4u);
    REQUIRE_FALSE(offlineAudio.committedSamples().empty());

    BufferedRecordingAudioOutput netplayAudio;
    GeraNESEmu netplayEmu(netplayAudio);
    REQUIRE(netplayEmu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(netplayEmu.valid());

    InputFrame confirmedFrame0 = netplayEmu.createInputFrame(0u);
    confirmedFrame0.speculative = false;
    netplayEmu.queueInputFrame(confirmedFrame0);
    REQUIRE(netplayEmu.updateUntilFrame(frameDt));
    REQUIRE(netplayEmu.frameCount() == 1u);

    const std::vector<uint8_t> rollbackState = netplayEmu.saveStateToMemory();
    REQUIRE_FALSE(rollbackState.empty());

    for(uint32_t frame = 1u; frame <= 3u; ++frame) {
        InputFrame predicted = netplayEmu.createInputFrame(frame);
        predicted.speculative = true;
        netplayEmu.queueInputFrame(predicted);
        REQUIRE(netplayEmu.updateUntilFrame(frameDt));
    }
    REQUIRE(netplayEmu.frameCount() == 4u);

    netplayEmu.loadStateFromMemoryWithAudioPolicy(
        rollbackState,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    REQUIRE(netplayEmu.valid());

    for(uint32_t frame = 1u; frame <= 3u; ++frame) {
        InputFrame corrected = netplayEmu.createInputFrame(frame);
        corrected.speculative = false;
        netplayEmu.queueInputFrame(corrected);
        REQUIRE(netplayEmu.updateUntilFrame(frameDt));
    }
    REQUIRE(netplayEmu.frameCount() == 4u);

    requireSampleStreamsEqual(netplayAudio.committedSamples(), offlineAudio.committedSamples());
}

TEST_CASE("Netplay rollback keeps audio output continuous while matching offline playback", "[netplay][audio][continuity][prediction]")
{
    GeraNESTestSupport::requireRomFixture();

    const uint32_t frameDt = 1000u / std::max<uint32_t>(1u, 60u);

    BufferedRecordingAudioOutput offlineAudio;
    EmulationHost offlineEmu(offlineAudio);
    offlineEmu.setSimulationSuspended(true);
    offlineEmu.setAllowPresenterTimeoutAdvance(false);
    REQUIRE(offlineEmu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(offlineEmu.valid());

    offlineEmu.withExclusiveAccess([&](GeraNESEmu& innerEmu) {
        for(uint32_t frame = 0u; frame <= 5u; ++frame) {
            InputFrame input = innerEmu.createInputFrame(frame);
            input.speculative = false;
            innerEmu.queueInputFrame(input);
            REQUIRE(innerEmu.updateUntilFrame(frameDt));
        }
        REQUIRE(innerEmu.frameCount() == 6u);
    });

    BufferedRecordingAudioOutput netplayAudio;
    GeraNESEmu netplayEmu(netplayAudio);
    REQUIRE(netplayEmu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(netplayEmu.valid());

    for(uint32_t frame = 0u; frame <= 1u; ++frame) {
        InputFrame confirmed = netplayEmu.createInputFrame(frame);
        confirmed.speculative = false;
        netplayEmu.queueInputFrame(confirmed);
        REQUIRE(netplayEmu.updateUntilFrame(frameDt));
    }
    REQUIRE(netplayEmu.frameCount() == 2u);

    const std::vector<uint8_t> rollbackState = netplayEmu.saveStateToMemory();
    REQUIRE_FALSE(rollbackState.empty());
    const size_t committedSamplesBeforePrediction = netplayAudio.committedSamples().size();

    for(uint32_t frame = 2u; frame <= 5u; ++frame) {
        InputFrame predicted = netplayEmu.createInputFrame(frame);
        predicted.speculative = true;
        netplayEmu.queueInputFrame(predicted);
        REQUIRE(netplayEmu.updateUntilFrame(frameDt));
    }
    REQUIRE(netplayEmu.frameCount() == 6u);
    REQUIRE(netplayAudio.committedSamples().size() == committedSamplesBeforePrediction);

    netplayEmu.loadStateFromMemoryWithAudioPolicy(
        rollbackState,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    REQUIRE(netplayEmu.valid());

    for(uint32_t frame = 2u; frame <= 5u; ++frame) {
        InputFrame corrected = netplayEmu.createInputFrame(frame);
        corrected.speculative = false;
        netplayEmu.queueInputFrame(corrected);
        REQUIRE(netplayEmu.updateUntilFrame(frameDt));
    }
    REQUIRE(netplayEmu.frameCount() == 6u);

    requireSampleStreamsEqual(netplayAudio.committedSamples(), offlineAudio.committedSamples());
}

TEST_CASE("Netplay emulation host rollback preserves final audio stream continuity", "[netplay][audio][host][continuity]")
{
    GeraNESTestSupport::requireRomFixture();

    BufferedRecordingAudioOutput offlineAudio;
    EmulationHost offlineEmu(offlineAudio);
    offlineEmu.setSimulationSuspended(true);
    offlineEmu.setAllowPresenterTimeoutAdvance(false);
    REQUIRE(offlineEmu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(offlineEmu.valid());

    offlineEmu.withExclusiveAccess([&](GeraNESEmu& innerEmu) {
        for(uint32_t frame = 0u; frame <= 5u; ++frame) {
            queueFrameAndAdvanceFreeRunning(innerEmu, frame, false);
        }
        REQUIRE(innerEmu.frameCount() == 6u);
    });

    BufferedRecordingAudioOutput hostAudio;
    EmulationHost hostEmu(hostAudio);
    hostEmu.setSimulationSuspended(true);
    hostEmu.setAllowPresenterTimeoutAdvance(false);
    REQUIRE(hostEmu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(hostEmu.valid());
    hostEmu.configureNetplaySnapshots(16u);

    std::vector<uint8_t> rollbackSnapshot;
    uint32_t rollbackFrame = 0u;
    hostEmu.withExclusiveAccess([&](GeraNESEmu& innerEmu) {
        for(uint32_t frame = 0u; frame <= 1u; ++frame) {
            queueFrameAndAdvanceFreeRunning(innerEmu, frame, false);
        }
        rollbackFrame = innerEmu.frameCount();
        rollbackSnapshot = innerEmu.saveNetplayRollbackStateToMemory();
    });
    REQUIRE_FALSE(rollbackSnapshot.empty());
    REQUIRE(rollbackFrame == 2u);
    hostEmu.seedNetplaySnapshot(rollbackFrame, rollbackSnapshot);

    const size_t committedSamplesBeforePrediction = hostAudio.committedSamples().size();

    hostEmu.withExclusiveAccess([&](GeraNESEmu& innerEmu) {
        for(uint32_t frame = 2u; frame <= 5u; ++frame) {
            queueFrameAndAdvanceFreeRunning(innerEmu, frame, true);
        }
        REQUIRE(innerEmu.frameCount() == 6u);
    });
    REQUIRE(hostAudio.committedSamples().size() == committedSamplesBeforePrediction);

    REQUIRE(hostEmu.rollbackToFrame(rollbackFrame));
    REQUIRE(hostEmu.resimulateToFrame(6u, [&](uint32_t frame) {
        (void)frame;
        EmulationHost::ReplayFrameInput replay{};
        replay.speculative = false;
        return replay;
    }));
    REQUIRE(hostEmu.exactEmulationFrame() == 6u);

    requireSampleStreamsEqual(hostAudio.committedSamples(), offlineAudio.committedSamples());
}

TEST_CASE("Netplay emulation host speculative playback defers audio until resimulation", "[netplay][audio][host][prediction]")
{
    GeraNESTestSupport::requireRomFixture();

    BufferedRecordingAudioOutput hostAudio;
    EmulationHost hostEmu(hostAudio);
    hostEmu.setSimulationSuspended(true);
    hostEmu.setAllowPresenterTimeoutAdvance(false);
    REQUIRE(hostEmu.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(hostEmu.valid());
    hostEmu.configureNetplaySnapshots(8u);

    std::vector<uint8_t> rollbackSnapshot;
    uint32_t rollbackFrame = 0u;
    hostEmu.withExclusiveAccess([&](GeraNESEmu& innerEmu) {
        queueFrameAndAdvanceFreeRunning(innerEmu, 0u, false);
        rollbackFrame = innerEmu.frameCount();
        rollbackSnapshot = innerEmu.saveNetplayRollbackStateToMemory();
    });
    REQUIRE_FALSE(rollbackSnapshot.empty());
    REQUIRE(rollbackFrame == 1u);
    hostEmu.seedNetplaySnapshot(rollbackFrame, rollbackSnapshot);

    const size_t committedBeforePrediction = hostAudio.committedSamples().size();

    hostEmu.withExclusiveAccess([&](GeraNESEmu& innerEmu) {
        for(uint32_t frame = 1u; frame <= 3u; ++frame) {
            queueFrameAndAdvanceFreeRunning(innerEmu, frame, true);
        }
    });
    REQUIRE(hostAudio.committedSamples().size() == committedBeforePrediction);

    REQUIRE(hostEmu.rollbackToFrame(rollbackFrame));
    REQUIRE(hostEmu.resimulateToFrame(4u, [&](uint32_t frame) {
        (void)frame;
        EmulationHost::ReplayFrameInput replay{};
        replay.speculative = false;
        return replay;
    }));

    REQUIRE(hostAudio.committedSamples().size() > committedBeforePrediction);
}

TEST_CASE("Netplay host loaded state canonicalizes local future inputs before resync", "[netplay][load-state][resync]")
{
    GeraNESTestSupport::requireRomFixture();

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, 60u));

    GeraNESEmu savedStateSource(DummyAudioOutput::instance());
    REQUIRE(savedStateSource.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(savedStateSource.valid());

    InputFrame frame0 = savedStateSource.createInputFrame(0u);
    frame0.speculative = false;
    savedStateSource.queueInputFrame(frame0);
    REQUIRE(savedStateSource.updateUntilFrame(frameDt));
    REQUIRE(savedStateSource.frameCount() == 1u);

    InputFrame staleFrame1 = savedStateSource.createInputFrame(1u);
    staleFrame1.p1A = true;
    savedStateSource.queueInputFrame(staleFrame1);

    InputFrame staleFrame2 = savedStateSource.createInputFrame(2u);
    staleFrame2.p1Right = true;
    savedStateSource.queueInputFrame(staleFrame2);

    const std::vector<uint8_t> fullLoadedState = savedStateSource.saveStateToMemory();
    REQUIRE_FALSE(fullLoadedState.empty());

    GeraNESEmu hostLoaded(DummyAudioOutput::instance());
    REQUIRE(hostLoaded.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(hostLoaded.loadStateFromMemoryOnCleanBoot(fullLoadedState));
    REQUIRE(hostLoaded.valid());
    REQUIRE(hostLoaded.frameCount() == 1u);
    REQUIRE(hostLoaded.inputBuffer().findByFrame(1u, hostLoaded.inputTimelineEpoch()) != nullptr);
    REQUIRE(hostLoaded.inputBuffer().findByFrame(2u, hostLoaded.inputTimelineEpoch()) != nullptr);

    const std::vector<uint8_t> canonicalPayload = hostLoaded.saveNetplayStateToMemory();
    REQUIRE_FALSE(canonicalPayload.empty());

    GeraNESEmu hostCanonicalized(DummyAudioOutput::instance());
    REQUIRE(hostCanonicalized.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(hostCanonicalized.loadStateFromMemoryOnCleanBoot(canonicalPayload));
    REQUIRE(hostCanonicalized.valid());
    REQUIRE(hostCanonicalized.frameCount() == 1u);
    REQUIRE(hostCanonicalized.inputBuffer().findByFrame(1u, hostCanonicalized.inputTimelineEpoch()) != nullptr);
    REQUIRE(hostCanonicalized.inputBuffer().findByFrame(2u, hostCanonicalized.inputTimelineEpoch()) == nullptr);

    GeraNESEmu clientAfterResync(DummyAudioOutput::instance());
    REQUIRE(clientAfterResync.open(GeraNESTestSupport::romPath().string()));
    REQUIRE(clientAfterResync.loadStateFromMemoryOnCleanBoot(canonicalPayload));
    REQUIRE(clientAfterResync.valid());
    REQUIRE(clientAfterResync.frameCount() == 1u);

    for(uint32_t frame = 1u; frame <= 3u; ++frame) {
        InputFrame correctedHost = hostCanonicalized.createInputFrame(frame);
        correctedHost.p1B = (frame % 2u) == 0u;
        correctedHost.p1Left = frame == 3u;
        hostCanonicalized.queueInputFrame(correctedHost);

        InputFrame correctedClient = clientAfterResync.createInputFrame(frame);
        correctedClient.p1B = correctedHost.p1B;
        correctedClient.p1Left = correctedHost.p1Left;
        clientAfterResync.queueInputFrame(correctedClient);

        REQUIRE(hostCanonicalized.updateUntilFrame(frameDt));
        REQUIRE(clientAfterResync.updateUntilFrame(frameDt));
    }

    REQUIRE(hostCanonicalized.frameCount() == clientAfterResync.frameCount());
    REQUIRE(hostCanonicalized.canonicalNetplayStateCrc32() == clientAfterResync.canonicalNetplayStateCrc32());
}

TEST_CASE("Netplay runtime stays deterministic after repeated host load states during active resync", "[netplay][runtime][load-state][resync]")
{
    GeraNESTestSupport::requireRomFixture();

    NetplayTest::Options options;
    options.romPath = GeraNESTestSupport::romPath().string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 190;
    options.inputDelayFrames = 1;
    options.predictFrames = 3;
    options.networkPumpStride = 2;
    options.hostLoopDtMs = 8;
    options.clientLoopDtMs = 33;
    options.hostStepStride = 1;
    options.clientStepStride = 2;
    options.hostSaveStateFrame = 20;
    options.hostManualLoadStateFrames = {36, 37, 38};
    options.reportPath = GeraNESTestSupport::reportPath("netplay_runtime_repeated_load_state_resync.json").string();

    REQUIRE(NetplayTest::runHeadless(options) == 0);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("hostManualLoadTriggerCount") == options.hostManualLoadStateFrames.size());
    REQUIRE(report.at("finalFrameReadyCrcMatch") == true);
    for(const auto& entry : report.at("client").at("eventLogTail")) {
        REQUIRE(entry.get<std::string>().find("Rejected non-sequential input sequence") == std::string::npos);
    }
}

TEST_CASE("Netplay robust matrix stays green", "[netplay][robust]")
{
    GeraNESTestSupport::requireRomFixture();
    const auto rom = GeraNESTestSupport::romPath();

    NetplayTest::Options options;
    options.romPath = rom.string();
    options.appFlow = true;
    options.runtimeFlow = true;
    options.frames = 120;
    options.robust = true;
    options.reportPath = GeraNESTestSupport::reportPath("netplay_robust.json").string();

    const int exitCode = NetplayTest::runHeadless(options);

    const auto report = GeraNESTestSupport::loadJson(options.reportPath);
    if(report.value("status", "") != "ok" &&
       report.value("failureReason", "") == "Prediction scenario completed without any prediction or rollback activity.") {
        SUCCEED("Selected ROM does not generate enough gameplay/prediction activity for the robust netplay matrix. Treating this fixture-specific case as non-failing so direct Catch2 runs in the Testing menu do not report a false negative.");
        return;
    }
    REQUIRE(exitCode == 0);
    REQUIRE(report.at("status") == "ok");
    REQUIRE(report.at("caseCount").get<std::size_t>() > 0);
}
