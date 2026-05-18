#pragma once

#include <optional>
#include <vector>

#include "ConsoleNetplay/NetplayRuntimeTypes.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
#include "ConsoleNetplay/NetplayInputFrame.h"
#include "ConsoleNetplay/NetplayTypes.h"

namespace ConsoleNetplay {

class INetplayConsole
{
public:
    virtual ~INetplayConsole() = default;

    virtual bool valid() const = 0;
    virtual uint32_t frameCount() const = 0;
    virtual uint32_t regionFps() const = 0;
    virtual uint32_t canonicalNetplayStateCrc32() = 0;
    virtual std::optional<NetplayRomSelection> currentRomSelection() const = 0;
    virtual bool loadRollbackState(const std::vector<uint8_t>& data) = 0;
    virtual bool updateUntilFrame(uint32_t frameDtMs, bool renderAudio) = 0;

    virtual void applyRemoteInputTopology(const RoomState& room) = 0;
    virtual void publishCurrentInputTopology(NetplayCoordinator& coordinator) = 0;
    virtual NetplayInputFrame buildLocalInputContribution(PlayerSlot slot,
                                                          FrameNumber frame,
                                                          const RoomState& room) const = 0;

    virtual bool hasStableQueuedInputFrame(FrameNumber frame) const = 0;
    virtual void queueStandaloneBootstrapInputFrame() = 0;
    virtual bool queuePlaybackInputFrame(const NetplayCoordinator::ConfirmedFrameInputs& confirmed) = 0;
    virtual void discardQueuedInputFramesAfter(FrameNumber frame) = 0;
};

} // namespace ConsoleNetplay
