#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "GeraNES/GeraNESEmu.h"

class ResimulationTest
{
public:
    struct Options
    {
        std::string romPath;
        std::string reportPath;
        uint32_t rollbackFrame = 120;
        uint32_t runtimeMsAfterRollback = 48;
        uint32_t futureInputFrames = 24;
        uint32_t seed = 0x13572468u;
    };

private:
    static constexpr int RESULT_FAILED = 1;
    static constexpr int RESULT_ERROR = 2;

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

    class RecordingAudioOutput : public IAudioOutput
    {
    public:
        struct Stats
        {
            uint32_t totalRenderCalls = 0;
            uint32_t audibleRenderCalls = 0;
            uint32_t silentRenderCalls = 0;
            uint32_t totalRenderedMs = 0;
        };

    private:
        Stats m_stats;

    public:
        void render(uint32_t dt, bool silenceFlag) override
        {
            ++m_stats.totalRenderCalls;
            m_stats.totalRenderedMs += dt;
            if(silenceFlag) {
                ++m_stats.silentRenderCalls;
            }
            else {
                ++m_stats.audibleRenderCalls;
            }
        }

        void clearAudioBuffers() override {}
        void discardQueuedAudio() override {}

        const Stats& stats() const
        {
            return m_stats;
        }

        void resetStats()
        {
            m_stats = {};
        }
    };

    struct RunArtifacts
    {
        GeraNESEmu::ExecutionPoint point;
        uint32_t crc32 = 0;
        uint32_t canonicalResimCrc32 = 0;
        std::vector<uint8_t> snapshot;
        std::vector<uint8_t> canonicalSnapshot;
        RecordingAudioOutput::Stats audioStats;
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

    static uint64_t mutateMaskForSpeculation(uint64_t mask)
    {
        mask ^= (1ull << 0);
        mask ^= (1ull << 1);
        mask ^= (1ull << 3);

        const bool up = (mask & (1ull << 4)) != 0;
        const bool down = (mask & (1ull << 5)) != 0;
        const bool left = (mask & (1ull << 6)) != 0;
        const bool right = (mask & (1ull << 7)) != 0;

        mask &= ~((1ull << 4) | (1ull << 5) | (1ull << 6) | (1ull << 7));
        if(up) mask |= (1ull << 5);
        if(down) mask |= (1ull << 4);
        if(left) mask |= (1ull << 7);
        if(right) mask |= (1ull << 6);
        if((mask & 0xF0ull) == 0) {
            mask |= (1ull << 7);
        }

        return mask;
    }

    static void queuePadMaskForFrame(GeraNESEmu& emu, uint32_t frame, uint64_t mask)
    {
        InputFrame inputFrame = emu.createInputFrame(frame);
        inputFrame.p1A = (mask & (1ull << 0)) != 0;
        inputFrame.p1B = (mask & (1ull << 1)) != 0;
        inputFrame.p1Select = (mask & (1ull << 2)) != 0;
        inputFrame.p1Start = (mask & (1ull << 3)) != 0;
        inputFrame.p1Up = (mask & (1ull << 4)) != 0;
        inputFrame.p1Down = (mask & (1ull << 5)) != 0;
        inputFrame.p1Left = (mask & (1ull << 6)) != 0;
        inputFrame.p1Right = (mask & (1ull << 7)) != 0;
        emu.queueInputFrame(inputFrame);
    }

    static void queueInputRange(GeraNESEmu& emu,
                                const DeterministicInputGenerator& generator,
                                uint32_t firstFrame,
                                uint32_t lastFrameInclusive,
                                bool speculative)
    {
        const uint32_t fps = std::max<uint32_t>(1u, emu.getRegionFPS());
        for(uint32_t frame = firstFrame; frame <= lastFrameInclusive; ++frame) {
            uint64_t mask = buildPadMask(generator.buttonsForFrame(frame, fps));
            if(speculative) {
                mask = mutateMaskForSpeculation(mask);
            }
            queuePadMaskForFrame(emu, frame, mask);
        }
    }

    static bool advanceUntilFrame(GeraNESEmu& emu, uint32_t targetFrame, uint32_t maxMs = 20000u)
    {
        uint32_t elapsedMs = 0;
        while(emu.frameCount() < targetFrame && elapsedMs < maxMs) {
            emu.update(1);
            ++elapsedMs;
        }
        return emu.frameCount() == targetFrame;
    }

    static bool advanceForMilliseconds(GeraNESEmu& emu, uint32_t runtimeMs)
    {
        for(uint32_t i = 0; i < runtimeMs; ++i) {
            emu.update(1);
            if(!emu.valid()) return false;
        }
        return true;
    }

    static std::optional<std::vector<uint8_t>> buildRollbackSnapshot(const Options& options,
                                                                     const DeterministicInputGenerator& generator,
                                                                     std::string& failureReason)
    {
        GeraNESEmu emu(DummyAudioOutput::instance());
        if(!emu.open(options.romPath) || !emu.valid()) {
            failureReason = "Failed to open ROM while building rollback snapshot.";
            return std::nullopt;
        }

        queueInputRange(emu, generator, 1u, options.rollbackFrame, false);
        if(!advanceUntilFrame(emu, options.rollbackFrame)) {
            failureReason = "Failed to advance to rollback frame.";
            return std::nullopt;
        }

        return emu.saveStateToMemory();
    }

