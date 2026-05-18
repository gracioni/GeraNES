#include "ConsoleNetplay/DesyncMonitor.h"

#include <algorithm>

namespace ConsoleNetplay {

void DesyncMonitor::reset()
{
    m_localHistory.clear();
    m_remoteHistory.clear();
    m_matchingHistory.clear();
    m_lastMismatchFrame = 0;
    m_consecutiveMismatchCount = 0;
}

DesyncMonitor::Update DesyncMonitor::submitLocalCrc(const HistoryEntry& entry)
{
    storeHistoryEntry(m_localHistory, entry);
    return evaluateFrame(entry.frame);
}

DesyncMonitor::Update DesyncMonitor::submitRemoteCrc(const HistoryEntry& entry)
{
    storeHistoryEntry(m_remoteHistory, entry);
    return evaluateFrame(entry.frame);
}

std::optional<DesyncMonitor::HistoryEntry> DesyncMonitor::findLocalHistoryEntry(FrameNumber frame) const
{
    return findHistoryEntry(m_localHistory, frame);
}

std::optional<DesyncMonitor::HistoryEntry> DesyncMonitor::latestLocalHistoryEntry() const
{
    if(m_localHistory.empty()) {
        return std::nullopt;
    }
    return m_localHistory.back();
}

std::optional<DesyncMonitor::HistoryEntry> DesyncMonitor::latestMatchingHistoryEntryBefore(FrameNumber frame) const
{
    for(auto it = m_matchingHistory.rbegin(); it != m_matchingHistory.rend(); ++it) {
        if(it->frame < frame) {
            return *it;
        }
    }
    return std::nullopt;
}

void DesyncMonitor::invalidateHistoryAfter(FrameNumber frame)
{
    while(!m_localHistory.empty() && m_localHistory.back().frame >= frame) {
        m_localHistory.pop_back();
    }
    while(!m_remoteHistory.empty() && m_remoteHistory.back().frame >= frame) {
        m_remoteHistory.pop_back();
    }
    while(!m_matchingHistory.empty() && m_matchingHistory.back().frame >= frame) {
        m_matchingHistory.pop_back();
    }
}

void DesyncMonitor::storeHistoryEntry(std::deque<HistoryEntry>& history, const HistoryEntry& entry)
{
    storeHistoryEntry(history, entry, kHistoryCapacity);
}

void DesyncMonitor::storeHistoryEntry(std::deque<HistoryEntry>& history,
                                      const HistoryEntry& entry,
                                      size_t capacity)
{
    for(auto it = history.rbegin(); it != history.rend(); ++it) {
        if(it->frame == entry.frame) {
            *it = entry;
            return;
        }
    }

    history.emplace_back(entry);
    while(history.size() > capacity) {
        history.pop_front();
    }
}

std::optional<DesyncMonitor::HistoryEntry> DesyncMonitor::findHistoryEntry(const std::deque<HistoryEntry>& history,
                                                                           FrameNumber frame)
{
    for(auto it = history.rbegin(); it != history.rend(); ++it) {
        if(it->frame == frame) {
            return *it;
        }
    }
    return std::nullopt;
}

DesyncMonitor::Update DesyncMonitor::evaluateFrame(FrameNumber frame)
{
    Update update;
    update.frame = frame;

    const std::optional<HistoryEntry> localEntry = findHistoryEntry(m_localHistory, frame);
    const std::optional<HistoryEntry> remoteEntry = findHistoryEntry(m_remoteHistory, frame);
    if(!localEntry.has_value() || !remoteEntry.has_value()) {
        return update;
    }
    update.localEntry = localEntry;
    update.remoteEntry = remoteEntry;
    update.localCrc32 = localEntry->crc32;
    update.remoteCrc32 = remoteEntry->crc32;
    if(localEntry->crc32 == 0 || remoteEntry->crc32 == 0) {
        return update;
    }

    update.compared = true;
    if(localEntry->crc32 != remoteEntry->crc32) {
        update.mismatchDetected = true;
        if(m_lastMismatchFrame != 0 && frame >= m_lastMismatchFrame) {
            m_consecutiveMismatchCount = static_cast<uint8_t>(
                std::min<unsigned>(255u, static_cast<unsigned>(m_consecutiveMismatchCount) + 1u)
            );
        } else {
            m_consecutiveMismatchCount = 1;
        }
        m_lastMismatchFrame = frame;
        update.consecutiveMismatchCount = m_consecutiveMismatchCount;
        return update;
    }

    update.mismatchResolved = (m_lastMismatchFrame != 0 || m_consecutiveMismatchCount != 0);
    m_lastMismatchFrame = 0;
    m_consecutiveMismatchCount = 0;
    storeHistoryEntry(m_matchingHistory, *localEntry, kMatchingHistoryCapacity);
    return update;
}

} // namespace ConsoleNetplay
