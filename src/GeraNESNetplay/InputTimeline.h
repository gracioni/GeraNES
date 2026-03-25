#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include "NetProtocol.h"

namespace Netplay {

struct TimelineInputEntry
{
    FrameNumber frame = 0;
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot playerSlot = kObserverPlayerSlot;
    uint64_t buttonMaskLo = 0;
    uint64_t buttonMaskHi = 0;
    uint32_t sequence = 0;
    bool predicted = false;
    bool confirmed = false;
};

class InputTimeline
{
private:
    size_t m_capacity = 0;
    std::deque<TimelineInputEntry> m_entries;

public:
    void configure(size_t capacity)
    {
        m_capacity = capacity;
        while(m_entries.size() > m_capacity) {
            m_entries.pop_front();
        }
    }

    void clear()
    {
        m_entries.clear();
    }

    size_t capacity() const
    {
        return m_capacity;
    }

    size_t size() const
    {
        return m_entries.size();
    }

    const std::deque<TimelineInputEntry>& entries() const
    {
        return m_entries;
    }

    const TimelineInputEntry* find(FrameNumber frame, ParticipantId participantId, PlayerSlot slot) const
    {
        for(const TimelineInputEntry& entry : m_entries) {
            if(entry.frame == frame &&
               entry.participantId == participantId &&
               entry.playerSlot == slot) {
                return &entry;
            }
        }
        return nullptr;
    }

    TimelineInputEntry* findMutable(FrameNumber frame, ParticipantId participantId, PlayerSlot slot)
    {
        for(TimelineInputEntry& entry : m_entries) {
            if(entry.frame == frame &&
               entry.participantId == participantId &&
               entry.playerSlot == slot) {
                return &entry;
            }
        }
        return nullptr;
    }

    void push(const TimelineInputEntry& entry)
    {
        if(m_capacity == 0) return;

        for(TimelineInputEntry& existing : m_entries) {
            if(existing.frame == entry.frame &&
               existing.participantId == entry.participantId &&
               existing.playerSlot == entry.playerSlot) {
                existing = entry;
                return;
            }
        }

        if(m_entries.size() >= m_capacity) {
            m_entries.pop_front();
        }

        m_entries.push_back(entry);
    }

    const TimelineInputEntry* latest() const
    {
        return m_entries.empty() ? nullptr : &m_entries.back();
    }

    const TimelineInputEntry* latestFor(PlayerSlot slot) const
    {
        for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
            if(it->playerSlot == slot) return &(*it);
        }
        return nullptr;
    }

    const TimelineInputEntry* latestFor(ParticipantId participantId, PlayerSlot slot) const
    {
        for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
            if(it->participantId == participantId && it->playerSlot == slot) return &(*it);
        }
        return nullptr;
    }

    const TimelineInputEntry* latestConfirmedFor(ParticipantId participantId, PlayerSlot slot) const
    {
        for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
            if(it->participantId == participantId && it->playerSlot == slot && it->confirmed) return &(*it);
        }
        return nullptr;
    }
};

} // namespace Netplay