    static std::optional<RunArtifacts> runReferenceFuture(const Options& options,
                                                          const std::vector<uint8_t>& rollbackSnapshot,
                                                          const DeterministicInputGenerator& generator,
                                                          std::string& failureReason)
    {
        RecordingAudioOutput audioOutput;
        GeraNESEmu emu(audioOutput);
        if(!emu.open(options.romPath) || !emu.valid()) {
            failureReason = "Failed to open ROM for reference future run.";
            return std::nullopt;
        }
        if(!emu.loadStateFromMemoryOnCleanBoot(rollbackSnapshot) || !emu.valid()) {
            failureReason = "Failed to load rollback snapshot for reference future run.";
            return std::nullopt;
        }

        const uint32_t firstFutureFrame = emu.frameCount() + 1u;
        const uint32_t lastFutureFrame = firstFutureFrame + options.futureInputFrames - 1u;
        queueInputRange(emu, generator, firstFutureFrame, lastFutureFrame, false);

        if(!advanceForMilliseconds(emu, options.runtimeMsAfterRollback)) {
            failureReason = "Reference future run failed while advancing.";
            return std::nullopt;
        }

        RunArtifacts artifacts;
        artifacts.point = emu.executionPoint();
        artifacts.crc32 = emu.canonicalStateCrc32();
        artifacts.canonicalResimCrc32 = emu.canonicalNetplayStateCrc32();
        artifacts.snapshot = emu.saveStateToMemory();
        artifacts.canonicalSnapshot = emu.saveNetplayStateToMemory();
        artifacts.audioStats = audioOutput.stats();
        return artifacts;
    }

    static nlohmann::json executionPointToJson(const GeraNESEmu::ExecutionPoint& point)
    {
        return {
            {"frame", point.frame},
            {"cpuCycle", point.cpuCycle},
            {"cpuCyclesRemaining", point.cpuCyclesRemaining},
            {"emulationTick", point.emulationTick}
        };
    }

    static nlohmann::json audioStatsToJson(const RecordingAudioOutput::Stats& stats)
    {
        return {
            {"totalRenderCalls", stats.totalRenderCalls},
            {"audibleRenderCalls", stats.audibleRenderCalls},
            {"silentRenderCalls", stats.silentRenderCalls},
            {"totalRenderedMs", stats.totalRenderedMs}
        };
    }

    static int emitReport(const Options& options, const nlohmann::json& report)
    {
        if(!options.reportPath.empty()) {
            std::ofstream out(options.reportPath, std::ios::binary);
            if(!out) {
                std::cerr << "Failed to write resimulation report: " << options.reportPath << std::endl;
                return RESULT_ERROR;
            }
            out << report.dump(2) << '\n';
            std::cout << options.reportPath << std::endl;
        }
        else {
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
                {"failureReason", "Resimulation test requires a ROM path."}
            });
        }

        if(options.futureInputFrames == 0 || options.runtimeMsAfterRollback == 0 || options.rollbackFrame == 0) {
            return emitReport(options, {
                {"status", "error"},
                {"failureReason", "rollbackFrame, runtimeMsAfterRollback and futureInputFrames must be non-zero."}
            });
        }

        const DeterministicInputGenerator generator(options.seed);

        std::string failureReason;
        const std::optional<std::vector<uint8_t>> rollbackSnapshot =
            buildRollbackSnapshot(options, generator, failureReason);
        if(!rollbackSnapshot.has_value()) {
            return emitReport(options, {
                {"status", "failed"},
                {"failureReason", failureReason}
            });
        }

        const std::optional<RunArtifacts> referenceArtifacts =
            runReferenceFuture(options, *rollbackSnapshot, generator, failureReason);
        if(!referenceArtifacts.has_value()) {
            return emitReport(options, {
                {"status", "failed"},
                {"failureReason", failureReason}
            });
        }

        RecordingAudioOutput speculativeAudioOutput;
        GeraNESEmu speculativeEmu(speculativeAudioOutput);
        if(!speculativeEmu.open(options.romPath) || !speculativeEmu.valid()) {
            return emitReport(options, {
                {"status", "failed"},
                {"failureReason", "Failed to open ROM for speculative run."}
            });
        }
        if(!speculativeEmu.loadStateFromMemoryOnCleanBoot(*rollbackSnapshot) || !speculativeEmu.valid()) {
            return emitReport(options, {
                {"status", "failed"},
                {"failureReason", "Failed to load rollback snapshot for speculative run."}
            });
        }

        const uint32_t speculativeFirstFrame = speculativeEmu.frameCount() + 1u;
        const uint32_t speculativeLastFrame = speculativeFirstFrame + options.futureInputFrames - 1u;
        queueInputRange(speculativeEmu, generator, speculativeFirstFrame, speculativeLastFrame, true);

        if(!advanceForMilliseconds(speculativeEmu, options.runtimeMsAfterRollback)) {
            return emitReport(options, {
                {"status", "failed"},
                {"failureReason", "Speculative run failed while advancing."}
            });
        }

