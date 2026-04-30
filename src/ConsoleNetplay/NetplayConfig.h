#pragma once

#include <cstdint>

#ifndef CONSOLE_NETPLAY_DESYNC_MONITOR_ENABLED
#define CONSOLE_NETPLAY_DESYNC_MONITOR_ENABLED 1
#endif

#ifndef CONSOLE_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES
#define CONSOLE_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES 30
#endif

namespace ConsoleNetplay {

inline constexpr const char* kDefaultNetplayRuntimeVersion = "standalone";

inline constexpr bool kDesyncMonitorEnabled =
    CONSOLE_NETPLAY_DESYNC_MONITOR_ENABLED != 0;

inline constexpr uint32_t kDesyncCrcIntervalFrames =
    CONSOLE_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES > 0
        ? static_cast<uint32_t>(CONSOLE_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES)
        : 30u;

} // namespace ConsoleNetplay
