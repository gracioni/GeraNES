#pragma once

#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/NetplayCoordinator.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

namespace Netplay
{

class ConfirmedInputBufferDriver
{
private:
    double m_inputProductionAccumulatorMs = 0.0;
    uint32_t m_producedThroughFrame = 0;
    uint32_t m_queuedThroughFrame = 0;
    uint32_t m_prebufferFrames = 10;
    mutable std::mutex m_pendingFramesMutex;
    std::deque<NetplayCoordinator::ConfirmedFrameInputs> m_pendingFrames;

    void seedInitialPrebufferIfNeeded(NetplayCoordinator& coordinator, PlayerSlot localSlot)
    {
        while(m_producedThroughFrame < m_prebufferFrames) {
            ++m_producedThroughFrame;
            coordinator.recordLocalInputFrame(m_producedThroughFrame, localSlot, 0);
        }
    }

    static void applyPadMaskToInputState(EmulationHost::InputState& state, PlayerSlot slot, uint64_t mask)
    {
        const bool a = (mask & (1ull << 0)) != 0;
        const bool b = (mask & (1ull << 1)) != 0;
        const bool select = (mask & (1ull << 2)) != 0;
        const bool start = (mask & (1ull << 3)) != 0;
        const bool up = (mask & (1ull << 4)) != 0;
        const bool down = (mask & (1ull << 5)) != 0;
        const bool left = (mask & (1ull << 6)) != 0;
        const bool right = (mask & (1ull << 7)) != 0;
        const bool x = (mask & (1ull << 8)) != 0;
        const bool y = (mask & (1ull << 9)) != 0;
        const bool l = (mask & (1ull << 10)) != 0;
        const bool r = (mask & (1ull << 11)) != 0;
        const bool up2 = (mask & (1ull << 12)) != 0;
        const bool down2 = (mask & (1ull << 13)) != 0;
        const bool left2 = (mask & (1ull << 14)) != 0;
        const bool right2 = (mask & (1ull << 15)) != 0;
        switch(slot) {
            case 0:
                state.p1A = a; state.p1B = b; state.p1Select = select; state.p1Start = start;
                state.p1Up = up; state.p1Down = down; state.p1Left = left; state.p1Right = right;
                state.p1X = x; state.p1Y = y; state.p1L = l; state.p1R = r;
                state.p1Up2 = up2; state.p1Down2 = down2; state.p1Left2 = left2; state.p1Right2 = right2;
                break;
            case 1:
                state.p2A = a; state.p2B = b; state.p2Select = select; state.p2Start = start;
                state.p2Up = up; state.p2Down = down; state.p2Left = left; state.p2Right = right;
                state.p2X = x; state.p2Y = y; state.p2L = l; state.p2R = r;
                state.p2Up2 = up2; state.p2Down2 = down2; state.p2Left2 = left2; state.p2Right2 = right2;
                break;
            case 2:
                state.p3A = a; state.p3B = b; state.p3Select = select; state.p3Start = start;
                state.p3Up = up; state.p3Down = down; state.p3Left = left; state.p3Right = right;
                break;
            case 3:
                state.p4A = a; state.p4B = b; state.p4Select = select; state.p4Start = start;
                state.p4Up = up; state.p4Down = down; state.p4Left = left; state.p4Right = right;
                break;
            default:
                break;
        }
    }

    static void applyPadMaskToInputFrame(InputFrame& inputFrame, PlayerSlot slot, uint64_t mask)
    {
        const bool a = (mask & (1ull << 0)) != 0;
        const bool b = (mask & (1ull << 1)) != 0;
        const bool select = (mask & (1ull << 2)) != 0;
        const bool start = (mask & (1ull << 3)) != 0;
        const bool up = (mask & (1ull << 4)) != 0;
        const bool down = (mask & (1ull << 5)) != 0;
        const bool left = (mask & (1ull << 6)) != 0;
        const bool right = (mask & (1ull << 7)) != 0;
        const bool x = (mask & (1ull << 8)) != 0;
        const bool y = (mask & (1ull << 9)) != 0;
        const bool l = (mask & (1ull << 10)) != 0;
        const bool r = (mask & (1ull << 11)) != 0;
        const bool up2 = (mask & (1ull << 12)) != 0;
        const bool down2 = (mask & (1ull << 13)) != 0;
        const bool left2 = (mask & (1ull << 14)) != 0;
        const bool right2 = (mask & (1ull << 15)) != 0;

        switch(slot) {
            case 0:
                inputFrame.p1A = a; inputFrame.p1B = b; inputFrame.p1Select = select; inputFrame.p1Start = start;
                inputFrame.p1Up = up; inputFrame.p1Down = down; inputFrame.p1Left = left; inputFrame.p1Right = right;
                inputFrame.p1X = x; inputFrame.p1Y = y; inputFrame.p1L = l; inputFrame.p1R = r;
                break;
            case 1:
                inputFrame.p2A = a; inputFrame.p2B = b; inputFrame.p2Select = select; inputFrame.p2Start = start;
                inputFrame.p2Up = up; inputFrame.p2Down = down; inputFrame.p2Left = left; inputFrame.p2Right = right;
                inputFrame.p2X = x; inputFrame.p2Y = y; inputFrame.p2L = l; inputFrame.p2R = r;
                break;
            case 2:
                inputFrame.p3A = a; inputFrame.p3B = b; inputFrame.p3Select = select; inputFrame.p3Start = start;
                inputFrame.p3Up = up; inputFrame.p3Down = down; inputFrame.p3Left = left; inputFrame.p3Right = right;
                break;
            case 3:
                inputFrame.p4A = a; inputFrame.p4B = b; inputFrame.p4Select = select; inputFrame.p4Start = start;
                inputFrame.p4Up = up; inputFrame.p4Down = down; inputFrame.p4Left = left; inputFrame.p4Right = right;
                break;
            default:
                break;
        }
    }

public:
    uint32_t producedThroughFrame() const
    {
        return m_producedThroughFrame;
    }

