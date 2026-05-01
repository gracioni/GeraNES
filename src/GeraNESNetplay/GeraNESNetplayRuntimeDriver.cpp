#include "GeraNESNetplay/GeraNESNetplayRuntimeDriver.h"

#include <algorithm>

#include "ConsoleNetplay/NetplayLog.h"
#include "ConsoleNetplay/NetplayRuntimeHostApply.h"
#include "GeraNESNetplay/GeraNESNetplayConsole.h"
#include "logger/logger.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

void configureRuntimeForGeraNES(NetplayAppRuntime& runtime, IEmulationHost& host)
{
    runtime.setRuntimeHostWakeCallback([&host]() {
        host.setSimulationSuspended(false);
    });
    setNetplayLogCallback([](const std::string& message, NetplayLogLevel level) {
        Logger::Type type = Logger::Type::INFO;
        switch(level) {
            case NetplayLogLevel::Debug: type = Logger::Type::DEBUG; break;
            case NetplayLogLevel::Warning: type = Logger::Type::WARNING; break;
            case NetplayLogLevel::Error: type = Logger::Type::ERROR; break;
            case NetplayLogLevel::User: type = Logger::Type::USER; break;
            case NetplayLogLevel::Info:
            default: type = Logger::Type::INFO; break;
        }
        Logger::instance().log(message, type);
    });
}

RuntimeLoopSettings buildRuntimeLoopSettings(IEmulationHost& host,
                                             bool autoGameplayTuning,
                                             bool showDebugLog,
                                             int gameplayReceiveDelayMs,
                                             int inputDelayFrames,
                                             int predictFrames)
{
    RuntimeLoopSettings settings;
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
#ifdef NDEBUG
    settings.inputDelaySettings.gameplayReceiveDelayMs = 0;
#endif
    return settings;
}

NetplayAppRuntime::UpdateResult runRuntimeOnEmulationThread(NetplayAppRuntime& runtime,
                                                            IEmulationHost& host,
                                                            GeraNESEmu& emu,
                                                            const IEmulationHost::InputState& latestInputState,
                                                            const RuntimeLoopSettings& settings)
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
