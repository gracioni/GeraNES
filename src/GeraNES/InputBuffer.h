#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "IExpansionDevice.h"
#include "Serialization.h"
#include "Settings.h"
#include "util/CircularBuffer.h"

struct InputFrame
{
    struct DecodedData {
        bool p1A = false;
        bool p1B = false;
        bool p1Select = false;
        bool p1Start = false;
        bool p1Up = false;
        bool p1Down = false;
        bool p1Left = false;
        bool p1Right = false;
        bool p1X = false;
        bool p1Y = false;
        bool p1L = false;
        bool p1R = false;

        bool p2A = false;
        bool p2B = false;
        bool p2Select = false;
        bool p2Start = false;
        bool p2Up = false;
        bool p2Down = false;
        bool p2Left = false;
        bool p2Right = false;
        bool p2X = false;
        bool p2Y = false;
        bool p2L = false;
        bool p2R = false;

        bool p3A = false;
        bool p3B = false;
        bool p3Select = false;
        bool p3Start = false;
        bool p3Up = false;
        bool p3Down = false;
        bool p3Left = false;
        bool p3Right = false;

        bool p4A = false;
        bool p4B = false;
        bool p4Select = false;
        bool p4Start = false;
        bool p4Up = false;
        bool p4Down = false;
        bool p4Left = false;
        bool p4Right = false;

        int zapperP1X = -1;
        int zapperP1Y = -1;
        bool zapperP1Trigger = false;
        int zapperP2X = -1;
        int zapperP2Y = -1;
        bool zapperP2Trigger = false;

        bool bandaiA = false;
        bool bandaiB = false;
        bool bandaiSelect = false;
        bool bandaiStart = false;
        bool bandaiUp = false;
        bool bandaiDown = false;
        bool bandaiLeft = false;
        bool bandaiRight = false;
        int bandaiX = -1;
        int bandaiY = -1;
        bool bandaiTrigger = false;

        float arkanoidP1Position = 0.5f;
        bool arkanoidP1Button = false;
        float arkanoidP2Position = 0.5f;
        bool arkanoidP2Button = false;
        float arkanoidFamicomPosition = 0.5f;
        bool arkanoidFamicomButton = false;

        bool konamiP1Run = false;
        bool konamiP1Jump = false;
        bool konamiP2Run = false;
        bool konamiP2Jump = false;

        int snesMouseP1DeltaX = 0;
        int snesMouseP1DeltaY = 0;
        bool snesMouseP1Left = false;
        bool snesMouseP1Right = false;
        int snesMouseP2DeltaX = 0;
        int snesMouseP2DeltaY = 0;
        bool snesMouseP2Left = false;
        bool snesMouseP2Right = false;

        bool vbP1A = false;
        bool vbP1B = false;
        bool vbP1Select = false;
        bool vbP1Start = false;
        bool vbP1Up0 = false;
        bool vbP1Down0 = false;
        bool vbP1Left0 = false;
        bool vbP1Right0 = false;
        bool vbP1Up1 = false;
        bool vbP1Down1 = false;
        bool vbP1Left1 = false;
        bool vbP1Right1 = false;
        bool vbP1L = false;
        bool vbP1R = false;

        bool vbP2A = false;
        bool vbP2B = false;
        bool vbP2Select = false;
        bool vbP2Start = false;
        bool vbP2Up0 = false;
        bool vbP2Down0 = false;
        bool vbP2Left0 = false;
        bool vbP2Right0 = false;
        bool vbP2Up1 = false;
        bool vbP2Down1 = false;
        bool vbP2Left1 = false;
        bool vbP2Right1 = false;
        bool vbP2L = false;
        bool vbP2R = false;

        int suborMouseP1DeltaX = 0;
        int suborMouseP1DeltaY = 0;
        bool suborMouseP1Left = false;
        bool suborMouseP1Right = false;
        int suborMouseP2DeltaX = 0;
        int suborMouseP2DeltaY = 0;
        bool suborMouseP2Left = false;
        bool suborMouseP2Right = false;

        IExpansionDevice::SuborKeyboardKeys suborKeyboardKeys = {};
        IExpansionDevice::FamilyBasicKeyboardKeys familyBasicKeyboardKeys = {};
        std::array<bool, 12> powerPadP1Buttons = {};
        std::array<bool, 12> powerPadP2Buttons = {};
    };

