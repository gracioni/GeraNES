#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>

#include "NetplayTypes.h"

namespace ConsoleNetplay {

class DesyncMonitor
{
public:
    struct Update
    {
        bool compared = false;
        bool mismatchDetected = false;
        bool mismatchResolved = false;
        FrameNumber frame = 0;
        uint8_t consecutiveMismatchCount = 0;
    };

    void reset();
    Update submitLocalCrc(FrameNumber frame, uint32_t crc32);
    Update submitRemoteCrc(FrameNumber frame, uint32_t crc32);
    void invalidateHistoryAfter(FrameNumber frame);

private:
    static constexpr size_t kHistoryCapacity = 512;

    std::deque<std::pair<FrameNumber, uint32_t>> m_localHistory;
    std::deque<std::pair<FrameNumber, uint32_t>> m_remoteHistory;
    FrameNumber m_lastMismatchFrame = 0;
    uint8_t m_consecutiveMismatchCount = 0;

    static void storeHistoryEntry(std::deque<std::pair<FrameNumber, uint32_t>>& history,
                                  FrameNumber frame,
                                  uint32_t crc32);
    static std::optional<uint32_t> findHistoryEntry(const std::deque<std::pair<FrameNumber, uint32_t>>& history,
                                                    FrameNumber frame);
    Update evaluateFrame(FrameNumber frame);
};

} // namespace ConsoleNetplay
