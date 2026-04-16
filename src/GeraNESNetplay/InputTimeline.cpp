#include "GeraNESNetplay/InputTimeline.h"

namespace Netplay {

void InputTimeline::configure(size_t capacity)
{
    m_capacity = capacity;
    while(m_entries.size() > m_capacity) {
        m_entries.pop_front();
    }
}

void InputTimeline::clear()
{
    m_entries.clear();
}

size_t InputTimeline::capacity() const
{
    return m_capacity;
}

size_t InputTimeline::size() const
{
    return m_entries.size();
}

const std::deque<TimelineInputEntry>& InputTimeline::entries() const
{
    return m_entries;
}

const TimelineInputEntry* InputTimeline::find(FrameNumber frame, ParticipantId participantId, PlayerSlot slot) const
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

TimelineInputEntry* InputTimeline::findMutable(FrameNumber frame, ParticipantId participantId, PlayerSlot slot)
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

void InputTimeline::push(const TimelineInputEntry& entry)
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

void InputTimeline::eraseFramesAfter(FrameNumber frame)
{
    for(auto it = m_entries.begin(); it != m_entries.end();) {
        if(it->frame > frame) {
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
}

const TimelineInputEntry* InputTimeline::latest() const
{
    return m_entries.empty() ? nullptr : &m_entries.back();
}

const TimelineInputEntry* InputTimeline::latestFor(PlayerSlot slot) const
{
    for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        if(it->playerSlot == slot) return &(*it);
    }
    return nullptr;
}

const TimelineInputEntry* InputTimeline::latestFor(ParticipantId participantId, PlayerSlot slot) const
{
    for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        if(it->participantId == participantId && it->playerSlot == slot) return &(*it);
    }
    return nullptr;
}

const TimelineInputEntry* InputTimeline::latestConfirmedFor(ParticipantId participantId, PlayerSlot slot) const
{
    for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        if(it->participantId == participantId && it->playerSlot == slot && it->confirmed) return &(*it);
    }
    return nullptr;
}

} // namespace Netplay