    uint32_t frame = 0;
    uint32_t timelineEpoch = 0;

    Settings::Device port1Device = Settings::Device::NONE;
    Settings::Device port2Device = Settings::Device::NONE;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
    std::vector<uint8_t> serializedInputData;

    bool operator==(const InputFrame&) const = default;
    bool operator!=(const InputFrame&) const = default;

    DecodedData decodedData() const;
    bool setDecodedData(const DecodedData& decoded);

    static InputFrame repeatedFrom(const InputFrame& previous, uint32_t targetFrame)
    {
        InputFrame repeated = previous;
        repeated.frame = targetFrame;
        DecodedData decoded = repeated.decodedData();
        decoded.snesMouseP1DeltaX = 0;
        decoded.snesMouseP1DeltaY = 0;
        decoded.snesMouseP2DeltaX = 0;
        decoded.snesMouseP2DeltaY = 0;
        decoded.suborMouseP1DeltaX = 0;
        decoded.suborMouseP1DeltaY = 0;
        decoded.suborMouseP2DeltaX = 0;
        decoded.suborMouseP2DeltaY = 0;
        (void)repeated.setDecodedData(decoded);
        return repeated;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, frame);
        SERIALIZEDATA(s, timelineEpoch);
        SERIALIZEDATA(s, port1Device);
        SERIALIZEDATA(s, port2Device);
        SERIALIZEDATA(s, expansionDevice);
        SERIALIZEDATA(s, nesMultitapDevice);
        SERIALIZEDATA(s, famicomMultitapDevice);
        uint32_t serializedSize = static_cast<uint32_t>(serializedInputData.size());
        SERIALIZEDATA(s, serializedSize);
        if(auto* deserialize = dynamic_cast<Deserialize*>(&s); deserialize != nullptr) {
            serializedInputData.assign(serializedSize, 0u);
            if(serializedSize != 0u) {
                deserialize->array(serializedInputData.data(), 1, serializedSize);
            }
        } else if(serializedSize != 0u) {
            s.array(serializedInputData.data(), 1, serializedSize);
        }
    }

    nlohmann::json toJson() const
    {
        const DecodedData decoded = decodedData();
        return {
            {"frame", frame},
            {"timelineEpoch", timelineEpoch},
            {"devices", {
                {"port1", static_cast<int>(port1Device)},
                {"port2", static_cast<int>(port2Device)},
                {"expansion", static_cast<int>(expansionDevice)},
                {"nesMultitap", static_cast<int>(nesMultitapDevice)},
                {"famicomMultitap", static_cast<int>(famicomMultitapDevice)}
            }},
            {"serializedInputSize", serializedInputData.size()},
            {"p1", {
                {"a", decoded.p1A}, {"b", decoded.p1B}, {"select", decoded.p1Select}, {"start", decoded.p1Start},
                {"up", decoded.p1Up}, {"down", decoded.p1Down}, {"left", decoded.p1Left}, {"right", decoded.p1Right},
                {"x", decoded.p1X}, {"y", decoded.p1Y}, {"l", decoded.p1L}, {"r", decoded.p1R}
            }},
            {"p2", {
                {"a", decoded.p2A}, {"b", decoded.p2B}, {"select", decoded.p2Select}, {"start", decoded.p2Start},
                {"up", decoded.p2Up}, {"down", decoded.p2Down}, {"left", decoded.p2Left}, {"right", decoded.p2Right},
                {"x", decoded.p2X}, {"y", decoded.p2Y}, {"l", decoded.p2L}, {"r", decoded.p2R}
            }},
            {"p3", {
                {"a", decoded.p3A}, {"b", decoded.p3B}, {"select", decoded.p3Select}, {"start", decoded.p3Start},
                {"up", decoded.p3Up}, {"down", decoded.p3Down}, {"left", decoded.p3Left}, {"right", decoded.p3Right}
            }},
            {"p4", {
                {"a", decoded.p4A}, {"b", decoded.p4B}, {"select", decoded.p4Select}, {"start", decoded.p4Start},
                {"up", decoded.p4Up}, {"down", decoded.p4Down}, {"left", decoded.p4Left}, {"right", decoded.p4Right}
            }},
            {"zapper", {
                {"p1", {{"x", decoded.zapperP1X}, {"y", decoded.zapperP1Y}, {"trigger", decoded.zapperP1Trigger}}},
                {"p2", {{"x", decoded.zapperP2X}, {"y", decoded.zapperP2Y}, {"trigger", decoded.zapperP2Trigger}}}
            }},
            {"bandai", {
                {"a", decoded.bandaiA}, {"b", decoded.bandaiB}, {"select", decoded.bandaiSelect}, {"start", decoded.bandaiStart},
                {"up", decoded.bandaiUp}, {"down", decoded.bandaiDown}, {"left", decoded.bandaiLeft}, {"right", decoded.bandaiRight},
                {"x", decoded.bandaiX}, {"y", decoded.bandaiY}, {"trigger", decoded.bandaiTrigger}
            }},
            {"arkanoid", {
                {"p1", {{"position", decoded.arkanoidP1Position}, {"button", decoded.arkanoidP1Button}}},
                {"p2", {{"position", decoded.arkanoidP2Position}, {"button", decoded.arkanoidP2Button}}},
                {"famicom", {{"position", decoded.arkanoidFamicomPosition}, {"button", decoded.arkanoidFamicomButton}}}
            }},
            {"konami", {
                {"p1Run", decoded.konamiP1Run}, {"p1Jump", decoded.konamiP1Jump},
                {"p2Run", decoded.konamiP2Run}, {"p2Jump", decoded.konamiP2Jump}
            }},
            {"snesMouse", {
                {"p1", {{"deltaX", decoded.snesMouseP1DeltaX}, {"deltaY", decoded.snesMouseP1DeltaY}, {"left", decoded.snesMouseP1Left}, {"right", decoded.snesMouseP1Right}}},
                {"p2", {{"deltaX", decoded.snesMouseP2DeltaX}, {"deltaY", decoded.snesMouseP2DeltaY}, {"left", decoded.snesMouseP2Left}, {"right", decoded.snesMouseP2Right}}}
            }},
            {"virtualBoy", {
                {"p1", {
                    {"a", decoded.vbP1A}, {"b", decoded.vbP1B}, {"select", decoded.vbP1Select}, {"start", decoded.vbP1Start},
                    {"up0", decoded.vbP1Up0}, {"down0", decoded.vbP1Down0}, {"left0", decoded.vbP1Left0}, {"right0", decoded.vbP1Right0},
                    {"up1", decoded.vbP1Up1}, {"down1", decoded.vbP1Down1}, {"left1", decoded.vbP1Left1}, {"right1", decoded.vbP1Right1},
                    {"l", decoded.vbP1L}, {"r", decoded.vbP1R}
                }},
                {"p2", {
                    {"a", decoded.vbP2A}, {"b", decoded.vbP2B}, {"select", decoded.vbP2Select}, {"start", decoded.vbP2Start},
                    {"up0", decoded.vbP2Up0}, {"down0", decoded.vbP2Down0}, {"left0", decoded.vbP2Left0}, {"right0", decoded.vbP2Right0},
                    {"up1", decoded.vbP2Up1}, {"down1", decoded.vbP2Down1}, {"left1", decoded.vbP2Left1}, {"right1", decoded.vbP2Right1},
                    {"l", decoded.vbP2L}, {"r", decoded.vbP2R}
                }}
            }},
            {"suborMouse", {
                {"p1", {{"deltaX", decoded.suborMouseP1DeltaX}, {"deltaY", decoded.suborMouseP1DeltaY}, {"left", decoded.suborMouseP1Left}, {"right", decoded.suborMouseP1Right}}},
                {"p2", {{"deltaX", decoded.suborMouseP2DeltaX}, {"deltaY", decoded.suborMouseP2DeltaY}, {"left", decoded.suborMouseP2Left}, {"right", decoded.suborMouseP2Right}}}
            }},
            {"keyboard", {
                {"subor", decoded.suborKeyboardKeys},
                {"familyBasic", decoded.familyBasicKeyboardKeys}
            }},
            {"powerPad", {
                {"p1", decoded.powerPadP1Buttons},
                {"p2", decoded.powerPadP2Buttons}
            }}
        };
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
