#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "ConsoleNetplay/NetplayTypes.h"
namespace ConsoleNetplay {

template<typename TValue>
class NetplayPerSlotValue
{
public:
    struct Entry
    {
        PlayerSlot slot = kObserverPlayerSlot;
        TValue value = {};

        bool operator==(const Entry&) const = default;
    };

private:
    std::vector<Entry> m_entries;

    typename std::vector<Entry>::iterator lowerBound(PlayerSlot slot)
    {
        return std::lower_bound(m_entries.begin(), m_entries.end(), slot, [](const Entry& entry, PlayerSlot candidateSlot) {
            return entry.slot < candidateSlot;
        });
    }

    typename std::vector<Entry>::const_iterator lowerBound(PlayerSlot slot) const
    {
        return std::lower_bound(m_entries.begin(), m_entries.end(), slot, [](const Entry& entry, PlayerSlot candidateSlot) {
            return entry.slot < candidateSlot;
        });
    }

public:
    TValue& operator[](PlayerSlot slot)
    {
        const auto it = lowerBound(slot);
        if(it != m_entries.end() && it->slot == slot) {
            return it->value;
        }
        return m_entries.insert(it, Entry{slot, {}})->value;
    }

    const TValue& operator[](PlayerSlot slot) const
    {
        static const TValue kDefaultValue = {};

        const auto it = lowerBound(slot);
        if(it != m_entries.end() && it->slot == slot) {
            return it->value;
        }
        return kDefaultValue;
    }

    TValue* find(PlayerSlot slot)
    {
        const auto it = lowerBound(slot);
        return (it != m_entries.end() && it->slot == slot) ? &it->value : nullptr;
    }

    const TValue* find(PlayerSlot slot) const
    {
        const auto it = lowerBound(slot);
        return (it != m_entries.end() && it->slot == slot) ? &it->value : nullptr;
    }

    bool contains(PlayerSlot slot) const
    {
        return find(slot) != nullptr;
    }

    const std::vector<Entry>& entries() const
    {
        return m_entries;
    }

    std::vector<Entry>& entries()
    {
        return m_entries;
    }

    void erase(PlayerSlot slot)
    {
        const auto it = lowerBound(slot);
        if(it != m_entries.end() && it->slot == slot) {
            m_entries.erase(it);
        }
    }

    bool operator==(const NetplayPerSlotValue&) const = default;
    bool operator!=(const NetplayPerSlotValue&) const = default;
};

struct NetplayInputFrame
{
    FrameNumber frame = 0;
    uint32_t timelineEpoch = 0;
    bool speculative = false;

    NetplayPerSlotValue<uint64_t> buttonMaskLo = {};
    NetplayPerSlotValue<uint64_t> buttonMaskHi = {};
    NetplayPerSlotValue<std::vector<uint8_t>> slotPayloads = {};

    // Console/app-defined metadata. The reusable netplay core treats this as
    // opaque bytes; adapters decide whether they need it.
    std::vector<uint8_t> framePayload;

    std::vector<PlayerSlot> activeSlots() const
    {
        std::vector<PlayerSlot> slots;
        slots.reserve(buttonMaskLo.entries().size() + buttonMaskHi.entries().size() + slotPayloads.entries().size());

        const auto hasData = [this](PlayerSlot slot) {
            return buttonMaskLo[slot] != 0u ||
                   buttonMaskHi[slot] != 0u ||
                   !slotPayloads[slot].empty();
        };

        const auto appendUnique = [&slots](PlayerSlot slot) {
            if(std::find(slots.begin(), slots.end(), slot) == slots.end()) {
                slots.push_back(slot);
            }
        };

        for(const auto& entry : buttonMaskLo.entries()) appendUnique(entry.slot);
        for(const auto& entry : buttonMaskHi.entries()) appendUnique(entry.slot);
        for(const auto& entry : slotPayloads.entries()) appendUnique(entry.slot);

        slots.erase(std::remove_if(slots.begin(), slots.end(), [&hasData](PlayerSlot slot) {
            return !hasData(slot);
        }), slots.end());
        return slots;
    }

    bool operator==(const NetplayInputFrame&) const = default;
    bool operator!=(const NetplayInputFrame&) const = default;

    static NetplayInputFrame repeatedFrom(const NetplayInputFrame& previous, FrameNumber targetFrame)
    {
        NetplayInputFrame repeated = previous;
        repeated.frame = targetFrame;
        repeated.speculative = false;
        return repeated;
    }
};

} // namespace ConsoleNetplay
