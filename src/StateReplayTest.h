#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "GeraNES/GeraNESEmu.h"

class StateReplayTest
{
public:
    struct Options
    {
        std::string romPath;
        std::string reportPath;
        uint32_t frames = 180;
        uint32_t replayHorizon = 3;
        uint32_t seed = 0x13572468u;
        std::vector<uint32_t> extraReplayHorizons;
        std::vector<uint32_t> extraSeeds;
        bool robust = false;
        uint32_t probeStride = 1;
        std::optional<uint32_t> fromFrame;
    };

private:
    struct Buttons
    {
        bool a = false;
        bool b = false;
        bool select = false;
        bool start = false;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
    };

    struct FrameRecord
    {
        uint32_t frame = 0;
        uint64_t inputMask = 0;
        uint32_t crc32 = 0;
        std::vector<uint8_t> snapshot;
    };

    struct ReplayMismatch
    {
        uint32_t fromFrame = 0;
        uint32_t targetFrame = 0;
        uint32_t expectedCrc32 = 0;
        uint32_t actualCrc32 = 0;
    };

    struct CaseResult
    {
        uint32_t seed = 0;
        uint32_t replayHorizon = 0;
        std::optional<ReplayMismatch> freshMismatch;
        std::optional<ReplayMismatch> dirtyMismatch;
        std::optional<ReplayMismatch> cleanBootMismatch;
        std::vector<FrameRecord> baseline;

        bool ok() const
        {
            return !freshMismatch.has_value() &&
                   !dirtyMismatch.has_value() &&
                   !cleanBootMismatch.has_value();
        }
    };

    static constexpr int RESULT_FAILED = 1;
    static constexpr int RESULT_ERROR = 2;

    class DeterministicInputGenerator
    {
    private:
        uint32_t m_seed = 0;

        static uint32_t mix(uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

    public:
        explicit DeterministicInputGenerator(uint32_t seed)
            : m_seed(seed == 0 ? 0x12345678u : seed)
        {
        }

        Buttons buttonsForFrame(uint32_t frame, uint32_t fps) const
        {
            const uint32_t startupQuietFrames = std::max<uint32_t>(fps, 30u);
            if(frame <= startupQuietFrames) {
                return {};
            }

            Buttons buttons;
            const uint32_t segmentLength = 5u + (mix(m_seed ^ 0xA511E9B3u) % 4u);
            const uint32_t segment = (frame - startupQuietFrames - 1u) / segmentLength;
            const uint32_t localFrame = (frame - startupQuietFrames - 1u) % segmentLength;
            const uint32_t action = mix(m_seed ^ segment ^ 0x9E3779B9u);
            const uint32_t direction = action % 10u;
            const bool tapStart = (action % 37u) == 0u && localFrame == 0u;
            const bool tapSelect = (action % 53u) == 0u && localFrame == 0u;

            if(tapStart) buttons.start = true;
            if(tapSelect) buttons.select = true;

            switch(direction) {
                case 0: buttons.up = true; break;
                case 1: buttons.down = true; break;
                case 2: buttons.left = true; break;
                case 3: buttons.right = true; break;
                case 4: buttons.up = true; buttons.right = true; break;
                case 5: buttons.up = true; buttons.left = true; break;
                case 6: buttons.down = true; buttons.right = true; break;
                case 7: buttons.down = true; buttons.left = true; break;
                default: break;
            }

            buttons.a = ((action >> 8) & 0x3u) != 0u;
            buttons.b = ((action >> 10) & 0x3u) == 1u || ((action >> 10) & 0x3u) == 2u;
            return buttons;
        }
    };

