#include "ConsoleNetplay/NetplayLog.h"

#include <deque>
#include <mutex>
#include <utility>

namespace ConsoleNetplay {

namespace {

constexpr size_t kMaxDebugLogLines = 256;

std::mutex g_logMutex;
NetplayLogCallback g_logCallback;
std::deque<std::string> g_debugMessages;
bool g_debugLogEnabled = false;

} // namespace

void setNetplayLogCallback(NetplayLogCallback callback)
{
    std::scoped_lock lock(g_logMutex);
    g_logCallback = std::move(callback);
}

void setNetplayDebugLogEnabled(bool enabled)
{
    std::scoped_lock lock(g_logMutex);
    g_debugLogEnabled = enabled;
}

bool netplayDebugLogEnabled()
{
    std::scoped_lock lock(g_logMutex);
    return g_debugLogEnabled;
}

void clearNetplayDebugLogMessages()
{
    std::scoped_lock lock(g_logMutex);
    g_debugMessages.clear();
}

std::vector<std::string> netplayDebugLogMessages()
{
    std::scoped_lock lock(g_logMutex);
    return {g_debugMessages.begin(), g_debugMessages.end()};
}

void logNetplayMessage(const std::string& message, NetplayLogLevel level)
{
    NetplayLogCallback callback;
    bool recordDebugMessage = false;
    {
        std::scoped_lock lock(g_logMutex);
        recordDebugMessage = g_debugLogEnabled;
        if(recordDebugMessage) {
            g_debugMessages.push_back(message);
            while(g_debugMessages.size() > kMaxDebugLogLines) {
                g_debugMessages.pop_front();
            }
        }
        callback = g_logCallback;
    }
    if(callback) {
        callback(message, level);
    }
}

} // namespace ConsoleNetplay
