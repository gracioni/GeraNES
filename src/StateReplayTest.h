#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
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
        uint64_t nextInputMask = 0;
        uint32_t crc32 = 0;
        std::vector<uint8_t> snapshot;
    };

    struct ReplayMismatch
    {
        uint32_t fromFrame = 0;
        uint32_t targetFrame = 0;
        uint32_t expectedCrc32 = 0;
        uint32_t actualCrc32 = 0;
        std::string reason;
    };

    struct CaseResult
    {
        uint32_t seed = 0;
        uint32_t replayHorizon = 0;
        std::optional<ReplayMismatch> freshMismatch;
        std::optional<ReplayMismatch> dirtyMismatch;
        std::optional<ReplayMismatch> cleanBootMismatch;
        std::vector<FrameRecord> baseline;
        std::string baselineFailureReason;

        bool ok() const
        {
            return baselineFailureReason.empty() &&
                   !freshMismatch.has_value() &&
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

        Buttons buttonsForFrame(uint32_t frameIndex, uint32_t fps) const
        {
            const uint32_t startupQuietFrames = std::max<uint32_t>(fps, 30u);
            if(frameIndex < startupQuietFrames) {
                return {};
            }

            Buttons buttons;
            const uint32_t activeIndex = frameIndex - startupQuietFrames;
            const uint32_t segmentLength = 5u + (mix(m_seed ^ 0xA511E9B3u) % 4u);
            const uint32_t segment = activeIndex / segmentLength;
            const uint32_t localFrame = activeIndex % segmentLength;
            const uint32_t action = mix(m_seed ^ segment ^ 0x9E3779B9u);

            switch(action % 10u) {
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
            buttons.start = (action % 37u) == 0u && localFrame == 0u;
            buttons.select = (action % 53u) == 0u && localFrame == 0u;
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

    static void queuePadMaskForCurrentFrame(GeraNESEmu& emu, uint64_t mask)
    {
        InputFrame frame = emu.createInputFrame(emu.frameCount());
        frame.p1A = (mask & (1ull << 0)) != 0;
        frame.p1B = (mask & (1ull << 1)) != 0;
        frame.p1Select = (mask & (1ull << 2)) != 0;
        frame.p1Start = (mask & (1ull << 3)) != 0;
        frame.p1Up = (mask & (1ull << 4)) != 0;
        frame.p1Down = (mask & (1ull << 5)) != 0;
        frame.p1Left = (mask & (1ull << 6)) != 0;
        frame.p1Right = (mask & (1ull << 7)) != 0;
        emu.queueInputFrame(frame);
    }

    static bool advanceExactlyOneFrame(GeraNESEmu& emu, uint64_t inputMask)
    {
        if(!emu.valid()) return false;        

        auto prev = emu.frameCount();
        while(prev == emu.frameCount()) {
            emu.update(1);
        }

        queuePadMaskForCurrentFrame(emu, inputMask);

        //return emu.updateUntilFrame(17);
        return true;
    }

    static std::optional<std::vector<FrameRecord>> buildBaseline(const std::string& romPath,
                                                                 uint32_t frames,
                                                                 uint32_t seed,
                                                                 std::string& failureReason)
    {
        GeraNESEmu emu(DummyAudioOutput::instance());
        if(!emu.open(romPath) || !emu.valid()) {
            failureReason = "Failed to open ROM.";
            return std::nullopt;
        }

        DeterministicInputGenerator generator(seed);
        std::vector<FrameRecord> records;
        records.reserve(frames + 1u);

        FrameRecord initial;
        initial.frame = 0;
        initial.crc32 = emu.canonicalStateCrc32();
        initial.snapshot = emu.saveStateToMemory();
        records.push_back(std::move(initial));

        for(uint32_t currentFrame = 0; currentFrame < frames; ++currentFrame) {
            const uint64_t inputMask = buildPadMask(
                generator.buttonsForFrame(currentFrame, std::max<uint32_t>(1u, emu.getRegionFPS()))
            );
            records[currentFrame].nextInputMask = inputMask;

            if(!advanceExactlyOneFrame(emu, inputMask)) {
                failureReason =
                    "Failed to advance baseline from frame " + std::to_string(currentFrame) +
                    " to frame " + std::to_string(currentFrame + 1u) + ".";
                return std::nullopt;
            }

            FrameRecord record;
            record.frame = emu.frameCount();
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
        if(fromFrame >= baseline.size()) {
            return ReplayMismatch{fromFrame, fromFrame, 0, 0, "Probe frame out of range."};
        }
        if(baseline[fromFrame].snapshot.empty()) {
            return ReplayMismatch{fromFrame, fromFrame, baseline[fromFrame].crc32, 0, "Missing snapshot."};
        }

        const bool loaded = cleanBootBeforeLoad
            ? emu.loadStateFromMemoryOnCleanBoot(baseline[fromFrame].snapshot)
            : (emu.loadStateFromMemory(baseline[fromFrame].snapshot), emu.valid());
        if(!loaded || !emu.valid()) {
            return ReplayMismatch{fromFrame, fromFrame, baseline[fromFrame].crc32, 0, "Failed to load snapshot."};
        }

        const uint32_t loadedCrc32 = emu.canonicalStateCrc32();
        if(loadedCrc32 != baseline[fromFrame].crc32) {
            return ReplayMismatch{
                fromFrame,
                fromFrame,
                baseline[fromFrame].crc32,
                loadedCrc32,
                "Loaded snapshot CRC mismatch."
            };
        }

        const uint32_t targetFrame = std::min<uint32_t>(
            static_cast<uint32_t>(baseline.size() - 1u),
            fromFrame + std::max<uint32_t>(1u, replayHorizon)
        );

        for(uint32_t currentFrame = fromFrame; currentFrame < targetFrame; ++currentFrame) {
            if(!advanceExactlyOneFrame(emu, baseline[currentFrame].nextInputMask)) {
                return ReplayMismatch{
                    fromFrame,
                    currentFrame + 1u,
                    baseline[currentFrame + 1u].crc32,
                    0,
                    "Failed to advance replayed frame."
                };
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
            actualCrc32,
            "Future CRC mismatch after replay."
        };
    }

    static std::optional<CaseResult> runCase(const Options& options,
                                             uint32_t seed,
                                             uint32_t replayHorizon)
    {
        CaseResult result;
        result.seed = seed;
        result.replayHorizon = replayHorizon;

        std::string baselineFailureReason;
        const std::optional<std::vector<FrameRecord>> baseline = buildBaseline(
            options.romPath,
            options.frames,
            seed,
            baselineFailureReason
        );
        if(!baseline.has_value()) {
            result.baselineFailureReason = baselineFailureReason;
            return result;
        }
        result.baseline = *baseline;

        std::unique_ptr<GeraNESEmu> dirtyEmu = std::make_unique<GeraNESEmu>(DummyAudioOutput::instance());
        std::unique_ptr<GeraNESEmu> freshEmu = std::make_unique<GeraNESEmu>(DummyAudioOutput::instance());
        std::unique_ptr<GeraNESEmu> cleanBootEmu = std::make_unique<GeraNESEmu>(DummyAudioOutput::instance());
        if(!dirtyEmu->open(options.romPath) || !dirtyEmu->valid()) {
            result.baselineFailureReason = "Failed to open dirty replay emulator.";
            return result;
        }
        if(!freshEmu->open(options.romPath) || !freshEmu->valid()) {
            result.baselineFailureReason = "Failed to open fresh replay emulator.";
            return result;
        }
        if(!cleanBootEmu->open(options.romPath) || !cleanBootEmu->valid()) {
            result.baselineFailureReason = "Failed to open clean-boot replay emulator.";
            return result;
        }

        std::vector<uint32_t> probeFrames;
        if(options.fromFrame.has_value()) {
            probeFrames.push_back(std::min<uint32_t>(*options.fromFrame, static_cast<uint32_t>(baseline->size() - 1u)));
        } else {
            const uint32_t stride = std::max<uint32_t>(1u, options.probeStride);
            for(uint32_t frame = 0; frame < baseline->size(); frame += stride) {
                probeFrames.push_back(frame);
            }
            if(probeFrames.empty() || probeFrames.back() != baseline->size() - 1u) {
                probeFrames.push_back(static_cast<uint32_t>(baseline->size() - 1u));
            }
        }

        for(const uint32_t fromFrame : probeFrames) {
            if(!result.freshMismatch.has_value()) {
                result.freshMismatch = probeFromSnapshot(*freshEmu, *baseline, fromFrame, replayHorizon, false);
            }
            if(!result.dirtyMismatch.has_value()) {
                result.dirtyMismatch = probeFromSnapshot(*dirtyEmu, *baseline, fromFrame, replayHorizon, false);
            }
            if(!result.cleanBootMismatch.has_value()) {
                result.cleanBootMismatch = probeFromSnapshot(*cleanBootEmu, *baseline, fromFrame, replayHorizon, true);
            }

            if(result.freshMismatch.has_value() ||
               result.dirtyMismatch.has_value() ||
               result.cleanBootMismatch.has_value()) {
                break;
            }
        }

        return result;
    }

    static nlohmann::json mismatchToJson(const std::optional<ReplayMismatch>& mismatch)
    {
        if(!mismatch.has_value()) return nullptr;
        return {
            {"fromFrame", mismatch->fromFrame},
            {"targetFrame", mismatch->targetFrame},
            {"expectedCrc32", mismatch->expectedCrc32},
            {"actualCrc32", mismatch->actualCrc32},
            {"reason", mismatch->reason}
        };
    }

    static nlohmann::json baselineTailToJson(const std::vector<FrameRecord>& baseline)
    {
        nlohmann::json frames = nlohmann::json::array();
        const size_t start = baseline.size() > 8 ? (baseline.size() - 8) : 0;
        for(size_t i = start; i < baseline.size(); ++i) {
            frames.push_back({
                {"frame", baseline[i].frame},
                {"nextInputMask", baseline[i].nextInputMask},
                {"crc32", baseline[i].crc32},
                {"snapshotSize", baseline[i].snapshot.size()}
            });
        }
        return frames;
    }

    static nlohmann::json caseResultToJson(const CaseResult& result)
    {
        return {
            {"seed", result.seed},
            {"replayHorizon", result.replayHorizon},
            {"ok", result.ok()},
            {"baselineFailureReason", result.baselineFailureReason},
            {"baselineFrames", result.baseline.size()},
            {"baselineTail", baselineTailToJson(result.baseline)},
            {"freshInstanceReplay", mismatchToJson(result.freshMismatch)},
            {"dirtyInstanceReplay", mismatchToJson(result.dirtyMismatch)},
            {"cleanBootReplay", mismatchToJson(result.cleanBootMismatch)}
        };
    }

    static int emitReport(const Options& options, const nlohmann::json& report)
    {
        if(!options.reportPath.empty()) {
            std::ofstream out(options.reportPath, std::ios::binary);
            if(!out) {
                std::cerr << "Failed to write state replay report: " << options.reportPath << std::endl;
                return RESULT_ERROR;
            }
            out << report.dump(2) << '\n';
            std::cout << options.reportPath << std::endl;
        } else {
            std::cout << report.dump(2) << std::endl;
        }

        return report.value("status", std::string()) == "ok" ? 0 : RESULT_FAILED;
    }

public:
    static int runHeadless(const Options& options)
    {
        if(options.romPath.empty()) {
            return emitReport(options, {
                {"status", "error"},
                {"failureReason", "State replay test requires a ROM path."}
            });
        }

        std::set<uint32_t> seeds = {options.seed};
        std::set<uint32_t> horizons = {std::max<uint32_t>(1u, options.replayHorizon)};
        for(uint32_t extraSeed : options.extraSeeds) seeds.insert(extraSeed);
        for(uint32_t extraHorizon : options.extraReplayHorizons) horizons.insert(std::max<uint32_t>(1u, extraHorizon));

        if(options.robust) {
            seeds.insert(1u);
            seeds.insert(0xDEADBEEFu);
            horizons.insert(1u);
            horizons.insert(8u);
        }

        std::cerr << "StateReplayTest: " << seeds.size() << " seed(s), "
                  << horizons.size() << " horizon(s), probeStride=" << std::max<uint32_t>(1u, options.probeStride);
        if(options.fromFrame.has_value()) {
            std::cerr << ", fromFrame=" << *options.fromFrame;
        }
        std::cerr << std::endl;

        nlohmann::json cases = nlohmann::json::array();
        bool allOk = true;

        for(uint32_t seed : seeds) {
            for(uint32_t replayHorizon : horizons) {
                std::cerr << "StateReplayTest: seed=" << seed
                          << " horizon=" << replayHorizon << std::endl;
                const std::optional<CaseResult> result = runCase(options, seed, replayHorizon);
                if(!result.has_value()) {
                    allOk = false;
                    cases.push_back({
                        {"seed", seed},
                        {"replayHorizon", replayHorizon},
                        {"ok", false},
                        {"baselineFailureReason", "Internal case setup failed."}
                    });
                    continue;
                }
                cases.push_back(caseResultToJson(*result));
                allOk = allOk && result->ok();
                if(!result->ok()) {
                    std::cerr << "Failed to run replay test case. seed=" << seed
                              << " replayHorizon=" << replayHorizon << std::endl;
                }
            }
        }

        nlohmann::json report = {
            {"status", allOk ? "ok" : "failed"},
            {"romPath", options.romPath},
            {"frames", options.frames},
            {"probeStride", std::max<uint32_t>(1u, options.probeStride)},
            {"fromFrame", options.fromFrame.has_value() ? nlohmann::json(*options.fromFrame) : nlohmann::json(nullptr)},
            {"cases", cases}
        };
        return emitReport(options, report);
    }
};

#endif
