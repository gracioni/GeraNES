#include "GeraNESNetplay/InputTimeline.h"

#include <iterator>

namespace Netplay {

InputTimeline::EntryKey InputTimeline::makeKey(FrameNumber frame,
                                               ParticipantId participantId,
                                               PlayerSlot slot)
{
    EntryKey key;
    key.frame = frame;
    key.participantId = participantId;
    key.playerSlot = slot;
    return key;
}

InputTimeline::EntryKey InputTimeline::makeKey(const TimelineInputEntry& entry)
{
    return makeKey(entry.frame, entry.participantId, entry.playerSlot);
}

void InputTimeline::configure(size_t capacity)
{
    m_capacity = capacity;
    while(m_entries.size() > m_capacity) {
        m_index.erase(makeKey(m_entries.front()));
        m_entries.pop_front();
    }
}

void InputTimeline::clear()
{
    m_entries.clear();
    m_index.clear();
}

size_t InputTimeline::capacity() const
{
    return m_capacity;
}

size_t InputTimeline::size() const
{
    return m_entries.size();
}

const InputTimeline::EntryList& InputTimeline::entries() const
{
    return m_entries;
}

InputTimeline::LookupStats InputTimeline::lookupStats() const
{
    return m_lookupStats;
}

const TimelineInputEntry* InputTimeline::find(FrameNumber frame, ParticipantId participantId, PlayerSlot slot) const
{
    const auto indexIt = m_index.find(makeKey(frame, participantId, slot));
    if(indexIt != m_index.end()) {
        m_lookupStats.record(false, true, 1);
        return &(*indexIt->second);
    }
    m_lookupStats.record(false, false, 0);
    return nullptr;
}

TimelineInputEntry* InputTimeline::findMutable(FrameNumber frame, ParticipantId participantId, PlayerSlot slot)
{
    const auto indexIt = m_index.find(makeKey(frame, participantId, slot));
    if(indexIt != m_index.end()) {
        m_lookupStats.record(true, true, 1);
        return &(*indexIt->second);
    }
    m_lookupStats.record(true, false, 0);
    return nullptr;
}

void InputTimeline::push(const TimelineInputEntry& entry)
{
    if(m_capacity == 0) return;

    const EntryKey key = makeKey(entry);
    if(const auto existingIt = m_index.find(key); existingIt != m_index.end()) {
        *existingIt->second = entry;
        return;
    }

    if(m_entries.size() >= m_capacity) {
        m_index.erase(makeKey(m_entries.front()));
        m_entries.pop_front();
    }

    m_entries.push_back(entry);
    m_index[key] = std::prev(m_entries.end());
}

void InputTimeline::eraseFramesAfter(FrameNumber frame)
{
    for(auto it = m_entries.begin(); it != m_entries.end();) {
        if(it->frame > frame) {
            m_index.erase(makeKey(*it));
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
