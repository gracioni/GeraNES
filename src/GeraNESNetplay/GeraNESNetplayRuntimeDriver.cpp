#include "GeraNESNetplay/GeraNESNetplayRuntimeDriver.h"

#include <mutex>

#include "ConsoleNetplay/NetplayLog.h"
#include "ConsoleNetplay/NetplayRuntimeHostApply.h"
#include "GeraNESNetplay/GeraNESNetplayConsole.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

namespace {

std::once_flag g_netplayLogInstallOnce;

} // namespace

void attachRuntimeWakeToHost(NetplayAppRuntime& runtime, IEmulationHost& host)
{
    runtime.setRuntimeHostWakeCallback([&host]() {
        host.setSimulationSuspended(false);
    });
}

void installProcessGlobalFrontendNetplayLogCallbackOnce()
{
    std::call_once(g_netplayLogInstallOnce, []() {
        setNetplayLogCallback([](const std::string&, NetplayLogLevel) {});
    });
}

RuntimeExecutionSettings buildGeraNESRuntimeExecutionSettings(IEmulationHost& host,
                                                              bool autoGameplayTuning,
                                                              bool showDebugLog,
                                                              int gameplayReceiveDelayMs,
                                                              int inputDelayFrames)
{
    int policyGameplayReceiveDelayMs = std::max(0, gameplayReceiveDelayMs);
#ifdef NDEBUG
    policyGameplayReceiveDelayMs = 0;
#endif
    return buildRuntimeExecutionSettings(
        host,
        autoGameplayTuning,
        showDebugLog,
        policyGameplayReceiveDelayMs,
        inputDelayFrames
    );
}

NetplayAppRuntime::UpdateResult executeRuntimeFrame(NetplayAppRuntime& runtime,
                                                    IEmulationHost& host,
                                                    GeraNESEmu& emu,
                                                    const IEmulationHost::InputState& latestInputState,
                                                    const RuntimeExecutionSettings& settings)
{
    GeraNESNetplayConsole console(host, emu, latestInputState);

    return runtime.updateAndApply(
        console,
        host,
        host.consumeManualStateChanges(),
        settings.inputDelaySettings,
        settings.frameSettings,
        GeraNESNetplayConsole::buildReplayFrameInput
    );
}

} // namespace GeraNESNetplay