    static uint64_t buildPadMask(const Buttons& buttons)
    {
        uint64_t mask = 0;
        if(buttons.a) mask |= (1ull << 0);
        if(buttons.b) mask |= (1ull << 1);
        if(buttons.select) mask |= (1ull << 2);
        if(buttons.start) mask |= (1ull << 3);
        if(buttons.up) mask |= (1ull << 4);
        if(buttons.down) mask |= (1ull << 5);
        if(buttons.left) mask |= (1ull << 6);
        if(buttons.right) mask |= (1ull << 7);
        return mask;
    }

    static void applyPadMask(GeraNESEmu& emu, uint64_t mask)
    {
        emu.setController1Buttons(
            (mask & (1ull << 0)) != 0,
            (mask & (1ull << 1)) != 0,
            (mask & (1ull << 2)) != 0,
            (mask & (1ull << 3)) != 0,
            (mask & (1ull << 4)) != 0,
            (mask & (1ull << 5)) != 0,
            (mask & (1ull << 6)) != 0,
            (mask & (1ull << 7)) != 0
        );
        emu.setController2Buttons(false, false, false, false, false, false, false, false);
        emu.setController3Buttons(false, false, false, false, false, false, false, false);
        emu.setController4Buttons(false, false, false, false, false, false, false, false);
    }

