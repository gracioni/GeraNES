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
    struct HistoryEntry
    {
        FrameNumber frame = 0;
        uint32_t crc32 = 0;
        CrcSubmissionSource submissionSource = CrcSubmissionSource::Unknown;
        FrameNumber localSimulationFrame = 0;
        FrameNumber confirmedFrame = 0;
    };

    struct Update
    {
        bool compared = false;
        bool mismatchDetected = false;
        bool mismatchResolved = false;
        FrameNumber frame = 0;
        uint8_t consecutiveMismatchCount = 0;
        std::optional<uint32_t> localCrc32;
        std::optional<uint32_t> remoteCrc32;
        std::optional<HistoryEntry> localEntry;
        std::optional<HistoryEntry> remoteEntry;
    };

    void reset();
    Update submitLocalCrc(const HistoryEntry& entry);
    Update submitRemoteCrc(const HistoryEntry& entry);
    Update submitLocalCrc(FrameNumber frame, uint32_t crc32)
    {
        return submitLocalCrc(HistoryEntry{frame, crc32});
    }
    Update submitRemoteCrc(FrameNumber frame, uint32_t crc32)
    {
        return submitRemoteCrc(HistoryEntry{frame, crc32});
    }
    std::optional<HistoryEntry> findLocalHistoryEntry(FrameNumber frame) const;
    void invalidateHistoryAfter(FrameNumber frame);

private:
    static constexpr size_t kHistoryCapacity = 512;

    std::deque<HistoryEntry> m_localHistory;
    std::deque<HistoryEntry> m_remoteHistory;
    FrameNumber m_lastMismatchFrame = 0;
    uint8_t m_consecutiveMismatchCount = 0;

    static void storeHistoryEntry(std::deque<HistoryEntry>& history, const HistoryEntry& entry);
    static std::optional<HistoryEntry> findHistoryEntry(const std::deque<HistoryEntry>& history, FrameNumber frame);
    Update evaluateFrame(FrameNumber frame);
};

} // namespace ConsoleNetplay
