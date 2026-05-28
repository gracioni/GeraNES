#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "InputState.h"
#include "Serialization.h"
#include "util/CircularBuffer.h"

struct InputFrame
{
    using PadButtons = InputState::PadButtons;
    using PointerState = InputState::PointerState;
    using RelativePointerState = InputState::RelativePointerState;
    using ArkanoidState = InputState::ArkanoidState;
    using KonamiHyperShotState = InputState::KonamiHyperShotState;

    uint32_t frame = 0;
    uint32_t timelineEpoch = 0;
    InputState state;

    Settings::Device& port1Device = state.port1Device;
    Settings::Device& port2Device = state.port2Device;
    Settings::ExpansionDevice& expansionDevice = state.expansionDevice;
    Settings::NesMultitapDevice& nesMultitapDevice = state.nesMultitapDevice;
    Settings::FamicomMultitapDevice& famicomMultitapDevice = state.famicomMultitapDevice;
    std::vector<uint8_t>& serializedInputData = state.serializedInputData;

    InputFrame() = default;
    InputFrame(const InputFrame& other)
        : frame(other.frame), timelineEpoch(other.timelineEpoch), state(other.state)
    {
    }
    InputFrame(InputFrame&& other) noexcept
        : frame(other.frame), timelineEpoch(other.timelineEpoch), state(std::move(other.state))
    {
    }
    InputFrame& operator=(const InputFrame& other)
    {
        if(this != &other) {
            frame = other.frame;
            timelineEpoch = other.timelineEpoch;
            state = other.state;
        }
        return *this;
    }
    InputFrame& operator=(InputFrame&& other) noexcept
    {
        if(this != &other) {
            frame = other.frame;
            timelineEpoch = other.timelineEpoch;
            state = std::move(other.state);
        }
        return *this;
    }

    bool operator==(const InputFrame& other) const
    {
        return frame == other.frame &&
               timelineEpoch == other.timelineEpoch &&
               state == other.state;
    }
    bool operator!=(const InputFrame& other) const
    {
        return !(*this == other);
    }

    bool multitapActive() const
    {
        return state.multitapActive();
    }

    PadButtons portButtons(int port) const { return state.portButtons(port); }
    void setPortButtons(int port, const PadButtons& buttons) { state.setPortButtons(port, buttons); }
    PadButtons bandaiButtons() const { return state.bandaiButtons(); }
    void setBandaiButtons(const PadButtons& buttons) { state.setBandaiButtons(buttons); }
    PointerState zapper(int port) const { return state.zapper(port); }
    void setZapper(int port, const PointerState& pointerState) { state.setZapper(port, pointerState); }
    PointerState bandaiPointer() const { return state.bandaiPointer(); }
    void setBandaiPointer(const PointerState& pointerState) { state.setBandaiPointer(pointerState); }
    ArkanoidState arkanoidController(int port) const { return state.arkanoidController(port); }
    void setArkanoidController(int port, const ArkanoidState& arkanoidState) { state.setArkanoidController(port, arkanoidState); }
    ArkanoidState arkanoidExpansion() const { return state.arkanoidExpansion(); }
    void setArkanoidExpansion(const ArkanoidState& arkanoidState) { state.setArkanoidExpansion(arkanoidState); }
    RelativePointerState snesMouse(int port) const { return state.snesMouse(port); }
    void setSnesMouse(int port, const RelativePointerState& pointerState) { state.setSnesMouse(port, pointerState); }
    RelativePointerState suborMouse(int port) const { return state.suborMouse(port); }
    void setSuborMouse(int port, const RelativePointerState& pointerState) { state.setSuborMouse(port, pointerState); }
    KonamiHyperShotState konamiHyperShot() const { return state.konamiHyperShot(); }
    void setKonamiHyperShot(const KonamiHyperShotState& konamiState) { state.setKonamiHyperShot(konamiState); }
    std::array<bool, 12> powerPadButtons(int port) const { return state.powerPadButtons(port); }
    void setPowerPadButtons(int port, const std::array<bool, 12>& buttons) { state.setPowerPadButtons(port, buttons); }
    IExpansionDevice::SuborKeyboardKeys suborKeyboardKeys() const { return state.suborKeyboardKeys(); }
    void setSuborKeyboardKeys(const IExpansionDevice::SuborKeyboardKeys& keys) { state.setSuborKeyboardKeys(keys); }
    IExpansionDevice::FamilyBasicKeyboardKeys familyBasicKeyboardKeys() const { return state.familyBasicKeyboardKeys(); }
    void setFamilyBasicKeyboardKeys(const IExpansionDevice::FamilyBasicKeyboardKeys& keys) { state.setFamilyBasicKeyboardKeys(keys); }

