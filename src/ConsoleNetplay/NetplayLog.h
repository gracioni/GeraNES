#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace ConsoleNetplay {

enum class NetplayLogLevel
{
    Debug,
    Info,
    Warning,
    Error,
    User
};

using NetplayLogCallback = std::function<void(const std::string&, NetplayLogLevel)>;

void setNetplayLogCallback(NetplayLogCallback callback);
void setNetplayDebugLogEnabled(bool enabled);
bool netplayDebugLogEnabled();
void clearNetplayDebugLogMessages();
std::vector<std::string> netplayDebugLogMessages();
void logNetplayMessage(const std::string& message, NetplayLogLevel level = NetplayLogLevel::Info);

} // namespace ConsoleNetplay
