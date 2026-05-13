#include "ConsoleNetplay/ConfirmedInputBufferDriver.h"

#include <algorithm>
#include <utility>

#include "ConsoleNetplay/NetplayInputAssignment.h"

namespace ConsoleNetplay
{

namespace {

bool hostWaitingOnRemotePlayableInput(const NetplayCoordinator& coordinator, bool hasLocalSlots)
{
    if(!coordinator.isHosting() || hasLocalSlots) {
        return false;
    }

    const ParticipantId localParticipantId = coordinator.localParticipantId();
    const RoomState& room = coordinator.session().roomState();
    return std::any_of(
        room.participants.begin(),
        room.participants.end(),
        [localParticipantId](const ParticipantInfo& participant) {
            return participant.id != localParticipantId &&
                   participant.connected &&
                   !participantIsObserver(participant);
        }
    );
}

} // namespace

void ConfirmedInputBufferDriver::seedInitialPrebufferIfNeeded(NetplayCoordinator& coordinator,
                                                              const std::vector<PlayerSlot>& localSlots,
                                                              const RoomState& room,
                                                              const LocalInputBuilder& buildLocalInput)
{
    while(m_producedThroughFrame < m_prebufferFrames) {
        ++m_producedThroughFrame;
        for(PlayerSlot localSlot : localSlots) {
            coordinator.recordLocalInputFrame(
                m_producedThroughFrame,
                localSlot,
                buildLocalInput(localSlot, m_producedThroughFrame, room)
            );
        }
    }
}

uint32_t ConfirmedInputBufferDriver::producedThroughFrame() const
{
    return m_producedThroughFrame;
}

uint32_t ConfirmedInputBufferDriver::queuedThroughFrame() const
{
    return m_queuedThroughFrame;
}

size_t ConfirmedInputBufferDriver::pendingFrameCount() const
{
    std::scoped_lock pendingLock(m_pendingFramesMutex);
    return m_pendingFrames.size();
}

uint32_t ConfirmedInputBufferDriver::prebufferFrames() const
{
    return m_prebufferFrames;
}

void ConfirmedInputBufferDriver::setPrebufferFrames(uint32_t frames)
{
    m_prebufferFrames = frames;
}

uint32_t ConfirmedInputBufferDriver::predictFrames() const
{
    return m_predictFrames;
}

void ConfirmedInputBufferDriver::setPredictFrames(uint32_t frames)
{
    m_predictFrames = frames;
}

ConfirmedInputBufferDriver::PlaybackQueueStats ConfirmedInputBufferDriver::playbackQueueStats() const
{
    return m_playbackQueueStats;
}

void ConfirmedInputBufferDriver::reset()
{
    m_inputProductionAccumulatorMs = 0.0;
    m_producedThroughFrame = 0;
    m_queuedThroughFrame = 0;
    m_lastProduceHadLocalSlots = false;
    std::scoped_lock pendingLock(m_pendingFramesMutex);
    m_pendingFrames.clear();
}

void ConfirmedInputBufferDriver::reanchor(uint32_t frame)
{
    m_inputProductionAccumulatorMs = 0.0;
    m_producedThroughFrame = frame;
    m_queuedThroughFrame = frame;
    m_lastProduceHadLocalSlots = false;
    std::scoped_lock pendingLock(m_pendingFramesMutex);
    m_pendingFrames.clear();
}

uint64_t ConfirmedInputBufferDriver::buildPadMask(bool a, bool b, bool select, bool start,
                                                  bool up, bool down, bool left, bool right,
                                                  bool x, bool y, bool l, bool r,
                                                  bool up2, bool down2, bool left2, bool right2)
{
    uint64_t mask = 0;
    if(a) mask |= (1ull << 0);
    if(b) mask |= (1ull << 1);
    if(select) mask |= (1ull << 2);
    if(start) mask |= (1ull << 3);
    if(up) mask |= (1ull << 4);
    if(down) mask |= (1ull << 5);
    if(left) mask |= (1ull << 6);
    if(right) mask |= (1ull << 7);
    if(x) mask |= (1ull << 8);
    if(y) mask |= (1ull << 9);
    if(l) mask |= (1ull << 10);
    if(r) mask |= (1ull << 11);
    if(up2) mask |= (1ull << 12);
    if(down2) mask |= (1ull << 13);
    if(left2) mask |= (1ull << 14);
    if(right2) mask |= (1ull << 15);
    return mask;
}

NetplayInputFrame ConfirmedInputBufferDriver::buildMaskContribution(PlayerSlot slot,
                                                                    FrameNumber frame,
                                                                    uint32_t timelineEpoch,
                                                                    uint64_t mask)
{
    NetplayInputFrame input;
    input.frame = frame;
    input.timelineEpoch = timelineEpoch;
    input.buttonMaskLo[slot] = mask;
    return input;
}

void ConfirmedInputBufferDriver::produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                                            bool active,
                                                            bool awaitingSync,
                                                            SessionState state,
                                                            std::optional<PlayerSlot> localSlot,
                                                            uint32_t dtMs,
                                                            const RoomState& room,
                                                            const LocalInputBuilder& buildLocalInput,
                                                            uint32_t regionFps,
                                                            uint32_t exactFrame,
                                                            uint32_t confirmedThroughFrame)
{
    std::vector<PlayerSlot> localSlots;
    if(localSlot.has_value()) {
        localSlots.push_back(*localSlot);
    }
    produceLocalBufferedInputs(
        coordinator,
        active,
        awaitingSync,
        state,
        localSlots,
        dtMs,
        room,
        buildLocalInput,
        regionFps,
        exactFrame,
        confirmedThroughFrame
    );
}

