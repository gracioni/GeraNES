#pragma once

#include <utility>

#include "ConsoleNetplay/NetplayAppRuntime.h"

namespace ConsoleNetplay {

template<typename RuntimeHost>
NetplayAppRuntime::UpdateResult NetplayAppRuntime::update(INetplayConsole& console,
                                                          RuntimeHost& host,
                                                          std::vector<NetplayManualStateChangeRecord> manualEvents,
                                                          RuntimeInputDelaySettings inputDelaySettings,
                                                          RuntimeFrameSettings frameSettings)
{
    NetplayStateBridgeAdapter<RuntimeHost> stateBridge(host);
    NetplayStateHostBridgeAdapter<RuntimeHost> hostBridge(host);
    NetplayRuntimeSessionControlsAdapter<RuntimeHost> sessionControls(host);
    UpdateContext context{
        console,
        stateBridge,
        hostBridge,
        sessionControls,
        std::move(manualEvents),
        std::move(inputDelaySettings),
        std::move(frameSettings),
        {}
    };
    return update(std::move(context));
}

template<typename RuntimeHost>
void NetplayAppRuntime::applyUpdateResultToHost(RuntimeHost& host,
                                                const UpdateResult& result,
                                                bool applyInputOwnership)
{
    host.configureInputBufferCapacity(result.inputBufferCapacity);
    if(result.snapshotCapacity.has_value()) {
        host.configureNetplaySnapshots(*result.snapshotCapacity);
    }
    if(applyInputOwnership) {
        host.setAutoQueuePendingInputOnFrameStart(result.autoQueuePendingInputOnFrameStart);
        host.setAllowPresenterTimeoutAdvance(result.allowPresenterTimeoutAdvance);
    }
    if(result.discardQueuedAudio) {
        host.discardQueuedAudio();
    }
    host.setSimulationSuspended(result.simulationSuspended);
}

template<typename RuntimeHost, typename Converter>
void NetplayAppRuntime::applyPlaybackResolverToHost(RuntimeHost& host,
                                                    bool enabled,
                                                    Converter converter)
{
    if(enabled) {
        host.setFrameInputResolver(
            [this, converter](FrameNumber frame, typename RuntimeHost::ReplayFrameInput& outFrame) {
                NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
                if(!tryBuildPlaybackFrame(frame, playbackFrame)) {
                    return false;
                }
                return converter(playbackFrame, frame, outFrame);
            }
        );
    } else {
        host.setFrameInputResolver({});
    }
}

template<typename RuntimeHost, typename Converter>
NetplayAppRuntime::UpdateResult NetplayAppRuntime::updateAndApply(
    INetplayConsole& console,
    RuntimeHost& host,
    std::vector<NetplayManualStateChangeRecord> manualEvents,
    RuntimeInputDelaySettings inputDelaySettings,
    RuntimeFrameSettings frameSettings,
    Converter playbackConverter)
{
    NetplayStateBridgeAdapter<RuntimeHost> stateBridge(host);
    NetplayStateHostBridgeAdapter<RuntimeHost> hostBridge(host);
    NetplayRuntimeSessionControlsAdapter<RuntimeHost> sessionControls(host);
    UpdateContext context{
        console,
        stateBridge,
        hostBridge,
        sessionControls,
        std::move(manualEvents),
        std::move(inputDelaySettings),
        std::move(frameSettings),
        [this, &host, playbackConverter](bool netplayOwnsEmulationInput, bool allowPresenterTimeoutAdvance) {
            host.setAutoQueuePendingInputOnFrameStart(!netplayOwnsEmulationInput);
            applyPlaybackResolverToHost(host, netplayOwnsEmulationInput, playbackConverter);
            host.setAllowPresenterTimeoutAdvance(allowPresenterTimeoutAdvance);
        }
    };
    const UpdateResult result = update(std::move(context));
    applyUpdateResultToHost(host, result, false);
    return result;
}

} // namespace ConsoleNetplay
