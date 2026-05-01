#include "GeraNESNetplay/GeraNESNetplayRuntimeDriver.h"

#include "ConsoleNetplay/NetplayLog.h"
#include "ConsoleNetplay/NetplayRuntimeHostApply.h"
#include "GeraNESNetplay/GeraNESNetplayConsole.h"
#include "logger/logger.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

void attachRuntimeWakeToHost(NetplayAppRuntime& runtime, IEmulationHost& host)
{
    runtime.setRuntimeHostWakeCallback([&host]() {
        host.setSimulationSuspended(false);
    });
}

void installFrontendNetplayLogCallback()
{
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