void ConfirmedInputBufferDriver::produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                                            bool active,
                                                            bool awaitingSync,
                                                            SessionState state,
                                                            const std::vector<PlayerSlot>& localSlots,
                                                            uint32_t dtMs,
                                                            const RoomState& room,
                                                            const LocalInputBuilder& buildLocalInput,
                                                            uint32_t regionFps,
                                                            uint32_t exactFrame,
                                                            uint32_t confirmedThroughFrame)
{
    if(!active) {
        reset();
        return;
    }
    if(awaitingSync) {
        m_inputProductionAccumulatorMs = 0.0;
        m_lastProduceHadLocalSlots = false;
        std::scoped_lock pendingLock(m_pendingFramesMutex);
        m_pendingFrames.clear();
        return;
    }
    if(state != SessionState::Running) {
        m_inputProductionAccumulatorMs = 0.0;
        return;
    }

    m_lastProduceHadLocalSlots = !localSlots.empty();

    if(localSlots.empty()) {
        const bool waitForRemotePlayableInput =
            hostWaitingOnRemotePlayableInput(coordinator, false);
        const uint32_t targetBufferedThroughFrame =
            waitForRemotePlayableInput
                ? confirmedThroughFrame
                : exactFrame + m_prebufferFrames;
        if(m_producedThroughFrame < targetBufferedThroughFrame) {
            m_producedThroughFrame = targetBufferedThroughFrame;
        }
        if(m_queuedThroughFrame > m_producedThroughFrame) {
            m_queuedThroughFrame = m_producedThroughFrame;
        }
        m_inputProductionAccumulatorMs = 0.0;
        return;
    }

    (void)dtMs;
    (void)regionFps;

    if(coordinator.isHosting() && m_producedThroughFrame < confirmedThroughFrame) {
        m_producedThroughFrame = confirmedThroughFrame;
    }
    if(coordinator.isHosting() && m_queuedThroughFrame < confirmedThroughFrame) {
        m_queuedThroughFrame = confirmedThroughFrame;
    }

    seedInitialPrebufferIfNeeded(coordinator, localSlots, room, buildLocalInput);

    const uint32_t targetBufferedThroughFrame = exactFrame + m_prebufferFrames;

    while(m_producedThroughFrame < targetBufferedThroughFrame) {
        ++m_producedThroughFrame;
        for(PlayerSlot localSlot : localSlots) {
            coordinator.recordLocalInputFrame(
                m_producedThroughFrame,
                localSlot,
                buildLocalInput(localSlot, m_producedThroughFrame, room)
            );
        }
    }
    m_inputProductionAccumulatorMs = 0.0;
}

void ConfirmedInputBufferDriver::produceLocalBufferedInputMasks(NetplayCoordinator& coordinator,
                                                                bool active,
                                                                bool awaitingSync,
                                                                SessionState state,
                                                                std::optional<PlayerSlot> localSlot,
                                                                uint32_t dtMs,
                                                                uint64_t localPrimaryMask,
                                                                uint32_t regionFps,
                                                                uint32_t exactFrame,
                                                                uint32_t confirmedThroughFrame)
{
    std::vector<PlayerSlot> localSlots;
    if(localSlot.has_value()) {
        localSlots.push_back(*localSlot);
    }
    produceLocalBufferedInputMasks(
        coordinator,
        active,
        awaitingSync,
        state,
        localSlots,
        dtMs,
        localPrimaryMask,
        regionFps,
        exactFrame,
        confirmedThroughFrame
    );
}

