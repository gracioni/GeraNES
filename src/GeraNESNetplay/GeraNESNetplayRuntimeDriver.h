#pragma once

#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "ConsoleNetplay/NetplayRuntimeDriver.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/IEmulationHost.h"

namespace GeraNESNetplay {

void attachRuntimeWakeToHost(ConsoleNetplay::NetplayAppRuntime& runtime, IEmulationHost& host);
void installFrontendNetplayLogCallback();
ConsoleNetplay::NetplayAppRuntime::UpdateResult executeRuntimeFrame(
    ConsoleNetplay::NetplayAppRuntime& runtime,
    IEmulationHost& host,
    GeraNESEmu& emu,
    const IEmulationHost::InputState& latestInputState,
    const ConsoleNetplay::RuntimeExecutionSettings& settings);

} // namespace GeraNESNetplay