    static InputFrame repeatedFrom(const InputFrame& previous, uint32_t targetFrame)
    {
        InputFrame repeated = previous;
        repeated.frame = targetFrame;
        RelativePointerState snes1 = repeated.snesMouse(1);
        RelativePointerState snes2 = repeated.snesMouse(2);
        RelativePointerState subor1 = repeated.suborMouse(1);
        RelativePointerState subor2 = repeated.suborMouse(2);
        snes1.deltaX = 0;
        snes1.deltaY = 0;
        snes2.deltaX = 0;
        snes2.deltaY = 0;
        subor1.deltaX = 0;
        subor1.deltaY = 0;
        subor2.deltaX = 0;
        subor2.deltaY = 0;
        repeated.setSnesMouse(1, snes1);
        repeated.setSnesMouse(2, snes2);
        repeated.setSuborMouse(1, subor1);
        repeated.setSuborMouse(2, subor2);
        return repeated;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, frame);
        SERIALIZEDATA(s, timelineEpoch);
        state.serialization(s);
    }

    nlohmann::json toJson() const
    {
        nlohmann::json j = state.toJson();
        j["frame"] = frame;
        j["timelineEpoch"] = timelineEpoch;
        return j;
    }

    std::string toJsonString(int indent = 2) const
    {
        return toJson().dump(indent);
    }

};

class InputBuffer
{
    // Contract:
    // - One authoritative input value exists per (frame, timelineEpoch).
    // - A pending frame can be inserted or updated.
    // - Once a frame is consumed, further writes to that frame are rejected.
    // - For each timeline epoch, new frames must be enqueued sequentially.
private:
    struct Entry
    {
        InputFrame frame;
        bool consumed = false;
    };

public:
    enum class EnqueueResult : uint8_t
    {
        Inserted,
        UpdatedPending,
        RejectedConsumed,
        RejectedEpoch,
        RejectedOutOfSequence
    };

    struct EnqueueCounters
    {
        uint64_t inserted = 0;
        uint64_t updatedPending = 0;
        uint64_t rejectedConsumed = 0;
        uint64_t rejectedEpoch = 0;
        uint64_t rejectedOutOfSequence = 0;
    };

private:
    size_t m_capacity = 1000;
    CircularBuffer<Entry> m_frames;
    EnqueueCounters m_enqueueCounters;
    mutable bool m_indexDirty = true;
    mutable std::unordered_map<uint64_t, size_t> m_indexByFrameEpoch;
    mutable std::unordered_map<uint32_t, size_t> m_latestIndexByFrame;
    mutable std::unordered_map<uint32_t, uint32_t> m_latestFrameByEpoch;

    static uint64_t makeFrameEpochKey(uint32_t frame, uint32_t timelineEpoch)
    {
        return (static_cast<uint64_t>(timelineEpoch) << 32) | static_cast<uint64_t>(frame);
    }

    void markIndexDirty()
    {
        m_indexDirty = true;
    }

    void rebuildIndexIfNeeded() const
    {
        if(!m_indexDirty) {
            return;
        }

        m_indexByFrameEpoch.clear();
        m_latestIndexByFrame.clear();
        m_latestFrameByEpoch.clear();

        const size_t frameCount = m_frames.size();
        m_indexByFrameEpoch.reserve(frameCount);
        for(size_t i = 0; i < frameCount; ++i) {
            const Entry& entry = m_frames.peakAt(i);
            m_indexByFrameEpoch[makeFrameEpochKey(entry.frame.frame, entry.frame.timelineEpoch)] = i;
            m_latestIndexByFrame[entry.frame.frame] = i;

            const auto latestIt = m_latestFrameByEpoch.find(entry.frame.timelineEpoch);
            if(latestIt == m_latestFrameByEpoch.end() || latestIt->second < entry.frame.frame) {
                m_latestFrameByEpoch[entry.frame.timelineEpoch] = entry.frame.frame;
            }
        }
        m_indexDirty = false;
    }

public:
    explicit InputBuffer(size_t capacity = 1000)
        : m_capacity(std::max<size_t>(1, capacity))
        , m_frames(m_capacity, CircularBuffer<Entry>::REPLACE)
    {
    }