void ConfirmedInputBufferDriver::produceLocalBufferedInputMasks(NetplayCoordinator& coordinator,
                                                                bool active,
                                                                bool awaitingSync,
                                                                SessionState state,
                                                                const std::vector<PlayerSlot>& localSlots,
                                                                uint32_t dtMs,
                                                                uint64_t localPrimaryMask,
                                                                uint32_t regionFps,
                                                                uint32_t exactFrame,
                                                                uint32_t confirmedThroughFrame)
{
    const RoomState room = coordinator.session().roomState();
    const auto buildLocalInput = [localPrimaryMask](PlayerSlot localSlot, FrameNumber frame, const RoomState& roomState) {
        return buildMaskContribution(localSlot, frame, roomState.timelineEpoch, localPrimaryMask);
    };
    produceLocalBufferedInputs(
        coordinator,
        active,
        awaitingSync,
        state,
        localSlots,
        dtMs,
        room,
        buildLocalInput,
        regionFps,
        exactFrame,
        confirmedThroughFrame
    );
}

void ConfirmedInputBufferDriver::produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                                            bool active,
                                                            bool awaitingSync,
                                                            SessionState state,
                                                            std::optional<PlayerSlot> localSlot,
                                                            uint32_t dtMs,
                                                            uint64_t localPrimaryMask,
                                                            uint32_t regionFps,
                                                            uint32_t exactFrame,
                                                            uint32_t confirmedThroughFrame)
{
    produceLocalBufferedInputMasks(
        coordinator,
        active,
        awaitingSync,
        state,
        localSlot,
        dtMs,
        localPrimaryMask,
        regionFps,
        exactFrame,
        confirmedThroughFrame
    );
}

void ConfirmedInputBufferDriver::produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                                            bool active,
                                                            bool awaitingSync,
                                                            SessionState state,
                                                            const std::vector<PlayerSlot>& localSlots,
                                                            uint32_t dtMs,
                                                            uint64_t localPrimaryMask,
                                                            uint32_t regionFps,
                                                            uint32_t exactFrame,
                                                            uint32_t confirmedThroughFrame)
{
    produceLocalBufferedInputMasks(
        coordinator,
        active,
        awaitingSync,
        state,
        localSlots,
        dtMs,
        localPrimaryMask,
        regionFps,
        exactFrame,
        confirmedThroughFrame
    );
}

uint32_t ConfirmedInputBufferDriver::confirmedThroughFrame(const NetplayCoordinator& coordinator) const
{
    return coordinator.isActive() ? coordinator.latestConfirmedFrame() : 0u;
}

bool ConfirmedInputBufferDriver::tryBuildConfirmedInputFrame(const NetplayCoordinator& coordinator,
                                                             bool active,
                                                             bool awaitingSync,
                                                             SessionState state,
                                                             FrameNumber frame,
                                                             NetplayInputFrame& outFrame) const
{
    if(!active || awaitingSync || state != SessionState::Running) return false;
    if(frame > confirmedThroughFrame(coordinator)) return false;
    const NetplayCoordinator::ConfirmedFrameInputs* confirmed = coordinator.findConfirmedFrame(frame);
    if(confirmed == nullptr) return false;
    outFrame = confirmed->netplayFrame;
    return true;
}