        const GeraNESEmu::ExecutionPoint speculativePoint = speculativeEmu.executionPoint();
        const uint32_t speculativeCrc32 = speculativeEmu.canonicalStateCrc32();
        const RecordingAudioOutput::Stats speculativeAudioBeforeRollback = speculativeAudioOutput.stats();
        const bool speculativeDivergedFromReference = speculativeCrc32 != referenceArtifacts->crc32;

        speculativeAudioOutput.resetStats();
        speculativeEmu.loadStateFromMemory(*rollbackSnapshot);
        const uint32_t correctedFirstFrame = speculativeEmu.frameCount() + 1u;
        const uint32_t correctedLastFrame = correctedFirstFrame + options.futureInputFrames - 1u;
        queueInputRange(speculativeEmu, generator, correctedFirstFrame, correctedLastFrame, false);

        const bool resimulated =
            speculativeEmu.resimulateSilentlyToExecutionPoint(speculativePoint);
        const GeraNESEmu::ExecutionPoint resimulatedPoint = speculativeEmu.executionPoint();
        const uint32_t resimulatedCrc32 = speculativeEmu.canonicalStateCrc32();
        const uint32_t resimulatedCanonicalResimCrc32 = speculativeEmu.canonicalNetplayStateCrc32();
        const std::vector<uint8_t> resimulatedSnapshot = speculativeEmu.saveStateToMemory();
        const std::vector<uint8_t> resimulatedCanonicalSnapshot = speculativeEmu.saveNetplayStateToMemory();
        const RecordingAudioOutput::Stats resimulationAudioStats = speculativeAudioOutput.stats();

        const bool pointMatch =
            resimulatedPoint.frame == referenceArtifacts->point.frame &&
            resimulatedPoint.emulationTick == referenceArtifacts->point.emulationTick;
        const bool exactTargetReached =
            resimulatedPoint.frame == speculativePoint.frame &&
            resimulatedPoint.emulationTick == speculativePoint.emulationTick;
        const bool stateMatch = resimulatedCanonicalSnapshot == referenceArtifacts->canonicalSnapshot;
        const bool fullStateMatch = resimulatedSnapshot == referenceArtifacts->snapshot;
        const bool audioStayedSilent = resimulationAudioStats.audibleRenderCalls == 0;

        const bool ok = resimulated &&
                        exactTargetReached &&
                        pointMatch &&
                        stateMatch &&
                        audioStayedSilent &&
                        referenceArtifacts->audioStats.audibleRenderCalls > 0 &&
                        speculativeAudioBeforeRollback.audibleRenderCalls > 0;

        nlohmann::json report = {
            {"status", ok ? "ok" : "failed"},
            {"romPath", options.romPath},
            {"rollbackFrame", options.rollbackFrame},
            {"runtimeMsAfterRollback", options.runtimeMsAfterRollback},
            {"futureInputFrames", options.futureInputFrames},
            {"seed", options.seed},
            {"referenceExecutionPoint", executionPointToJson(referenceArtifacts->point)},
            {"speculativeExecutionPoint", executionPointToJson(speculativePoint)},
            {"resimulatedExecutionPoint", executionPointToJson(resimulatedPoint)},
            {"referenceCrc32", referenceArtifacts->crc32},
            {"referenceCanonicalResimCrc32", referenceArtifacts->canonicalResimCrc32},
            {"speculativeCrc32", speculativeCrc32},
            {"resimulatedCrc32", resimulatedCrc32},
            {"resimulatedCanonicalResimCrc32", resimulatedCanonicalResimCrc32},
            {"referenceAudio", audioStatsToJson(referenceArtifacts->audioStats)},
            {"speculativeAudioBeforeRollback", audioStatsToJson(speculativeAudioBeforeRollback)},
            {"resimulationAudio", audioStatsToJson(resimulationAudioStats)},
            {"speculativeDivergedFromReference", speculativeDivergedFromReference},
            {"resimulated", resimulated},
            {"exactTargetReached", exactTargetReached},
            {"referencePointMatch", pointMatch},
            {"stateMatch", stateMatch},
            {"fullStateMatch", fullStateMatch},
            {"audioStayedSilent", audioStayedSilent}
        };

        if(!ok) {
            std::string reason;
            if(!resimulated) reason = "Silent resimulation did not complete.";
            else if(!exactTargetReached) reason = "Silent resimulation failed to stop at the requested execution point.";
            else if(!pointMatch) reason = "Resimulated execution point does not match the clean reference run.";
            else if(!stateMatch) reason = "Resimulated state does not match the clean reference snapshot.";
            else if(!audioStayedSilent) reason = "Silent resimulation emitted audible audio renders.";
            else if(referenceArtifacts->audioStats.audibleRenderCalls == 0) reason = "Reference run did not render audible audio.";
            else if(speculativeAudioBeforeRollback.audibleRenderCalls == 0) reason = "Speculative run did not render audible audio.";
            else reason = "Unknown resimulation failure.";
            report["failureReason"] = reason;
        }

        return emitReport(options, report);
    }
};

#endif
