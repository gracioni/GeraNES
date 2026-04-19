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
public:
    struct LookupStats
    {
        uint64_t findCalls = 0;
        uint64_t findMutableCalls = 0;
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t totalScannedEntries = 0;
        uint64_t maxScannedEntries = 0;
        uint64_t lastScannedEntries = 0;

        void record(bool mutableLookup, bool hit, size_t scannedEntries)
        {
            if(mutableLookup) {
                ++findMutableCalls;
            } else {
                ++findCalls;
            }
            if(hit) {
                ++hits;
            } else {
                ++misses;
            }
            const uint64_t scanned = static_cast<uint64_t>(scannedEntries);
            lastScannedEntries = scanned;
            totalScannedEntries += scanned;
            if(scanned > maxScannedEntries) {
                maxScannedEntries = scanned;
            }
        }
    };

private:
    size_t m_capacity = 0;
    std::deque<TimelineInputEntry> m_entries;
    mutable LookupStats m_lookupStats;

public:
    void configure(size_t capacity);
    void clear();

    size_t capacity() const;
    size_t size() const;

    const std::deque<TimelineInputEntry>& entries() const;
    LookupStats lookupStats() const;

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
