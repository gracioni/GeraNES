#pragma once

#include <cstdint>

#ifndef GERANES_NETPLAY_DESYNC_MONITOR_ENABLED
#define GERANES_NETPLAY_DESYNC_MONITOR_ENABLED 1
#endif

#ifndef GERANES_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES
#define GERANES_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES 30
#endif

namespace Netplay {

inline constexpr bool kDesyncMonitorEnabled =
    GERANES_NETPLAY_DESYNC_MONITOR_ENABLED != 0;

inline constexpr uint32_t kDesyncCrcIntervalFrames =
    GERANES_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES > 0
        ? static_cast<uint32_t>(GERANES_NETPLAY_DESYNC_CRC_INTERVAL_FRAMES)
        : 30u;

} // namespace Netplay