    static bool advanceExactlyOneFrame(GeraNESEmu& emu, uint64_t inputMask)
    {
        if(!emu.valid()) return false;

        applyPadMask(emu, inputMask);
        const uint32_t previousFrame = emu.frameCount();
        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, emu.getRegionFPS()));
        while(emu.valid() && emu.frameCount() == previousFrame) {
            emu.update(frameDt);
        }
        return emu.valid() && emu.frameCount() == previousFrame + 1u;
    }

    static std::optional<std::vector<FrameRecord>> buildBaseline(const std::string& romPath,
                                                                 uint32_t frames,
                                                                 uint32_t seed)
    {
        GeraNESEmu emu(DummyAudioOutput::instance());
        if(!emu.open(romPath) || !emu.valid()) {
            return std::nullopt;
        }

        DeterministicInputGenerator generator(seed);
        std::vector<FrameRecord> records;
        records.reserve(frames + 1u);

        FrameRecord frameZero;
        frameZero.frame = 0;
        frameZero.crc32 = emu.canonicalStateCrc32();
        frameZero.snapshot = emu.saveStateToMemory();
        records.push_back(std::move(frameZero));

        for(uint32_t frame = 1; frame <= frames; ++frame) {
            const uint64_t inputMask = buildPadMask(generator.buttonsForFrame(frame, std::max<uint32_t>(1, emu.getRegionFPS())));
            if(!advanceExactlyOneFrame(emu, inputMask)) {
                return std::nullopt;
            }

            FrameRecord record;
            record.frame = frame;
            record.inputMask = inputMask;
            record.crc32 = emu.canonicalStateCrc32();
            record.snapshot = emu.saveStateToMemory();
            records.push_back(std::move(record));
        }

        return records;
    }

    static std::optional<ReplayMismatch> probeFromSnapshot(GeraNESEmu& emu,
                                                           const std::vector<FrameRecord>& baseline,
                                                           uint32_t fromFrame,
                                                           uint32_t replayHorizon,
                                                           bool cleanBootBeforeLoad)
    {
        if(fromFrame >= baseline.size()) return std::nullopt;
        if(baseline[fromFrame].snapshot.empty()) return std::nullopt;

        const bool loaded = cleanBootBeforeLoad
            ? emu.loadStateFromMemoryOnCleanBoot(baseline[fromFrame].snapshot)
            : (emu.loadStateFromMemory(baseline[fromFrame].snapshot), true);
        if(!loaded || !emu.valid()) {
            return ReplayMismatch{fromFrame, fromFrame, baseline[fromFrame].crc32, 0};
        }

        const uint32_t loadedCrc32 = emu.canonicalStateCrc32();
        if(loadedCrc32 != baseline[fromFrame].crc32) {
            return ReplayMismatch{
                fromFrame,
                fromFrame,
                baseline[fromFrame].crc32,
                loadedCrc32
            };
        }

        const uint32_t targetFrame = std::min<uint32_t>(
            static_cast<uint32_t>(baseline.size() - 1u),
            fromFrame + std::max<uint32_t>(1, replayHorizon)
        );

        for(uint32_t frame = fromFrame + 1u; frame <= targetFrame; ++frame) {
            if(!advanceExactlyOneFrame(emu, baseline[frame].inputMask)) {
                return ReplayMismatch{fromFrame, frame, baseline[frame].crc32, 0};
            }
        }

        const uint32_t actualCrc32 = emu.canonicalStateCrc32();
        if(actualCrc32 == baseline[targetFrame].crc32) {
            return std::nullopt;
        }

        return ReplayMismatch{
            fromFrame,
            targetFrame,
            baseline[targetFrame].crc32,
            actualCrc32
        };
    }

    static nlohmann::json mismatchToJson(const std::optional<ReplayMismatch>& mismatch)
    {
        if(!mismatch.has_value()) {
            return {{"ok", true}};
        }

        return {
            {"ok", false},
            {"fromFrame", mismatch->fromFrame},
            {"targetFrame", mismatch->targetFrame},
            {"expectedCrc32", mismatch->expectedCrc32},
            {"actualCrc32", mismatch->actualCrc32}
        };
    }

    static nlohmann::json buildFrameWindow(const std::vector<FrameRecord>& baseline,
                                           const std::optional<ReplayMismatch>& freshMismatch,
                                           const std::optional<ReplayMismatch>& dirtyMismatch)
    {
        uint32_t centerFrame = 0;
        if(freshMismatch.has_value()) {
            centerFrame = freshMismatch->targetFrame;
        } else if(dirtyMismatch.has_value()) {
            centerFrame = dirtyMismatch->targetFrame;
        }

        if(centerFrame == 0 || baseline.empty()) {
            centerFrame = static_cast<uint32_t>(baseline.empty() ? 0 : baseline.back().frame);
        }

        const uint32_t frameStart = centerFrame > 3 ? centerFrame - 3u : 0u;
        const uint32_t frameEnd = std::min<uint32_t>(
            static_cast<uint32_t>(baseline.empty() ? 0 : baseline.back().frame),
            centerFrame + 3u
        );

        nlohmann::json frames = nlohmann::json::array();
        for(uint32_t frame = frameStart; frame <= frameEnd && frame < baseline.size(); ++frame) {
            frames.push_back({
                {"frame", baseline[frame].frame},
                {"inputMask", baseline[frame].inputMask},
                {"crc32", baseline[frame].crc32},
                {"snapshotSize", baseline[frame].snapshot.size()}
            });
        }

        return {
            {"frameStart", frameStart},
            {"frameEnd", frameEnd},
            {"frames", frames}
        };
    }

    static int writeReport(const Options& options,
                           const std::vector<CaseResult>& cases)
    {
        const bool ok = std::all_of(
            cases.begin(),
            cases.end(),
            [](const CaseResult& result) { return result.ok(); }
        );

        nlohmann::json casesJson = nlohmann::json::array();
        uint32_t passedCases = 0;
        for(const CaseResult& result : cases) {
            if(result.ok()) {
                ++passedCases;
            }

            casesJson.push_back({
                {"seed", result.seed},
                {"replayHorizon", result.replayHorizon},
                {"freshInstanceReplay", mismatchToJson(result.freshMismatch)},
                {"dirtyInstanceReplay", mismatchToJson(result.dirtyMismatch)},
                {"cleanBootReplay", mismatchToJson(result.cleanBootMismatch)},
                {"frameWindow", buildFrameWindow(result.baseline, result.freshMismatch, result.dirtyMismatch)}
            });
        }

        std::set<uint32_t> seeds;
        std::set<uint32_t> replayHorizons;
        for(const CaseResult& result : cases) {
            seeds.insert(result.seed);
            replayHorizons.insert(result.replayHorizon);
        }

        nlohmann::json report = {
            {"status", ok ? "ok" : "mismatch"},
            {"romPath", options.romPath},
            {"frames", options.frames},
            {"robust", options.robust},
            {"seed", options.seed},
            {"replayHorizon", options.replayHorizon},
            {"probeStride", options.probeStride},
            {"fromFrame", options.fromFrame.has_value() ? nlohmann::json(*options.fromFrame) : nlohmann::json(nullptr)},
            {"seeds", std::vector<uint32_t>(seeds.begin(), seeds.end())},
            {"replayHorizons", std::vector<uint32_t>(replayHorizons.begin(), replayHorizons.end())},
            {"summary", {
                {"caseCount", static_cast<uint32_t>(cases.size())},
                {"passedCases", passedCases}
            }},
            {"cases", casesJson}
        };

        const std::string serialized = report.dump(2);
        if(!options.reportPath.empty()) {
            std::ofstream out(options.reportPath, std::ios::binary | std::ios::trunc);
            out << serialized;
            std::cout << options.reportPath << std::endl;
        } else {
            std::cout << serialized << std::endl;
        }

        return ok ? 0 : RESULT_FAILED;
    }

    static std::vector<uint32_t> buildSeedList(const Options& options)
    {
        std::set<uint32_t> seeds = {options.seed};
        for(uint32_t seed : options.extraSeeds) {
            seeds.insert(seed);
        }

        if(options.robust) {
            seeds.insert(0x00000001u);
            seeds.insert(0xDEADBEEFu);
            seeds.insert(options.seed ^ 0x9E3779B9u);
        }

        return std::vector<uint32_t>(seeds.begin(), seeds.end());
    }

    static std::vector<uint32_t> buildReplayHorizonList(const Options& options)
    {
        std::set<uint32_t> horizons = {std::max<uint32_t>(1, options.replayHorizon)};
        for(uint32_t horizon : options.extraReplayHorizons) {
            if(horizon > 0) {
                horizons.insert(horizon);
            }
        }

        if(options.robust) {
            horizons.insert(1u);
            horizons.insert(3u);
            horizons.insert(std::min<uint32_t>(8u, options.frames));
            horizons.insert(std::min<uint32_t>(16u, options.frames));
        }

        horizons.erase(0u);
        return std::vector<uint32_t>(horizons.begin(), horizons.end());
    }

    static std::vector<uint32_t> buildProbeFrames(const Options& options, uint32_t maxFromFrame)
    {
        if(options.fromFrame.has_value()) {
            if(*options.fromFrame > maxFromFrame) {
                return {};
            }
            return {*options.fromFrame};
        }

        const uint32_t stride = std::max<uint32_t>(1, options.probeStride);
        std::vector<uint32_t> frames;
        frames.reserve((maxFromFrame / stride) + 2u);
        for(uint32_t fromFrame = 0; fromFrame <= maxFromFrame; fromFrame += stride) {
            frames.push_back(fromFrame);
        }
        if(frames.empty() || frames.back() != maxFromFrame) {
            frames.push_back(maxFromFrame);
        }
        return frames;
    }

    static std::optional<CaseResult> runCase(const Options& options, uint32_t seed, uint32_t replayHorizon)
    {
        std::cerr << "StateReplayTest: seed=" << seed << " horizon=" << replayHorizon << std::endl;
        const std::optional<std::vector<FrameRecord>> baseline = buildBaseline(options.romPath, options.frames, seed);
        if(!baseline.has_value()) {
            return std::nullopt;
        }

        auto freshEmu = std::make_unique<GeraNESEmu>(DummyAudioOutput::instance());
        if(!freshEmu->open(options.romPath) || !freshEmu->valid()) {
            return std::nullopt;
        }

        auto dirtyEmu = std::make_unique<GeraNESEmu>(DummyAudioOutput::instance());
        if(!dirtyEmu->open(options.romPath) || !dirtyEmu->valid()) {
            return std::nullopt;
        }

        auto cleanBootEmu = std::make_unique<GeraNESEmu>(DummyAudioOutput::instance());
        if(!cleanBootEmu->open(options.romPath) || !cleanBootEmu->valid()) {
            return std::nullopt;
        }

        DeterministicInputGenerator generator(seed);
        for(uint32_t frame = 1; frame <= options.frames; ++frame) {
            const uint64_t inputMask = buildPadMask(generator.buttonsForFrame(frame, std::max<uint32_t>(1, dirtyEmu->getRegionFPS())));
            if(!advanceExactlyOneFrame(*dirtyEmu, inputMask)) {
                return std::nullopt;
            }
        }

        CaseResult result;
        result.seed = seed;
        result.replayHorizon = replayHorizon;
        result.baseline = *baseline;

        const uint32_t maxFromFrame = static_cast<uint32_t>(baseline->size() > 1 ? baseline->size() - 2u : 0u);
        const std::vector<uint32_t> probeFrames = buildProbeFrames(options, maxFromFrame);
        if(probeFrames.empty()) {
            return std::nullopt;
        }

        const size_t progressStep = std::max<size_t>(1u, probeFrames.size() / 10u);
        for(size_t probeIndex = 0; probeIndex < probeFrames.size(); ++probeIndex) {
            const uint32_t fromFrame = probeFrames[probeIndex];
            if(probeIndex == 0 || ((probeIndex + 1u) % progressStep) == 0u || (probeIndex + 1u) == probeFrames.size()) {
                std::cerr << "StateReplayTest: probe " << (probeIndex + 1u) << "/" << probeFrames.size()
                          << " fromFrame=" << fromFrame << std::endl;
            }

            if(!result.freshMismatch.has_value()) {
                result.freshMismatch = probeFromSnapshot(*freshEmu, *baseline, fromFrame, replayHorizon, false);
            }
            if(!result.dirtyMismatch.has_value()) {
                result.dirtyMismatch = probeFromSnapshot(*dirtyEmu, *baseline, fromFrame, replayHorizon, false);
            }
            if(!result.cleanBootMismatch.has_value()) {
                result.cleanBootMismatch = probeFromSnapshot(*cleanBootEmu, *baseline, fromFrame, replayHorizon, true);
            }
            if(result.freshMismatch.has_value() &&
               result.dirtyMismatch.has_value() &&
               result.cleanBootMismatch.has_value()) {
                break;
            }
        }

        return result;
    }

