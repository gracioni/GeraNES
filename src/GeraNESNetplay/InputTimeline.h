#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include "GeraNES/InputBuffer.h"
#include "NetProtocol.h"

namespace Netplay {

struct TimelineInputEntry
{
    FrameNumber frame = 0;
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot playerSlot = kObserverPlayerSlot;
    uint64_t buttonMaskLo = 0;
    uint64_t buttonMaskHi = 0;
    InputFrame inputFrame = {};
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
    void configure(size_t capacity);
    void clear();

    size_t capacity() const;
    size_t size() const;

    const std::deque<TimelineInputEntry>& entries() const;

    const TimelineInputEntry* find(FrameNumber frame, ParticipantId participantId, PlayerSlot slot) const;
    TimelineInputEntry* findMutable(FrameNumber frame, ParticipantId participantId, PlayerSlot slot);

    void push(const TimelineInputEntry& entry);
    void eraseFramesAfter(FrameNumber frame);

    const TimelineInputEntry* latest() const;
    const TimelineInputEntry* latestFor(PlayerSlot slot) const;
    const TimelineInputEntry* latestFor(ParticipantId participantId, PlayerSlot slot) const;
    const TimelineInputEntry* latestConfirmedFor(ParticipantId participantId, PlayerSlot slot) const;
};

} // namespace Netplay