void ConfirmedInputBufferDriver::preparePlaybackFramesForEmulationThread(NetplayCoordinator& coordinator,
                                                                         bool active,
                                                                         bool awaitingSync,
                                                                         SessionState state,
                                                                         uint32_t emulationFrame,
                                                                         std::optional<FrameNumber> maxPlaybackFrame)
{
    if(!active) {
        reset();
        return;
    }
    if(awaitingSync) {
        std::scoped_lock pendingLock(m_pendingFramesMutex);
        m_pendingFrames.clear();
        return;
    }
    if(state != SessionState::Running) {
        std::scoped_lock pendingLock(m_pendingFramesMutex);
        m_pendingFrames.clear();
        return;
    }

    const uint32_t confirmedFrame = confirmedThroughFrame(coordinator);
    const uint32_t delaySlackFrame = confirmedFrame + m_prebufferFrames;
    const uint32_t predictedThroughFrame = delaySlackFrame + m_predictFrames;
    const uint32_t queueHorizonFrame = emulationFrame + (m_prebufferFrames * 2u) + m_predictFrames + 1u;
    const bool waitForRemotePlayableInput =
        hostWaitingOnRemotePlayableInput(coordinator, m_lastProduceHadLocalSlots);
    const uint32_t hostFallbackThroughFrame =
        coordinator.isHosting() && !m_lastProduceHadLocalSlots && !waitForRemotePlayableInput
            ? queueHorizonFrame
            : m_producedThroughFrame;
    const uint32_t playableThroughFrame =
        coordinator.isHosting()
            ? hostFallbackThroughFrame
            : std::min(m_producedThroughFrame, predictedThroughFrame);
    const uint32_t firstFrame = emulationFrame + 1u;
    uint32_t targetThroughFrame = std::min(playableThroughFrame, queueHorizonFrame);
    if(maxPlaybackFrame.has_value()) {
        targetThroughFrame = std::min<uint32_t>(targetThroughFrame, *maxPlaybackFrame);
    }
    const auto allowPredictionForFrame = [delaySlackFrame, predictedThroughFrame, waitForRemotePlayableInput](uint32_t frame) {
        return !waitForRemotePlayableInput &&
               frame > delaySlackFrame &&
               frame <= predictedThroughFrame;
    };
    const auto allowHostFallbackForFrame = [predictedThroughFrame, waitForRemotePlayableInput](uint32_t frame) {
        return !waitForRemotePlayableInput && frame > predictedThroughFrame;
    };

    std::scoped_lock pendingLock(m_pendingFramesMutex);
    while(!m_pendingFrames.empty() && m_pendingFrames.front().frame < firstFrame) {
        m_pendingFrames.pop_front();
    }

    if(targetThroughFrame < firstFrame) {
        m_pendingFrames.clear();
        m_queuedThroughFrame = emulationFrame;
        m_playbackQueueStats.record(0, m_queuedThroughFrame);
        return;
    }

    while(!m_pendingFrames.empty() && m_pendingFrames.back().frame > targetThroughFrame) {
        m_pendingFrames.pop_back();
    }

    for(auto it = m_pendingFrames.begin(); it != m_pendingFrames.end();) {
        NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
        const bool allowPrediction = allowPredictionForFrame(it->frame);
        if(!coordinator.tryBuildPlaybackFrame(
               it->frame,
               allowPrediction,
               playbackFrame,
               allowHostFallbackForFrame(it->frame)
           )) {
            m_pendingFrames.erase(it, m_pendingFrames.end());
            break;
        }
        *it = std::move(playbackFrame);
        ++it;
    }

    uint32_t nextFrame = firstFrame;
    if(!m_pendingFrames.empty()) {
        if(m_pendingFrames.front().frame != firstFrame) {
            m_pendingFrames.clear();
        } else {
            nextFrame = m_pendingFrames.back().frame + 1u;
        }
    }

    for(uint32_t frame = nextFrame; frame <= targetThroughFrame; ++frame) {
        NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
        const bool allowPrediction = allowPredictionForFrame(frame);
        if(!coordinator.tryBuildPlaybackFrame(
               frame,
               allowPrediction,
               playbackFrame,
               allowHostFallbackForFrame(frame)
           )) {
            break;
        }
        m_pendingFrames.push_back(std::move(playbackFrame));
    }

    m_queuedThroughFrame = m_pendingFrames.empty() ? emulationFrame : m_pendingFrames.back().frame;
    m_playbackQueueStats.record(m_pendingFrames.size(), m_queuedThroughFrame);
}

void ConfirmedInputBufferDriver::prepareConfirmedFramesForEmulationThread(NetplayCoordinator& coordinator,
                                                                          bool active,
                                                                          bool awaitingSync,
                                                                          SessionState state,
                                                                          uint32_t emulationFrame)
{
    preparePlaybackFramesForEmulationThread(coordinator, active, awaitingSync, state, emulationFrame);
}

void ConfirmedInputBufferDriver::consumePendingFrames(FrameNumber currentFrame,
                                                      FrameNumber queueLimitFrame,
                                                      const PendingFrameConsumer& consumer)
{
    std::scoped_lock pendingLock(m_pendingFramesMutex);
    while(!m_pendingFrames.empty() && m_pendingFrames.front().frame < currentFrame) {
        m_pendingFrames.pop_front();
    }

    for(const NetplayCoordinator::ConfirmedFrameInputs& confirmed : m_pendingFrames) {
        if(confirmed.frame > queueLimitFrame) {
            break;
        }
        consumer(confirmed);
    }
}

} // namespace ConsoleNetplay
