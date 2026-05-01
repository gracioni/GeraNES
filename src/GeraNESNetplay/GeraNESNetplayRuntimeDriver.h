#pragma once

#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/IEmulationHost.h"

namespace GeraNESNetplay {

struct RuntimeLoopSettings
{
    ConsoleNetplay::RuntimeInputDelaySettings inputDelaySettings;
    ConsoleNetplay::NetplayAppRuntime::RuntimeFrameSettings frameSettings;
};

void configureRuntimeForGeraNES(ConsoleNetplay::NetplayAppRuntime& runtime, IEmulationHost& host);
RuntimeLoopSettings buildRuntimeLoopSettings(IEmulationHost& host,
                                             bool autoGameplayTuning,
                                             bool showDebugLog,
                                             int gameplayReceiveDelayMs,
                                             int inputDelayFrames,
                                             int predictFrames);
ConsoleNetplay::NetplayAppRuntime::UpdateResult runRuntimeOnEmulationThread(
    ConsoleNetplay::NetplayAppRuntime& runtime,
    IEmulationHost& host,
    GeraNESEmu& emu,
    const IEmulationHost::InputState& latestInputState,
    const RuntimeLoopSettings& settings);

} // namespace GeraNESNetplay
