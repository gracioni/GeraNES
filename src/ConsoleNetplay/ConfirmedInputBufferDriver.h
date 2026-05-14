#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

#include "ConsoleNetplay/NetplayCoordinator.h"

namespace ConsoleNetplay
{

class ConfirmedInputBufferDriver
{
public:
    using LocalInputBuilder = std::function<NetplayInputFrame(PlayerSlot, FrameNumber, const RoomState&)>;
    using PendingFrameConsumer = std::function<void(const NetplayCoordinator::ConfirmedFrameInputs&)>;

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
                                      const RoomState& room,
                                      const LocalInputBuilder& buildLocalInput);

public:
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

    static NetplayInputFrame buildMaskContribution(PlayerSlot slot,
                                                   FrameNumber frame,
                                                   uint32_t timelineEpoch,
                                                   uint64_t mask);

    void produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                    bool active,
                                    bool awaitingSync,
                                    SessionState state,
                                    std::optional<PlayerSlot> localSlot,
                                    // Reserved for callers that still sample input on a wall-clock cadence.
                                    // Current production is frame-target based, so these are compatibility inputs.
                                    uint32_t dtMs,
                                    const RoomState& room,
                                    const LocalInputBuilder& buildLocalInput,
                                    uint32_t regionFps,
                                    uint32_t exactFrame,
                                    uint32_t confirmedThroughFrame);

    void produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                    bool active,
                                    bool awaitingSync,
                                    SessionState state,
                                    const std::vector<PlayerSlot>& localSlots,
                                    // Reserved for callers that still sample input on a wall-clock cadence.
                                    // Current production is frame-target based, so these are compatibility inputs.
                                    uint32_t dtMs,
                                    const RoomState& room,
                                    const LocalInputBuilder& buildLocalInput,
                                    uint32_t regionFps,
                                    uint32_t exactFrame,
                                    uint32_t confirmedThroughFrame);

    void produceLocalBufferedInputMasks(NetplayCoordinator& coordinator,
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

    void produceLocalBufferedInputMasks(NetplayCoordinator& coordinator,
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

    bool tryBuildConfirmedInputFrame(const NetplayCoordinator& coordinator,
                                     bool active,
                                     bool awaitingSync,
                                     SessionState state,
                                     FrameNumber frame,
                                     NetplayInputFrame& outFrame) const;

    void preparePlaybackFramesForEmulationThread(NetplayCoordinator& coordinator,
                                                 bool active,
                                                 bool awaitingSync,
                                                 SessionState state,
                                                 uint32_t emulationFrame,
                                                 std::optional<FrameNumber> maxPlaybackFrame = std::nullopt);

    void prepareConfirmedFramesForEmulationThread(NetplayCoordinator& coordinator,
                                                  bool active,
                                                  bool awaitingSync,
                                                  SessionState state,
                                                  uint32_t emulationFrame);

    void consumePendingFrames(FrameNumber currentFrame,
                              FrameNumber queueLimitFrame,
                              const PendingFrameConsumer& consumer);
};

}