    size_t capacity() const { return m_capacity; }

    size_t size() const
    {
        return m_frames.size();
    }

    bool empty() const
    {
        return m_frames.empty();
    }

    void clear()
    {
        m_frames.clear();
        m_enqueueCounters = {};
        m_indexByFrameEpoch.clear();
        m_latestIndexByFrame.clear();
        m_latestFrameByEpoch.clear();
        markIndexDirty();
    }

    void reconfigureCapacity(size_t capacity)
    {
        const size_t targetCapacity = std::max<size_t>(1, capacity);
        if(targetCapacity == m_capacity) {
            return;
        }

        CircularBuffer<Entry> rebuilt(targetCapacity, CircularBuffer<Entry>::REPLACE);
        const size_t frameCount = m_frames.size();
        for(size_t i = 0; i < frameCount; ++i) {
            rebuilt.write(m_frames.peakAt(i));
        }
        m_capacity = targetCapacity;
        m_frames = std::move(rebuilt);
        markIndexDirty();
    }

    EnqueueCounters enqueueCounters() const
    {
        return m_enqueueCounters;
    }

    void eraseFramesAfter(uint32_t frame)
    {
        CircularBuffer<Entry> rebuilt(m_capacity, CircularBuffer<Entry>::REPLACE);
        const size_t frameCount = m_frames.size();
        for(size_t i = 0; i < frameCount; ++i) {
            const Entry& current = m_frames.peakAt(i);
            if(current.frame.frame <= frame) {
                rebuilt.write(current);
            }
        }
        m_frames = rebuilt;
        markIndexDirty();
    }

    void eraseFramesNotMatchingTimelineEpoch(uint32_t timelineEpoch)
    {
        CircularBuffer<Entry> rebuilt(m_capacity, CircularBuffer<Entry>::REPLACE);
        const size_t frameCount = m_frames.size();
        for(size_t i = 0; i < frameCount; ++i) {
            const Entry& current = m_frames.peakAt(i);
            if(current.frame.timelineEpoch == timelineEpoch) {
                rebuilt.write(current);
            }
        }
        m_frames = rebuilt;
        markIndexDirty();
    }

    const InputFrame* findByFrame(uint32_t targetFrame) const
    {
        rebuildIndexIfNeeded();
        const auto it = m_latestIndexByFrame.find(targetFrame);
        if(it != m_latestIndexByFrame.end()) {
            return &m_frames.peakAt(it->second).frame;
        }
        return nullptr;
    }

    const InputFrame* findByFrame(uint32_t targetFrame, uint32_t targetTimelineEpoch) const
    {
        rebuildIndexIfNeeded();
        const auto it = m_indexByFrameEpoch.find(makeFrameEpochKey(targetFrame, targetTimelineEpoch));
        if(it != m_indexByFrameEpoch.end()) {
            return &m_frames.peakAt(it->second).frame;
        }
        return nullptr;
    }

    std::optional<uint32_t> latestFrameForTimelineEpoch(uint32_t timelineEpoch) const
    {
        rebuildIndexIfNeeded();
        const auto latestIt = m_latestFrameByEpoch.find(timelineEpoch);
        if(latestIt == m_latestFrameByEpoch.end()) {
            return std::nullopt;
        }
        return latestIt->second;
    }

    bool markConsumed(uint32_t frame, uint32_t timelineEpoch)
    {
        rebuildIndexIfNeeded();
        const auto it = m_indexByFrameEpoch.find(makeFrameEpochKey(frame, timelineEpoch));
        if(it != m_indexByFrameEpoch.end()) {
            m_frames.peakAt(it->second).consumed = true;
            return true;
        }
        return false;
    }

    bool isConsumed(uint32_t frame, uint32_t timelineEpoch) const
    {
        rebuildIndexIfNeeded();
        const auto it = m_indexByFrameEpoch.find(makeFrameEpochKey(frame, timelineEpoch));
        if(it != m_indexByFrameEpoch.end()) {
            return m_frames.peakAt(it->second).consumed;
        }
        return false;
    }

