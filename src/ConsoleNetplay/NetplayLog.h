#pragma once

#include <functional>
#include <string>

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
void logNetplayMessage(const std::string& message, NetplayLogLevel level = NetplayLogLevel::Info);

} // namespace ConsoleNetplay