public:
    static int runHeadless(const Options& options)
    {
        if(options.romPath.empty()) {
            std::cerr << "State replay test requires a ROM path." << std::endl;
            return RESULT_ERROR;
        }

        const std::vector<uint32_t> seeds = buildSeedList(options);
        const std::vector<uint32_t> replayHorizons = buildReplayHorizonList(options);
        std::cerr << "StateReplayTest: " << seeds.size() << " seed(s), "
                  << replayHorizons.size() << " horizon(s), "
                  << "probeStride=" << options.probeStride;
        if(options.fromFrame.has_value()) {
            std::cerr << ", fromFrame=" << *options.fromFrame;
        }
        std::cerr << std::endl;
        std::vector<CaseResult> cases;
        cases.reserve(seeds.size() * replayHorizons.size());

        for(uint32_t seed : seeds) {
            for(uint32_t replayHorizon : replayHorizons) {
                const std::optional<CaseResult> result = runCase(options, seed, replayHorizon);
                if(!result.has_value()) {
                    std::cerr << "Failed to run replay test case. seed=" << seed
                              << " replayHorizon=" << replayHorizon << std::endl;
                    return RESULT_ERROR;
                }
                cases.push_back(*result);
            }
        }

        return writeReport(options, cases);
    }
};

#endif