    EnqueueResult push(const InputFrame& frame, std::optional<uint32_t> expectedTimelineEpoch = std::nullopt)
    {
        if(expectedTimelineEpoch.has_value() && frame.timelineEpoch != *expectedTimelineEpoch) {
            ++m_enqueueCounters.rejectedEpoch;
            return EnqueueResult::RejectedEpoch;
        }

        rebuildIndexIfNeeded();
        const size_t frameCount = m_frames.size();
        const auto existingIt = m_indexByFrameEpoch.find(makeFrameEpochKey(frame.frame, frame.timelineEpoch));
        if(existingIt != m_indexByFrameEpoch.end()) {
            Entry& existing = m_frames.peakAt(existingIt->second);
            if(existing.consumed) {
                ++m_enqueueCounters.rejectedConsumed;
                return EnqueueResult::RejectedConsumed;
            }
            existing.frame = frame;
            ++m_enqueueCounters.updatedPending;
            markIndexDirty();
            return EnqueueResult::UpdatedPending;
        }

        const auto latestIt = m_latestFrameByEpoch.find(frame.timelineEpoch);
        const std::optional<uint32_t> latestFrameInEpoch =
            latestIt == m_latestFrameByEpoch.end()
                ? std::nullopt
                : std::optional<uint32_t>(latestIt->second);
        if(latestFrameInEpoch.has_value() && frame.frame != (*latestFrameInEpoch + 1u)) {
            ++m_enqueueCounters.rejectedOutOfSequence;
            return EnqueueResult::RejectedOutOfSequence;
        }

        if(frameCount == 0) {
            m_frames.write(Entry{frame, false});
            ++m_enqueueCounters.inserted;
            markIndexDirty();
            return EnqueueResult::Inserted;
        }

        const Entry& latest = m_frames.peakAt(frameCount - 1);
        if(frame.frame > latest.frame.frame) {
            m_frames.write(Entry{frame, false});
            ++m_enqueueCounters.inserted;
            markIndexDirty();
            return EnqueueResult::Inserted;
        }

        CircularBuffer<Entry> rebuilt(m_capacity, CircularBuffer<Entry>::REPLACE);
        bool inserted = false;
        for(size_t i = 0; i < frameCount; ++i) {
            const Entry& current = m_frames.peakAt(i);
            if(!inserted && frame.frame < current.frame.frame) {
                rebuilt.write(Entry{frame, false});
                inserted = true;
            }
            rebuilt.write(current);
        }
        if(!inserted) rebuilt.write(Entry{frame, false});
        m_frames = rebuilt;
        ++m_enqueueCounters.inserted;
        markIndexDirty();
        return EnqueueResult::Inserted;
    }

    void serialization(SerializationBase& s)
    {
        uint32_t capacity = static_cast<uint32_t>(m_capacity);
        SERIALIZEDATA(s, capacity);
        if(auto* deserialize = dynamic_cast<Deserialize*>(&s); deserialize != nullptr) {
            m_capacity = std::max<size_t>(1, capacity);
            m_frames = CircularBuffer<Entry>(m_capacity, CircularBuffer<Entry>::REPLACE);
            m_indexByFrameEpoch.clear();
            m_latestIndexByFrame.clear();
            m_latestFrameByEpoch.clear();
            markIndexDirty();
        }

        uint32_t frameCount = static_cast<uint32_t>(m_frames.size());
        SERIALIZEDATA(s, frameCount);
        if(auto* deserialize = dynamic_cast<Deserialize*>(&s); deserialize != nullptr) {
            m_frames.clear();
            m_indexByFrameEpoch.clear();
            m_latestIndexByFrame.clear();
            m_latestFrameByEpoch.clear();
            markIndexDirty();
        }

        for(uint32_t i = 0; i < frameCount; ++i) {
            if(auto* deserialize = dynamic_cast<Deserialize*>(&s); deserialize != nullptr) {
                InputFrame frame;
                frame.serialization(*deserialize);
                bool consumed = false;
                deserialize->single(reinterpret_cast<uint8_t*>(&consumed), sizeof(consumed));
                m_frames.write(Entry{frame, consumed});
            } else {
                Entry& entry = m_frames.peakAt(i);
                entry.frame.serialization(s);
                SERIALIZEDATA(s, entry.consumed);
            }
        }
        markIndexDirty();
    }
};
