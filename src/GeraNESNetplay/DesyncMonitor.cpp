#include "GeraNESNetplay/DesyncMonitor.h"

#include <algorithm>

namespace Netplay {

void DesyncMonitor::reset()
{
    m_localHistory.clear();
    m_remoteHistory.clear();
    m_lastMismatchFrame = 0;
    m_consecutiveMismatchCount = 0;
}

DesyncMonitor::Update DesyncMonitor::submitLocalCrc(FrameNumber frame, uint32_t crc32)
{
    storeHistoryEntry(m_localHistory, frame, crc32);
    return evaluateFrame(frame);
}

DesyncMonitor::Update DesyncMonitor::submitRemoteCrc(FrameNumber frame, uint32_t crc32)
{
    storeHistoryEntry(m_remoteHistory, frame, crc32);
    return evaluateFrame(frame);
}

void DesyncMonitor::invalidateLocalHistoryAfter(FrameNumber frame)
{
    while(!m_localHistory.empty() && m_localHistory.back().first > frame) {
        m_localHistory.pop_back();
    }
}

void DesyncMonitor::storeHistoryEntry(std::deque<std::pair<FrameNumber, uint32_t>>& history,
                                      FrameNumber frame,
                                      uint32_t crc32)
{
    for(auto it = history.rbegin(); it != history.rend(); ++it) {
        if(it->first == frame) {
            it->second = crc32;
            return;
        }
    }

    history.emplace_back(frame, crc32);
    while(history.size() > kHistoryCapacity) {
        history.pop_front();
    }
}

std::optional<uint32_t> DesyncMonitor::findHistoryEntry(const std::deque<std::pair<FrameNumber, uint32_t>>& history,
                                                        FrameNumber frame)
{
    for(auto it = history.rbegin(); it != history.rend(); ++it) {
        if(it->first == frame) {
            return it->second;
        }
    }
    return std::nullopt;
}

DesyncMonitor::Update DesyncMonitor::evaluateFrame(FrameNumber frame)
{
    Update update;
    update.frame = frame;

    const std::optional<uint32_t> localCrc = findHistoryEntry(m_localHistory, frame);
    const std::optional<uint32_t> remoteCrc = findHistoryEntry(m_remoteHistory, frame);
    if(!localCrc.has_value() || !remoteCrc.has_value()) {
        return update;
    }
    if(*localCrc == 0 || *remoteCrc == 0) {
        return update;
    }

    update.compared = true;
    if(*localCrc != *remoteCrc) {
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
    return update;
}

} // namespace Netplay