    uint32_t queuedThroughFrame() const
    {
        return m_queuedThroughFrame;
    }

    size_t pendingFrameCount() const
    {
        std::scoped_lock pendingLock(m_pendingFramesMutex);
        return m_pendingFrames.size();
    }

    uint32_t prebufferFrames() const
    {
        return m_prebufferFrames;
    }

    void reset()
    {
        m_inputProductionAccumulatorMs = 0.0;
        m_producedThroughFrame = 0;
        m_queuedThroughFrame = 0;
        std::scoped_lock pendingLock(m_pendingFramesMutex);
        m_pendingFrames.clear();
    }

    void reanchor()
    {
        reset();
    }

    static uint64_t buildPadMask(bool a, bool b, bool select, bool start,
                                 bool up, bool down, bool left, bool right,
                                 bool x = false, bool y = false, bool l = false, bool r = false,
                                 bool up2 = false, bool down2 = false, bool left2 = false, bool right2 = false)
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

    void produceLocalBufferedInputs(NetplayCoordinator& coordinator,
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
        if(!active || awaitingSync || state != SessionState::Running) {
            reset();
            return;
        }

        if(!localSlot.has_value()) {
            reset();
            return;
        }

        seedInitialPrebufferIfNeeded(coordinator, *localSlot);

        const double fps = static_cast<double>(std::max(1u, regionFps));
        const double frameDurationMs = 1000.0 / fps;
        m_inputProductionAccumulatorMs += static_cast<double>(dtMs);

        const uint32_t targetBufferedThroughFrame = exactFrame + m_prebufferFrames;
        while(m_inputProductionAccumulatorMs >= frameDurationMs &&
              m_producedThroughFrame < targetBufferedThroughFrame) {
            m_inputProductionAccumulatorMs -= frameDurationMs;
            ++m_producedThroughFrame;
            coordinator.recordLocalInputFrame(m_producedThroughFrame, *localSlot, localPrimaryMask);
        }

        while(m_producedThroughFrame < targetBufferedThroughFrame) {
            ++m_producedThroughFrame;
            coordinator.recordLocalInputFrame(m_producedThroughFrame, *localSlot, localPrimaryMask);
        }

        const double maxBufferedAccumulatorMs = frameDurationMs * static_cast<double>(m_prebufferFrames);
        if(m_inputProductionAccumulatorMs > maxBufferedAccumulatorMs) {
            m_inputProductionAccumulatorMs = maxBufferedAccumulatorMs;
        }
    }

    uint32_t confirmedThroughFrame(const NetplayCoordinator& coordinator) const
    {
        return coordinator.isActive() ? coordinator.latestConfirmedFrame() : 0u;
    }

    EmulationHost::InputState buildReplayInputState(const NetplayCoordinator& coordinator, FrameNumber frame) const
    {
        EmulationHost::InputState state{};
        const NetplayCoordinator::ConfirmedFrameInputs* confirmed = coordinator.findConfirmedFrame(frame);
        if(confirmed == nullptr) {
            return state;
        }

        for(PlayerSlot slot = 0; slot < 4; ++slot) {
            applyPadMaskToInputState(state, slot, confirmed->buttonMaskLo[slot]);
        }
        return state;
    }

    bool tryBuildConfirmedInputState(const NetplayCoordinator& coordinator,
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

    void prepareConfirmedFramesForEmulationThread(const NetplayCoordinator& coordinator,
                                                  bool active,
                                                  bool awaitingSync,
                                                  SessionState state,
                                                  uint32_t emulationFrame)
    {
        if(!active || awaitingSync || state != SessionState::Running) {
            reset();
            return;
        }

        const uint32_t throughFrame = confirmedThroughFrame(coordinator);
        const uint32_t queueHorizonFrame = emulationFrame + (m_prebufferFrames * 2u) + 1u;
        std::scoped_lock pendingLock(m_pendingFramesMutex);
        while(m_queuedThroughFrame < throughFrame && m_queuedThroughFrame < queueHorizonFrame) {
            const uint32_t nextFrame = m_queuedThroughFrame + 1u;
            const NetplayCoordinator::ConfirmedFrameInputs* confirmed = coordinator.findConfirmedFrame(nextFrame);
            if(confirmed == nullptr) {
                break;
            }
            m_pendingFrames.push_back(*confirmed);
            m_queuedThroughFrame = nextFrame;
        }
    }

    void queuePendingFramesToEmu(GeraNESEmu& emu)
    {
        std::scoped_lock pendingLock(m_pendingFramesMutex);
        while(!m_pendingFrames.empty()) {
            const uint32_t currentFrame = emu.frameCount();
            const NetplayCoordinator::ConfirmedFrameInputs& confirmed = m_pendingFrames.front();
            if(confirmed.frame < currentFrame) {
                m_pendingFrames.pop_front();
                continue;
            }
            if(confirmed.frame > currentFrame + m_prebufferFrames) {
                break;
            }
            if(emu.inputBuffer().findByFrame(confirmed.frame) != nullptr) {
                m_pendingFrames.pop_front();
                continue;
            }

            InputFrame inputFrame = emu.createInputFrame(confirmed.frame);
            for(PlayerSlot slot = 0; slot < 4; ++slot) {
                applyPadMaskToInputFrame(inputFrame, slot, confirmed.buttonMaskLo[slot]);
            }
            emu.queueInputFrame(inputFrame);
            m_pendingFrames.pop_front();
        }
    }
};

}
