#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <optional>
#include <thread>
#include <string>
#include <vector>

#include "GeraNESApp/PendingInputFrames.h"
#include "GeraNESApp/ReplayFile.h"
#include "GeraNESApp/ThreadedEmulationHost.h"
#include "StateReplayTest.h"
#include "TestSupport.h"

namespace
{
    namespace fs = std::filesystem;

    struct ByteDiff
    {
        size_t offset = 0;
        uint8_t expected = 0;
        uint8_t actual = 0;
    };

    uint32_t stateCrc32(const std::vector<uint8_t>& data)
    {
        return data.empty() ? 0u : Crc32::calc(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::vector<uint8_t> serializeInputFrame(const InputFrame& frame)
    {
        InputFrame copy = frame;
        Serialize s;
        copy.serialization(s);
        return s.takeData();
    }

    InputFrame makeReplayInputFrame(GeraNESEmu& emu, uint32_t frame, uint64_t mask)
    {
        InputFrame inputFrame = emu.createInputFrame(frame);
        inputFrame.state.setPortButtons(1, {
            (mask & (1ull << 0)) != 0,
            (mask & (1ull << 1)) != 0,
            (mask & (1ull << 2)) != 0,
            (mask & (1ull << 3)) != 0,
            (mask & (1ull << 4)) != 0,
            (mask & (1ull << 5)) != 0,
            (mask & (1ull << 6)) != 0,
            (mask & (1ull << 7)) != 0
        });
        return inputFrame;
    }

    std::optional<ByteDiff> firstByteDiff(const std::vector<uint8_t>& expected, const std::vector<uint8_t>& actual)
    {
        const size_t sharedSize = std::min(expected.size(), actual.size());
        for(size_t index = 0; index < sharedSize; ++index) {
            if(expected[index] != actual[index]) {
                return ByteDiff{index, expected[index], actual[index]};
            }
        }
        if(expected.size() != actual.size()) {
            const size_t offset = sharedSize;
            return ByteDiff{
                offset,
                offset < expected.size() ? expected[offset] : 0u,
                offset < actual.size() ? actual[offset] : 0u
            };
        }
        return std::nullopt;
    }

    bool queueInputMaskForCurrentFrame(GeraNESEmu& emu, uint64_t mask)
    {
        InputFrame frame = makeReplayInputFrame(emu, emu.frameCount(), mask);
        return emu.setPlaybackInputFrame(frame);
    }

    bool advanceExactlyOneFrame(GeraNESEmu& emu, uint64_t inputMask)
    {
        if(!queueInputMaskForCurrentFrame(emu, inputMask)) {
            return false;
        }

        const uint32_t frameBefore = emu.frameCount();
        const uint32_t frameDtMs = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
        (void)emu.updateUntilFrame(frameDtMs, false);
        return emu.valid() && emu.frameCount() == frameBefore + 1u;
    }

    uint64_t deterministicReplayMask(uint32_t frame)
    {
        const uint32_t mixed = (frame * 1103515245u) + 12345u;
        uint64_t mask = 0;
        if((mixed & 0x0001u) != 0u) mask |= (1ull << 0);
        if((mixed & 0x0002u) != 0u) mask |= (1ull << 1);
        if((mixed % 29u) == 0u) mask |= (1ull << 2);
        if((mixed % 37u) == 0u) mask |= (1ull << 3);
        if((mixed & 0x0010u) != 0u) mask |= (1ull << 4);
        if((mixed & 0x0020u) != 0u) mask |= (1ull << 5);
        if((mixed & 0x0040u) != 0u) mask |= (1ull << 6);
        if((mixed & 0x0080u) != 0u) mask |= (1ull << 7);
        if((mask & (1ull << 4)) != 0u && (mask & (1ull << 5)) != 0u) mask &= ~(1ull << 5);
        if((mask & (1ull << 6)) != 0u && (mask & (1ull << 7)) != 0u) mask &= ~(1ull << 7);
        return mask;
    }

    fs::path locateReplayFixturePath()
    {
        static constexpr const char* kReplayName = "Mega Man 2 (U).replay";
        const std::array<fs::path, 5> candidates = {
            fs::path("build") / "replays" / kReplayName,
            fs::path("build") / "replay" / kReplayName,
            fs::path("replays") / kReplayName,
            fs::path("replay") / kReplayName,
            fs::path("..") / "build" / "replays" / kReplayName
        };

        for(const fs::path& candidate : candidates) {
            if(fs::exists(candidate)) {
                return candidate;
            }
        }
        return {};
    }

    bool advanceExactlyOneReplayFrame(GeraNESEmu& emu, const InputFrame& replayFrame)
    {
        if(!emu.setPlaybackInputFrame(replayFrame)) {
            return false;
        }

        const uint32_t frameBefore = emu.frameCount();
        const uint32_t frameDtMs = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
        (void)emu.updateUntilFrame(frameDtMs, false);
        return emu.valid() && emu.frameCount() == frameBefore + 1u;
    }

    bool waitForHostFrame(ThreadedEmulationHost& host, uint32_t targetFrame, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while(std::chrono::steady_clock::now() < deadline) {
            if(host.lastFrameReadyFrame() >= targetFrame) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return host.lastFrameReadyFrame() >= targetFrame;
    }
}

TEST_CASE("PendingInputFrames keeps queued frames addressable by frame number", "[state-replay][pending-input]")
{
    PendingInputFrames buffer;

    InputFrame later;
    later.frame = 11;
    buffer.set(later);

    InputFrame current;
    current.frame = 10;
    buffer.set(current);

    REQUIRE(buffer.find(10) != nullptr);
    REQUIRE(buffer.find(11) != nullptr);
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

TEST_CASE("State replay snapshots roundtrip byte-exact after dirty and clean-boot loads", "[state-replay][roundtrip]")
{
    GeraNESTestSupport::requireRomFixture();

    GeraNESEmu baseline(DummyAudioOutput::instance());
    REQUIRE(baseline.openRom(GeraNESTestSupport::romPath().string()));
    REQUIRE(baseline.valid());

    std::vector<std::vector<uint8_t>> snapshots;
    snapshots.push_back(baseline.saveStateToMemory());

    for(uint32_t frame = 0; frame < 80u; ++frame) {
        INFO("baseline frame " << frame);
        REQUIRE(advanceExactlyOneFrame(baseline, deterministicReplayMask(frame)));
        snapshots.push_back(baseline.saveStateToMemory());
    }

    const std::vector<uint32_t> probeFrames = {0u, 1u, 2u, 3u, 10u, 25u, 50u, 80u};
    for(const uint32_t probeFrame : probeFrames) {
        INFO("probeFrame=" << probeFrame);
        REQUIRE(probeFrame < snapshots.size());

        GeraNESEmu dirty(DummyAudioOutput::instance());
        REQUIRE(dirty.openRom(GeraNESTestSupport::romPath().string()));
        REQUIRE(dirty.valid());
        dirty.loadStateFromMemory(snapshots[probeFrame]);
        REQUIRE(dirty.valid());
        const std::vector<uint8_t> dirtyRoundtrip = dirty.saveStateToMemory();
        REQUIRE(dirtyRoundtrip == snapshots[probeFrame]);

        GeraNESEmu clean(DummyAudioOutput::instance());
        REQUIRE(clean.openRom(GeraNESTestSupport::romPath().string()));
        REQUIRE(clean.valid());
        REQUIRE(clean.loadStateFromMemoryOnCleanBoot(snapshots[probeFrame]));
        REQUIRE(clean.valid());
        const std::vector<uint8_t> cleanRoundtrip = clean.saveStateToMemory();
        REQUIRE(cleanRoundtrip == snapshots[probeFrame]);
    }
}

TEST_CASE("Replay-style restore and advance stays byte-exact from restored snapshots", "[state-replay][seek-advance]")
{
    GeraNESTestSupport::requireRomFixture();

    struct FrameBaseline
    {
        std::vector<uint8_t> state;
        uint32_t crc32 = 0u;
        uint64_t nextInputMask = 0u;
        bool retainsPreviousPlaybackFrame = false;
        std::vector<uint8_t> preparedReplayInputFrame;
    };

    GeraNESEmu baseline(DummyAudioOutput::instance());
    REQUIRE(baseline.openRom(GeraNESTestSupport::romPath().string()));
    REQUIRE(baseline.valid());

    std::vector<FrameBaseline> frames;
    {
        const std::vector<uint8_t> state = baseline.saveStateToMemory();
        frames.push_back({
            state,
            stateCrc32(state),
            0u,
            false,
            {}
        });
    }

    for(uint32_t frame = 0; frame < 140u; ++frame) {
        frames[frame].nextInputMask = deterministicReplayMask(frame);
        frames[frame].preparedReplayInputFrame =
            serializeInputFrame(makeReplayInputFrame(baseline, frame, frames[frame].nextInputMask));
        REQUIRE(advanceExactlyOneFrame(baseline, frames[frame].nextInputMask));
        const std::vector<uint8_t> state = baseline.saveStateToMemory();
        frames.push_back({
            state,
            stateCrc32(state),
            0u,
            baseline.hasPlaybackInputFrame(frame),
            {}
        });
    }

    const std::vector<uint32_t> restoreFrames = {0u, 1u, 2u, 15u, 47u, 89u, 120u};
    for(const uint32_t fromFrame : restoreFrames) {
        INFO("fromFrame=" << fromFrame);
        REQUIRE(fromFrame + 8u < frames.size());

        auto verifyAdvanceFromRestore = [&](const char* label, auto&& loadFn) {
            INFO("restoreMode=" << label);

            GeraNESEmu replay(DummyAudioOutput::instance());
            REQUIRE(replay.openRom(GeraNESTestSupport::romPath().string()));
            REQUIRE(replay.valid());
            REQUIRE(loadFn(replay, frames[fromFrame].state));
            REQUIRE(replay.valid());
            REQUIRE(stateCrc32(replay.saveStateToMemory()) == frames[fromFrame].crc32);

            for(uint32_t frame = fromFrame; frame < fromFrame + 8u; ++frame) {
                INFO("advance from restored frame " << frame);
                const std::vector<uint8_t> preparedReplayInputFrame =
                    serializeInputFrame(makeReplayInputFrame(replay, frame, frames[frame].nextInputMask));
                REQUIRE(preparedReplayInputFrame == frames[frame].preparedReplayInputFrame);
                REQUIRE(advanceExactlyOneFrame(replay, frames[frame].nextInputMask));
                const std::vector<uint8_t> replayState = replay.saveStateToMemory();
                const std::optional<ByteDiff> diff = firstByteDiff(frames[frame + 1u].state, replayState);
                CAPTURE(frames[frame + 1u].crc32);
                CAPTURE(stateCrc32(replayState));
                CAPTURE(diff.has_value());
                CAPTURE(diff ? diff->offset : 0u);
                CAPTURE(diff ? static_cast<uint32_t>(diff->expected) : 0u);
                CAPTURE(diff ? static_cast<uint32_t>(diff->actual) : 0u);
                CAPTURE(frames[frame + 1u].state.size());
                CAPTURE(replayState.size());
                CAPTURE(frames[frame + 1u].retainsPreviousPlaybackFrame);
                CAPTURE(replay.hasPlaybackInputFrame(frame));
                REQUIRE_FALSE(diff.has_value());
            }
        };

        verifyAdvanceFromRestore("dirty", [](GeraNESEmu& replay, const std::vector<uint8_t>& state) {
            replay.loadStateFromMemory(state);
            return replay.valid();
        });
        verifyAdvanceFromRestore("clean-boot", [](GeraNESEmu& replay, const std::vector<uint8_t>& state) {
            return replay.loadStateFromMemoryOnCleanBoot(state);
        });
    }
}

TEST_CASE("Replay file snapshots restore and continue with matching replay CRCs", "[state-replay][replay-file][seek-advance]")
{
    GeraNESTestSupport::requireRomFixture();

    const fs::path replayPath = locateReplayFixturePath();
    REQUIRE_FALSE(replayPath.empty());

    ReplayFile::Data replayData;
    std::string replayError;
    REQUIRE(ReplayFile::load(replayPath, replayData, replayError));
    REQUIRE_FALSE(replayData.frames.empty());

    struct FrameRecord
    {
        std::vector<uint8_t> state;
        uint32_t crc32 = 0u;
    };

    GeraNESEmu baseline(DummyAudioOutput::instance());
    REQUIRE(baseline.openRom(GeraNESTestSupport::romPath().string(), false));
    REQUIRE(baseline.valid());

    std::vector<FrameRecord> frames;
    frames.reserve(replayData.frames.size() + 1u);
    {
        const std::vector<uint8_t> state = baseline.saveStateToMemory();
        frames.push_back({state, stateCrc32(state)});
    }

    for(size_t index = 0; index < replayData.frames.size(); ++index) {
        INFO("baseline replay frame " << index);
        REQUIRE(advanceExactlyOneReplayFrame(baseline, replayData.frames[index]));
        const std::vector<uint8_t> state = baseline.saveStateToMemory();
        frames.push_back({state, stateCrc32(state)});
    }

    const uint32_t lastFrame = static_cast<uint32_t>(replayData.frames.size());
    std::vector<uint32_t> restoreFrames = {
        0u,
        1u,
        2u,
        std::min<uint32_t>(32u, lastFrame),
        std::min<uint32_t>(256u, lastFrame),
        std::min<uint32_t>(1024u, lastFrame),
        lastFrame / 2u,
        (lastFrame * 3u) / 4u
    };
    if(lastFrame > 64u) {
        restoreFrames.push_back(lastFrame - 64u);
    }
    if(lastFrame > 8u) {
        restoreFrames.push_back(lastFrame - 8u);
    }
    std::sort(restoreFrames.begin(), restoreFrames.end());
    restoreFrames.erase(std::unique(restoreFrames.begin(), restoreFrames.end()), restoreFrames.end());

    for(const uint32_t fromFrame : restoreFrames) {
        INFO("replay restore frame=" << fromFrame);
        REQUIRE(fromFrame < frames.size());

        auto verifyAdvanceFromRestore = [&](const char* label, auto&& loadFn) {
            INFO("restoreMode=" << label);

            GeraNESEmu replay(DummyAudioOutput::instance());
            REQUIRE(replay.openRom(GeraNESTestSupport::romPath().string(), false));
            REQUIRE(replay.valid());
            REQUIRE(loadFn(replay, frames[fromFrame].state));
            REQUIRE(replay.valid());
            REQUIRE(stateCrc32(replay.saveStateToMemory()) == frames[fromFrame].crc32);

            const uint32_t horizon = std::min<uint32_t>(32u, lastFrame - fromFrame);
            for(uint32_t frame = fromFrame; frame < fromFrame + horizon; ++frame) {
                INFO("advance replay frame " << frame);
                REQUIRE(advanceExactlyOneReplayFrame(replay, replayData.frames[frame]));
                const std::vector<uint8_t> replayState = replay.saveStateToMemory();
                const uint32_t replayCrc32 = stateCrc32(replayState);
                CAPTURE(frames[frame + 1u].crc32);
                CAPTURE(replayCrc32);
                if(replayCrc32 != frames[frame + 1u].crc32) {
                    const std::optional<ByteDiff> diff = firstByteDiff(frames[frame + 1u].state, replayState);
                    CAPTURE(diff.has_value());
                    CAPTURE(diff ? diff->offset : 0u);
                    CAPTURE(diff ? static_cast<uint32_t>(diff->expected) : 0u);
                    CAPTURE(diff ? static_cast<uint32_t>(diff->actual) : 0u);
                }
                REQUIRE(replayCrc32 == frames[frame + 1u].crc32);
            }
        };

        verifyAdvanceFromRestore("dirty", [](GeraNESEmu& replay, const std::vector<uint8_t>& state) {
            replay.loadStateFromMemory(state);
            return replay.valid();
        });
        verifyAdvanceFromRestore("clean-boot", [](GeraNESEmu& replay, const std::vector<uint8_t>& state) {
            return replay.loadStateFromMemoryOnCleanBoot(state);
        });
    }
}

TEST_CASE("Threaded replay seek and resume matches baseline replay CRCs", "[state-replay][replay-file][threaded-seek]")
{
    GeraNESTestSupport::requireRomFixture();

    const fs::path replayPath = locateReplayFixturePath();
    REQUIRE_FALSE(replayPath.empty());

    ReplayFile::Data replayData;
    std::string replayError;
    REQUIRE(ReplayFile::load(replayPath, replayData, replayError));
    REQUIRE_FALSE(replayData.frames.empty());

    const uint32_t replayFrameCount = static_cast<uint32_t>(replayData.frames.size());
    const uint32_t framesToTest = std::min<uint32_t>(replayFrameCount, 2000u);
    REQUIRE(framesToTest > 64u);

    const uint32_t frameDtMs = 16u;
    std::vector<uint32_t> baselineFrameCrc32(framesToTest + 1u, 0u);

    {
        ThreadedEmulationHost host(DummyAudioOutput::instance());
        host.setSimulationSuspended(true);
        REQUIRE(host.open(GeraNESTestSupport::romPath().string(), false));
        REQUIRE(host.valid());
        host.loadReplayPlayback(replayData.frames);
        host.setPresenterLockActive(true);
        REQUIRE(host.replayPlay());

        const std::vector<uint8_t> initialState = host.withExclusiveAccess([](GeraNESEmu& emu) {
            return emu.saveStateToMemory();
        });
        baselineFrameCrc32[0] = stateCrc32(initialState);

        uint32_t expectedFrame = 1u;
        while(expectedFrame <= framesToTest) {
            host.updateUntilFrame(frameDtMs);
            REQUIRE(waitForHostFrame(host, expectedFrame, std::chrono::milliseconds(250)));
            baselineFrameCrc32[expectedFrame] = host.lastFrameReadyNetplayCrc32();
            ++expectedFrame;
        }

        host.shutdown();
    }

    const std::vector<uint32_t> seekFrames = {
        0u,
        1u,
        2u,
        15u,
        63u,
        127u,
        255u,
        511u,
        1023u,
        framesToTest / 2u,
        framesToTest - 64u
    };

    for(const uint32_t seekFrame : seekFrames) {
        INFO("seekFrame=" << seekFrame);
        REQUIRE(seekFrame < framesToTest);

        ThreadedEmulationHost host(DummyAudioOutput::instance());
        host.setSimulationSuspended(true);
        REQUIRE(host.open(GeraNESTestSupport::romPath().string(), false));
        REQUIRE(host.valid());
        host.loadReplayPlayback(replayData.frames);
        host.setPresenterLockActive(true);
        REQUIRE(host.replaySeekToFrame(seekFrame));

        const uint32_t settledFrame = host.replayPlaybackStatus().cursorFrame;
        REQUIRE(settledFrame == seekFrame);
        REQUIRE(host.replayPlay());
        const uint32_t horizon = std::min<uint32_t>(16u, framesToTest - seekFrame);
        for(uint32_t step = 0; step < horizon; ++step) {
            const uint32_t expectedFrame = seekFrame + step + 1u;
            host.updateUntilFrame(frameDtMs);
            REQUIRE(waitForHostFrame(host, expectedFrame, std::chrono::milliseconds(250)));
            CAPTURE(expectedFrame);
            CAPTURE(host.lastFrameReadyNetplayCrc32());
            CAPTURE(baselineFrameCrc32[expectedFrame]);
            REQUIRE(host.lastFrameReadyNetplayCrc32() == baselineFrameCrc32[expectedFrame]);
        }

        host.shutdown();
    }
}
