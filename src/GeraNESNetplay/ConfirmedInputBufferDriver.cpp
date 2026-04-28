#include "GeraNESNetplay/ConfirmedInputBufferDriver.h"

#include <algorithm>
#include <utility>

namespace Netplay
{

namespace {

struct PadButtons
{
    bool a = false;
    bool b = false;
    bool select = false;
    bool start = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool x = false;
    bool y = false;
    bool l = false;
    bool r = false;
    bool up2 = false;
    bool down2 = false;
    bool left2 = false;
    bool right2 = false;
};

PadButtons decodePadMask(uint64_t mask)
{
    PadButtons buttons;
    buttons.a = (mask & (1ull << 0)) != 0;
    buttons.b = (mask & (1ull << 1)) != 0;
    buttons.select = (mask & (1ull << 2)) != 0;
    buttons.start = (mask & (1ull << 3)) != 0;
    buttons.up = (mask & (1ull << 4)) != 0;
    buttons.down = (mask & (1ull << 5)) != 0;
    buttons.left = (mask & (1ull << 6)) != 0;
    buttons.right = (mask & (1ull << 7)) != 0;
    buttons.x = (mask & (1ull << 8)) != 0;
    buttons.y = (mask & (1ull << 9)) != 0;
    buttons.l = (mask & (1ull << 10)) != 0;
    buttons.r = (mask & (1ull << 11)) != 0;
    buttons.up2 = (mask & (1ull << 12)) != 0;
    buttons.down2 = (mask & (1ull << 13)) != 0;
    buttons.left2 = (mask & (1ull << 14)) != 0;
    buttons.right2 = (mask & (1ull << 15)) != 0;
    return buttons;
}

} // namespace

void ConfirmedInputBufferDriver::seedInitialPrebufferIfNeeded(NetplayCoordinator& coordinator,
                                                               const std::vector<PlayerSlot>& localSlots,
                                                               const EmulationHost::InputState& localInputState,
                                                               const RoomState& room)
{
    while(m_producedThroughFrame < m_prebufferFrames) {
        ++m_producedThroughFrame;
        for(PlayerSlot localSlot : localSlots) {
            coordinator.recordLocalInputFrame(
                m_producedThroughFrame,
                localSlot,
                buildAssignedContribution(localSlot, localInputState, makeRoomTopologyBaseFrame(m_producedThroughFrame, room))
            );
        }
    }
}

void ConfirmedInputBufferDriver::applyPadMaskToInputState(EmulationHost::InputState& state, PlayerSlot slot, uint64_t mask)
{
    const PadButtons buttons = decodePadMask(mask);
    switch(slot) {
        case kPort1PlayerSlot:
        case kMultitapP1PlayerSlot:
            state.p1A = buttons.a; state.p1B = buttons.b; state.p1Select = buttons.select; state.p1Start = buttons.start;
            state.p1Up = buttons.up; state.p1Down = buttons.down; state.p1Left = buttons.left; state.p1Right = buttons.right;
            state.p1X = buttons.x; state.p1Y = buttons.y; state.p1L = buttons.l; state.p1R = buttons.r;
            state.p1Up2 = buttons.up2; state.p1Down2 = buttons.down2; state.p1Left2 = buttons.left2; state.p1Right2 = buttons.right2;
            break;
        case kPort2PlayerSlot:
        case kMultitapP2PlayerSlot:
            state.p2A = buttons.a; state.p2B = buttons.b; state.p2Select = buttons.select; state.p2Start = buttons.start;
            state.p2Up = buttons.up; state.p2Down = buttons.down; state.p2Left = buttons.left; state.p2Right = buttons.right;
            state.p2X = buttons.x; state.p2Y = buttons.y; state.p2L = buttons.l; state.p2R = buttons.r;
            state.p2Up2 = buttons.up2; state.p2Down2 = buttons.down2; state.p2Left2 = buttons.left2; state.p2Right2 = buttons.right2;
            break;
        case kExpansionPlayerSlot:
        case kMultitapP3PlayerSlot:
            state.p3A = buttons.a; state.p3B = buttons.b; state.p3Select = buttons.select; state.p3Start = buttons.start;
            state.p3Up = buttons.up; state.p3Down = buttons.down; state.p3Left = buttons.left; state.p3Right = buttons.right;
            break;
        case kMultitapP4PlayerSlot:
            state.p4A = buttons.a; state.p4B = buttons.b; state.p4Select = buttons.select; state.p4Start = buttons.start;
            state.p4Up = buttons.up; state.p4Down = buttons.down; state.p4Left = buttons.left; state.p4Right = buttons.right;
            break;
        default:
            break;
    }
}

void ConfirmedInputBufferDriver::applyPadMaskToInputFrame(InputFrame& inputFrame, PlayerSlot slot, uint64_t mask)
{
    const PadButtons buttons = decodePadMask(mask);

    switch(slot) {
        case kPort1PlayerSlot:
        case kMultitapP1PlayerSlot:
            inputFrame.p1A = buttons.a; inputFrame.p1B = buttons.b; inputFrame.p1Select = buttons.select; inputFrame.p1Start = buttons.start;
            inputFrame.p1Up = buttons.up; inputFrame.p1Down = buttons.down; inputFrame.p1Left = buttons.left; inputFrame.p1Right = buttons.right;
            inputFrame.p1X = buttons.x; inputFrame.p1Y = buttons.y; inputFrame.p1L = buttons.l; inputFrame.p1R = buttons.r;
            inputFrame.vbP1Up1 = buttons.up2; inputFrame.vbP1Down1 = buttons.down2; inputFrame.vbP1Left1 = buttons.left2; inputFrame.vbP1Right1 = buttons.right2;
            break;
        case kPort2PlayerSlot:
        case kMultitapP2PlayerSlot:
            inputFrame.p2A = buttons.a; inputFrame.p2B = buttons.b; inputFrame.p2Select = buttons.select; inputFrame.p2Start = buttons.start;
            inputFrame.p2Up = buttons.up; inputFrame.p2Down = buttons.down; inputFrame.p2Left = buttons.left; inputFrame.p2Right = buttons.right;
            inputFrame.p2X = buttons.x; inputFrame.p2Y = buttons.y; inputFrame.p2L = buttons.l; inputFrame.p2R = buttons.r;
            inputFrame.vbP2Up1 = buttons.up2; inputFrame.vbP2Down1 = buttons.down2; inputFrame.vbP2Left1 = buttons.left2; inputFrame.vbP2Right1 = buttons.right2;
            break;
        case kExpansionPlayerSlot:
        case kMultitapP3PlayerSlot:
            inputFrame.p3A = buttons.a; inputFrame.p3B = buttons.b; inputFrame.p3Select = buttons.select; inputFrame.p3Start = buttons.start;
            inputFrame.p3Up = buttons.up; inputFrame.p3Down = buttons.down; inputFrame.p3Left = buttons.left; inputFrame.p3Right = buttons.right;
            break;
        case kMultitapP4PlayerSlot:
            inputFrame.p4A = buttons.a; inputFrame.p4B = buttons.b; inputFrame.p4Select = buttons.select; inputFrame.p4Start = buttons.start;
            inputFrame.p4Up = buttons.up; inputFrame.p4Down = buttons.down; inputFrame.p4Left = buttons.left; inputFrame.p4Right = buttons.right;
            break;
        default:
            break;
    }
}

void ConfirmedInputBufferDriver::applyInputFrameToInputState(EmulationHost::InputState& state, const InputFrame& inputFrame)
{
    state.p1A = inputFrame.p1A; state.p1B = inputFrame.p1B; state.p1Select = inputFrame.p1Select; state.p1Start = inputFrame.p1Start;
    state.p1Up = inputFrame.p1Up; state.p1Down = inputFrame.p1Down; state.p1Left = inputFrame.p1Left; state.p1Right = inputFrame.p1Right;
    state.p1X = inputFrame.p1X; state.p1Y = inputFrame.p1Y; state.p1L = inputFrame.p1L; state.p1R = inputFrame.p1R;
    state.p1Up2 = inputFrame.vbP1Up1; state.p1Down2 = inputFrame.vbP1Down1; state.p1Left2 = inputFrame.vbP1Left1; state.p1Right2 = inputFrame.vbP1Right1;

    state.p2A = inputFrame.p2A; state.p2B = inputFrame.p2B; state.p2Select = inputFrame.p2Select; state.p2Start = inputFrame.p2Start;
    state.p2Up = inputFrame.p2Up; state.p2Down = inputFrame.p2Down; state.p2Left = inputFrame.p2Left; state.p2Right = inputFrame.p2Right;
    state.p2X = inputFrame.p2X; state.p2Y = inputFrame.p2Y; state.p2L = inputFrame.p2L; state.p2R = inputFrame.p2R;
    state.p2Up2 = inputFrame.vbP2Up1; state.p2Down2 = inputFrame.vbP2Down1; state.p2Left2 = inputFrame.vbP2Left1; state.p2Right2 = inputFrame.vbP2Right1;

    state.p3A = inputFrame.p3A; state.p3B = inputFrame.p3B; state.p3Select = inputFrame.p3Select; state.p3Start = inputFrame.p3Start;
    state.p3Up = inputFrame.p3Up; state.p3Down = inputFrame.p3Down; state.p3Left = inputFrame.p3Left; state.p3Right = inputFrame.p3Right;
    state.p4A = inputFrame.p4A; state.p4B = inputFrame.p4B; state.p4Select = inputFrame.p4Select; state.p4Start = inputFrame.p4Start;
    state.p4Up = inputFrame.p4Up; state.p4Down = inputFrame.p4Down; state.p4Left = inputFrame.p4Left; state.p4Right = inputFrame.p4Right;

    state.p1PowerPadButtons = inputFrame.powerPadP1Buttons;
    state.p2PowerPadButtons = inputFrame.powerPadP2Buttons;
    state.suborKeyboardKeys = inputFrame.suborKeyboardKeys;
    state.familyBasicKeyboardKeys = inputFrame.familyBasicKeyboardKeys;

    if(inputFrame.zapperP1X != -1 || inputFrame.zapperP1Y != -1 || inputFrame.zapperP1Trigger) {
        state.zapperX = inputFrame.zapperP1X;
        state.zapperY = inputFrame.zapperP1Y;
    } else if(inputFrame.zapperP2X != -1 || inputFrame.zapperP2Y != -1 || inputFrame.zapperP2Trigger) {
        state.zapperX = inputFrame.zapperP2X;
        state.zapperY = inputFrame.zapperP2Y;
    } else if(inputFrame.bandaiX != -1 || inputFrame.bandaiY != -1 || inputFrame.bandaiTrigger) {
        state.zapperX = inputFrame.bandaiX;
        state.zapperY = inputFrame.bandaiY;
    } else {
        state.zapperX = -1;
        state.zapperY = -1;
    }

    if(inputFrame.snesMouseP1DeltaX != 0 || inputFrame.snesMouseP1DeltaY != 0 ||
       inputFrame.snesMouseP1Left || inputFrame.snesMouseP1Right) {
        state.mouseDeltaX = inputFrame.snesMouseP1DeltaX;
        state.mouseDeltaY = inputFrame.snesMouseP1DeltaY;
        state.mousePrimaryButton = inputFrame.snesMouseP1Left;
        state.mouseSecondaryButton = inputFrame.snesMouseP1Right;
    } else if(inputFrame.snesMouseP2DeltaX != 0 || inputFrame.snesMouseP2DeltaY != 0 ||
              inputFrame.snesMouseP2Left || inputFrame.snesMouseP2Right) {
        state.mouseDeltaX = inputFrame.snesMouseP2DeltaX;
        state.mouseDeltaY = inputFrame.snesMouseP2DeltaY;
        state.mousePrimaryButton = inputFrame.snesMouseP2Left;
        state.mouseSecondaryButton = inputFrame.snesMouseP2Right;
    } else if(inputFrame.suborMouseP1DeltaX != 0 || inputFrame.suborMouseP1DeltaY != 0 ||
              inputFrame.suborMouseP1Left || inputFrame.suborMouseP1Right) {
        state.mouseDeltaX = inputFrame.suborMouseP1DeltaX;
        state.mouseDeltaY = inputFrame.suborMouseP1DeltaY;
        state.mousePrimaryButton = inputFrame.suborMouseP1Left;
        state.mouseSecondaryButton = inputFrame.suborMouseP1Right;
    } else if(inputFrame.suborMouseP2DeltaX != 0 || inputFrame.suborMouseP2DeltaY != 0 ||
              inputFrame.suborMouseP2Left || inputFrame.suborMouseP2Right) {
        state.mouseDeltaX = inputFrame.suborMouseP2DeltaX;
        state.mouseDeltaY = inputFrame.suborMouseP2DeltaY;
        state.mousePrimaryButton = inputFrame.suborMouseP2Left;
        state.mouseSecondaryButton = inputFrame.suborMouseP2Right;
    } else {
        state.mouseDeltaX = 0;
        state.mouseDeltaY = 0;
        state.mousePrimaryButton = false;
        state.mouseSecondaryButton = false;
    }

    state.arkanoidNesPosition = inputFrame.arkanoidP1Button || inputFrame.arkanoidP1Position != 0.5f
        ? inputFrame.arkanoidP1Position
        : inputFrame.arkanoidP2Position;
    state.arkanoidFamicomPosition = inputFrame.arkanoidFamicomPosition;
    state.zapperP1Trigger = inputFrame.zapperP1Trigger;
    state.zapperP2Trigger = inputFrame.zapperP2Trigger;
    state.bandaiTrigger = inputFrame.bandaiTrigger;
    state.konamiP1Run = inputFrame.konamiP1Run;
    state.konamiP1Jump = inputFrame.konamiP1Jump;
    state.konamiP2Run = inputFrame.konamiP2Run;
    state.konamiP2Jump = inputFrame.konamiP2Jump;

    if(inputFrame.arkanoidP1Button || inputFrame.arkanoidP2Button || inputFrame.arkanoidFamicomButton) {
        state.mousePrimaryButton = state.mousePrimaryButton ||
                                   inputFrame.arkanoidP1Button ||
                                   inputFrame.arkanoidP2Button ||
                                   inputFrame.arkanoidFamicomButton;
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

void ConfirmedInputBufferDriver::produceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                                            bool active,
                                                            bool awaitingSync,
                                                            SessionState state,
                                                            std::optional<PlayerSlot> localSlot,
                                                            uint32_t dtMs,
                                                            const EmulationHost::InputState& localInputState,
                                                            const RoomState& room,
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
        localInputState,
        room,
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
                                                            const EmulationHost::InputState& localInputState,
                                                            const RoomState& room,
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
        // Input production horizon must follow configured delay only.
        // Prediction is consumed by playback/simulation, not by committed local input generation.
        const uint32_t targetBufferedThroughFrame = exactFrame + m_prebufferFrames;
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

    // Rollback/resimulation can temporarily rewind the emulator behind the
    // already-confirmed frontier. In that state, replay should consume the
    // existing confirmed local timeline instead of generating new committed
    // local inputs for those historical frames again.
    if(m_producedThroughFrame < confirmedThroughFrame) {
        m_producedThroughFrame = confirmedThroughFrame;
    }
    if(m_queuedThroughFrame < confirmedThroughFrame) {
        m_queuedThroughFrame = confirmedThroughFrame;
    }

    seedInitialPrebufferIfNeeded(coordinator, localSlots, localInputState, room);

    // Produce committed local inputs using delay-only horizon.
    const uint32_t targetBufferedThroughFrame = exactFrame + m_prebufferFrames;

    while(m_producedThroughFrame < targetBufferedThroughFrame) {
        ++m_producedThroughFrame;
        for(PlayerSlot localSlot : localSlots) {
            coordinator.recordLocalInputFrame(
                m_producedThroughFrame,
                localSlot,
                buildAssignedContribution(localSlot, localInputState, makeRoomTopologyBaseFrame(m_producedThroughFrame, room))
            );
        }
    }
    m_inputProductionAccumulatorMs = 0.0;
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
    EmulationHost::InputState inputState{};
    if(!localSlots.empty()) {
        applyPadMaskToInputState(inputState, localSlots.front(), localPrimaryMask);
    }

    RoomState room = coordinator.session().roomState();
    if(room.port1Device == std::nullopt) room.port1Device = Settings::Device::CONTROLLER;
    if(room.port2Device == std::nullopt) room.port2Device = Settings::Device::CONTROLLER;
    produceLocalBufferedInputs(
        coordinator,
        active,
        awaitingSync,
        state,
        localSlots,
        dtMs,
        inputState,
        room,
        regionFps,
        exactFrame,
        confirmedThroughFrame
    );
}

uint32_t ConfirmedInputBufferDriver::confirmedThroughFrame(const NetplayCoordinator& coordinator) const
{
    return coordinator.isActive() ? coordinator.latestConfirmedFrame() : 0u;
}

EmulationHost::InputState ConfirmedInputBufferDriver::buildReplayInputState(const NetplayCoordinator& coordinator, FrameNumber frame) const
{
    EmulationHost::InputState state{};
    const NetplayCoordinator::ConfirmedFrameInputs* confirmed = coordinator.findConfirmedFrame(frame);
    if(confirmed == nullptr) {
        return state;
    }
    applyInputFrameToInputState(state, confirmed->inputFrame);
    return state;
}

bool ConfirmedInputBufferDriver::tryBuildConfirmedInputState(const NetplayCoordinator& coordinator,
                                                             bool active,
                                                             bool awaitingSync,
                                                             SessionState state,
                                                             FrameNumber frame,
                                                             EmulationHost::InputState& outState) const
{
    if(!active || awaitingSync || state != SessionState::Running) return false;
    if(frame > confirmedThroughFrame(coordinator)) return false;
    outState = buildReplayInputState(coordinator, frame);
    return true;
}

void ConfirmedInputBufferDriver::preparePlaybackFramesForEmulationThread(NetplayCoordinator& coordinator,
                                                                         bool active,
                                                                         bool awaitingSync,
                                                                         SessionState state,
                                                                         uint32_t emulationFrame)
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
    const uint32_t hostFallbackThroughFrame =
        coordinator.isHosting() && !m_lastProduceHadLocalSlots
            ? queueHorizonFrame
            : m_producedThroughFrame;
    const uint32_t playableThroughFrame =
        coordinator.isHosting()
            ? hostFallbackThroughFrame
            : std::min(m_producedThroughFrame, predictedThroughFrame);
    const uint32_t firstFrame = emulationFrame + 1u;
    const uint32_t targetThroughFrame = std::min(playableThroughFrame, queueHorizonFrame);
    const auto allowPredictionForFrame = [delaySlackFrame, predictedThroughFrame](uint32_t frame) {
        return frame > delaySlackFrame && frame <= predictedThroughFrame;
    };
    const auto allowHostFallbackForFrame = [predictedThroughFrame](uint32_t frame) {
        return frame > predictedThroughFrame;
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

    // Refresh existing frames in-place so confirmed updates can replace older
    // predicted payloads without rebuilding the entire deque.
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

void ConfirmedInputBufferDriver::queuePendingFramesToEmu(GeraNESEmu& emu)
{
    std::scoped_lock pendingLock(m_pendingFramesMutex);
    const uint32_t currentFrame = emu.frameCount();
    while(!m_pendingFrames.empty() && m_pendingFrames.front().frame < currentFrame) {
        m_pendingFrames.pop_front();
    }

    const uint32_t queueLimitFrame = currentFrame + m_prebufferFrames + m_predictFrames;
    for(const NetplayCoordinator::ConfirmedFrameInputs& confirmed : m_pendingFrames) {
        if(confirmed.frame > queueLimitFrame) {
            break;
        }
        InputFrame inputFrame = confirmed.inputFrame;
        inputFrame.speculative = confirmed.predicted;
        inputFrame.timelineEpoch = emu.inputTimelineEpoch();
        (void)emu.queueInputFrame(inputFrame);
    }
}

} // namespace Netplay
