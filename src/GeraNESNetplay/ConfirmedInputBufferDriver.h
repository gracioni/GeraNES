#pragma once

#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/EmulationHost.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
#include "ConsoleNetplay/NetplayInputAssignment.h"

#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace ConsoleNetplay
{

class ConfirmedInputBufferDriver
{
public:
    struct PlaybackQueueStats
    {
        uint64_t rebuildCount = 0;
        uint64_t totalBuiltFrames = 0;
        uint64_t maxBuiltFrames = 0;
        uint64_t lastBuiltFrames = 0;
        uint32_t lastPreparedThroughFrame = 0;

        void record(size_t builtFrames, uint32_t preparedThroughFrame)
        {
            ++rebuildCount;
            const uint64_t built = static_cast<uint64_t>(builtFrames);
            lastBuiltFrames = built;
            totalBuiltFrames += built;
            if(built > maxBuiltFrames) {
                maxBuiltFrames = built;
            }
            lastPreparedThroughFrame = preparedThroughFrame;
        }
    };

private:
    double m_inputProductionAccumulatorMs = 0.0;
    uint32_t m_producedThroughFrame = 0;
    uint32_t m_queuedThroughFrame = 0;
    uint32_t m_prebufferFrames = 10;
    uint32_t m_predictFrames = 0;
    bool m_lastProduceHadLocalSlots = false;
    mutable std::mutex m_pendingFramesMutex;
    std::deque<NetplayCoordinator::ConfirmedFrameInputs> m_pendingFrames;
    PlaybackQueueStats m_playbackQueueStats;

    void seedInitialPrebufferIfNeeded(NetplayCoordinator& coordinator,
                                      const std::vector<PlayerSlot>& localSlots,
                                      const EmulationHost::InputState& localInputState,
                                      const RoomState& room);

public:
    static void applyPadMaskToInputState(EmulationHost::InputState& state, PlayerSlot slot, uint64_t mask);
    static void applyPadMaskToInputFrame(InputFrame& inputFrame, PlayerSlot slot, uint64_t mask);
    static void applyInputFrameToInputState(EmulationHost::InputState& state, const InputFrame& inputFrame);

    uint32_t producedThroughFrame() const;
    uint32_t queuedThroughFrame() const;
    size_t pendingFrameCount() const;
    uint32_t prebufferFrames() const;
    void setPrebufferFrames(uint32_t frames);
    uint32_t predictFrames() const;
    void setPredictFrames(uint32_t frames);
    PlaybackQueueStats playbackQueueStats() const;
    void reset();
    void reanchor(uint32_t frame);

    static uint64_t buildPadMask(bool a, bool b, bool select, bool start,
                                 bool up, bool down, bool left, bool right,
                                 bool x = false, bool y = false, bool l = false, bool r = false,
                                 bool up2 = false, bool down2 = false, bool left2 = false, bool right2 = false);

    void produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                    bool active,
                                    bool awaitingSync,
                                    SessionState state,
                                    std::optional<PlayerSlot> localSlot,
                                    uint32_t dtMs,
                                    const EmulationHost::InputState& localInputState,
                                    const RoomState& room,
                                    uint32_t regionFps,
                                    uint32_t exactFrame,
                                    uint32_t confirmedThroughFrame);

    void produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                    bool active,
                                    bool awaitingSync,
                                    SessionState state,
                                    const std::vector<PlayerSlot>& localSlots,
                                    uint32_t dtMs,
                                    const EmulationHost::InputState& localInputState,
                                    const RoomState& room,
                                    uint32_t regionFps,
                                    uint32_t exactFrame,
                                    uint32_t confirmedThroughFrame);

    void produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                    bool active,
                                    bool awaitingSync,
                                    SessionState state,
                                    std::optional<PlayerSlot> localSlot,
                                    uint32_t dtMs,
                                    uint64_t localPrimaryMask,
                                    uint32_t regionFps,
                                    uint32_t exactFrame,
                                    uint32_t confirmedThroughFrame);

    void produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                    bool active,
                                    bool awaitingSync,
                                    SessionState state,
                                    const std::vector<PlayerSlot>& localSlots,
                                    uint32_t dtMs,
                                    uint64_t localPrimaryMask,
                                    uint32_t regionFps,
                                    uint32_t exactFrame,
                                    uint32_t confirmedThroughFrame);

    uint32_t confirmedThroughFrame(const NetplayCoordinator& coordinator) const;

    EmulationHost::InputState buildReplayInputState(const NetplayCoordinator& coordinator, FrameNumber frame) const;

    bool tryBuildConfirmedInputState(const NetplayCoordinator& coordinator,
                                     bool active,
                                     bool awaitingSync,
                                     SessionState state,
                                     FrameNumber frame,
                                     EmulationHost::InputState& outState) const;

    void preparePlaybackFramesForEmulationThread(NetplayCoordinator& coordinator,
                                                 bool active,
                                                 bool awaitingSync,
                                                 SessionState state,
                                                 uint32_t emulationFrame);

    void prepareConfirmedFramesForEmulationThread(NetplayCoordinator& coordinator,
                                                  bool active,
                                                  bool awaitingSync,
                                                  SessionState state,
                                                  uint32_t emulationFrame);

    void queuePendingFramesToEmu(GeraNESEmu& emu);
};

}
