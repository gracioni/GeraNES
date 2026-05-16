#include "GeraNESNetplay/GeraNESNetplayRuntimeDriver.h"

#include <mutex>

#include "ConsoleNetplay/NetplayLog.h"
#include "ConsoleNetplay/NetplayRuntimeHostApply.h"
#include "GeraNESNetplay/GeraNESNetplayAdapters.h"
#include "GeraNESNetplay/GeraNESNetplayConsole.h"
#include "logger/logger.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

namespace {

std::once_flag g_netplayLogInstallOnce;

} // namespace

void attachRuntimeWakeToHost(NetplayAppRuntime& runtime, IEmulationHost& host)
{
    runtime.setRepeatedInputFrameTransformer(repeatedNetplayInputFrameFrom);
    runtime.setRuntimeHostWakeCallback([&host]() {
        host.setSimulationSuspended(false);
    });
}

void installProcessGlobalFrontendNetplayLogCallbackOnce()
{
    std::call_once(g_netplayLogInstallOnce, []() {
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
    });
}

RuntimeExecutionSettings buildGeraNESRuntimeExecutionSettings(IEmulationHost& host,
                                                              bool autoGameplayTuning,
                                                              bool showDebugLog,
                                                              int gameplayReceiveDelayMs,
                                                              int inputDelayFrames,
                                                              int predictFrames)
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
        inputDelayFrames,
        predictFrames
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
        [&emu](const NetplayCoordinator::ConfirmedFrameInputs& confirmed,
               FrameNumber frame,
               IEmulationHost::ReplayFrameInput& outFrame) {
            return GeraNESNetplayConsole::buildReplayFrameInput(confirmed, frame, emu, outFrame);
        }
    );
}

} // namespace GeraNESNetplay
