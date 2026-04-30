#include "ConsoleNetplay/NetplayLog.h"

#include <mutex>
#include <utility>

namespace ConsoleNetplay {

namespace {

std::mutex g_logMutex;
NetplayLogCallback g_logCallback;

} // namespace

void setNetplayLogCallback(NetplayLogCallback callback)
{
    std::scoped_lock lock(g_logMutex);
    g_logCallback = std::move(callback);
}

void logNetplayMessage(const std::string& message, NetplayLogLevel level)
{
    NetplayLogCallback callback;
    {
        std::scoped_lock lock(g_logMutex);
        callback = g_logCallback;
    }
    if(callback) {
        callback(message, level);
    }
}

} // namespace ConsoleNetplay
