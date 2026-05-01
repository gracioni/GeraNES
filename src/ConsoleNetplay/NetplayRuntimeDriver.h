#pragma once

#include <algorithm>

#include "ConsoleNetplay/NetplayAppRuntime.h"

namespace ConsoleNetplay {

struct RuntimeExecutionSettings
{
    RuntimeInputDelaySettings inputDelaySettings;
    NetplayAppRuntime::RuntimeFrameSettings frameSettings;
};

template<typename RuntimeHost>
RuntimeExecutionSettings buildRuntimeExecutionSettings(RuntimeHost& host,
                                                       bool autoGameplayTuning,
                                                       bool showDebugLog,
                                                       int gameplayReceiveDelayMs,
                                                       int inputDelayFrames,
                                                       int predictFrames)
{
    RuntimeExecutionSettings settings;
    settings.frameSettings.autoGameplayTuning = autoGameplayTuning;
    settings.frameSettings.showDebugLog = showDebugLog;
    settings.frameSettings.diagnostics = host.getNetplayDiagnostics();
    settings.frameSettings.discardQueuedNetplayInputsAfter = [&host](FrameNumber frame) {
        host.discardQueuedNetplayInputsAfter(frame);
    };

    settings.inputDelaySettings.debugMode = showDebugLog;
    settings.inputDelaySettings.autoGameplayTuning = autoGameplayTuning;
    settings.inputDelaySettings.gameplayReceiveDelayMs =
        static_cast<uint32_t>(std::max(0, gameplayReceiveDelayMs));
    settings.inputDelaySettings.manualInputDelayFrames =
        static_cast<uint32_t>(std::max(0, inputDelayFrames));
    settings.inputDelaySettings.manualPredictFrames =
        static_cast<uint32_t>(std::max(0, predictFrames));
    settings.inputDelaySettings.regionFps = host.getRegionFPS();
    return settings;
}

} // namespace ConsoleNetplay
