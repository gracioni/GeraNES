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
    struct PadButtons {
        bool a = false;
        bool b = false;
        bool select = false;
        bool start = false;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool x = false;
        bool y = false;
        bool l = false;
        bool r = false;
        bool up2 = false;
        bool down2 = false;
        bool left2 = false;
        bool right2 = false;
    };

    struct PointerState {
        int x = -1;
        int y = -1;
        bool trigger = false;
    };

    struct RelativePointerState {
        int deltaX = 0;
        int deltaY = 0;
        bool primary = false;
        bool secondary = false;
    };

    struct ArkanoidState {
        float position = 0.5f;
        bool button = false;
    };

    struct KonamiHyperShotState {
        bool p1Run = false;
        bool p1Jump = false;
        bool p2Run = false;
        bool p2Jump = false;
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

    bool multitapActive() const
    {
        return nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE ||
               famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER;
    }

    PadButtons portButtons(int port) const;
    void setPortButtons(int port, const PadButtons& buttons);
    PadButtons bandaiButtons() const;
    void setBandaiButtons(const PadButtons& buttons);
    PointerState zapper(int port) const;
    void setZapper(int port, const PointerState& state);
    PointerState bandaiPointer() const;
    void setBandaiPointer(const PointerState& state);
    ArkanoidState arkanoidController(int port) const;
    void setArkanoidController(int port, const ArkanoidState& state);
    ArkanoidState arkanoidExpansion() const;
    void setArkanoidExpansion(const ArkanoidState& state);
    RelativePointerState snesMouse(int port) const;
    void setSnesMouse(int port, const RelativePointerState& state);
    RelativePointerState suborMouse(int port) const;
    void setSuborMouse(int port, const RelativePointerState& state);
    KonamiHyperShotState konamiHyperShot() const;
    void setKonamiHyperShot(const KonamiHyperShotState& state);
    std::array<bool, 12> powerPadButtons(int port) const;
    void setPowerPadButtons(int port, const std::array<bool, 12>& buttons);
    IExpansionDevice::SuborKeyboardKeys suborKeyboardKeys() const;
    void setSuborKeyboardKeys(const IExpansionDevice::SuborKeyboardKeys& keys);
    IExpansionDevice::FamilyBasicKeyboardKeys familyBasicKeyboardKeys() const;
    void setFamilyBasicKeyboardKeys(const IExpansionDevice::FamilyBasicKeyboardKeys& keys);

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
        const PadButtons p1 = portButtons(1);
        const PadButtons p2 = portButtons(2);
        const PadButtons p3 = portButtons(3);
        const PadButtons p4 = portButtons(4);
        const PointerState zapperP1 = zapper(1);
        const PointerState zapperP2 = zapper(2);
        const PadButtons bandaiPad = bandaiButtons();
        const PointerState bandai = bandaiPointer();
        const ArkanoidState arkanoidP1 = arkanoidController(1);
        const ArkanoidState arkanoidP2 = arkanoidController(2);
        const ArkanoidState arkanoidFamicom = arkanoidExpansion();
        const KonamiHyperShotState konami = konamiHyperShot();
        const RelativePointerState snesMouseP1 = snesMouse(1);
        const RelativePointerState snesMouseP2 = snesMouse(2);
        const RelativePointerState suborMouseP1 = suborMouse(1);
        const RelativePointerState suborMouseP2 = suborMouse(2);
        const auto suborKeys = suborKeyboardKeys();
        const auto familyBasicKeys = familyBasicKeyboardKeys();
        const auto powerPadP1 = powerPadButtons(1);
        const auto powerPadP2 = powerPadButtons(2);
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
                {"a", p1.a}, {"b", p1.b}, {"select", p1.select}, {"start", p1.start},
                {"up", p1.up}, {"down", p1.down}, {"left", p1.left}, {"right", p1.right},
                {"x", p1.x}, {"y", p1.y}, {"l", p1.l}, {"r", p1.r}
            }},
            {"p2", {
                {"a", p2.a}, {"b", p2.b}, {"select", p2.select}, {"start", p2.start},
                {"up", p2.up}, {"down", p2.down}, {"left", p2.left}, {"right", p2.right},
                {"x", p2.x}, {"y", p2.y}, {"l", p2.l}, {"r", p2.r}
            }},
            {"p3", {
                {"a", p3.a}, {"b", p3.b}, {"select", p3.select}, {"start", p3.start},
                {"up", p3.up}, {"down", p3.down}, {"left", p3.left}, {"right", p3.right}
            }},
            {"p4", {
                {"a", p4.a}, {"b", p4.b}, {"select", p4.select}, {"start", p4.start},
                {"up", p4.up}, {"down", p4.down}, {"left", p4.left}, {"right", p4.right}
            }},
            {"zapper", {
                {"p1", {{"x", zapperP1.x}, {"y", zapperP1.y}, {"trigger", zapperP1.trigger}}},
                {"p2", {{"x", zapperP2.x}, {"y", zapperP2.y}, {"trigger", zapperP2.trigger}}}
            }},
            {"bandai", {
                {"a", bandaiPad.a}, {"b", bandaiPad.b}, {"select", bandaiPad.select}, {"start", bandaiPad.start},
                {"up", bandaiPad.up}, {"down", bandaiPad.down}, {"left", bandaiPad.left}, {"right", bandaiPad.right},
                {"x", bandai.x}, {"y", bandai.y}, {"trigger", bandai.trigger}
            }},
            {"arkanoid", {
                {"p1", {{"position", arkanoidP1.position}, {"button", arkanoidP1.button}}},
                {"p2", {{"position", arkanoidP2.position}, {"button", arkanoidP2.button}}},
                {"famicom", {{"position", arkanoidFamicom.position}, {"button", arkanoidFamicom.button}}}
            }},
            {"konami", {
                {"p1Run", konami.p1Run}, {"p1Jump", konami.p1Jump},
                {"p2Run", konami.p2Run}, {"p2Jump", konami.p2Jump}
            }},
            {"snesMouse", {
                {"p1", {{"deltaX", snesMouseP1.deltaX}, {"deltaY", snesMouseP1.deltaY}, {"left", snesMouseP1.primary}, {"right", snesMouseP1.secondary}}},
                {"p2", {{"deltaX", snesMouseP2.deltaX}, {"deltaY", snesMouseP2.deltaY}, {"left", snesMouseP2.primary}, {"right", snesMouseP2.secondary}}}
            }},
            {"virtualBoy", {
                {"p1", {
                    {"a", p1.a}, {"b", p1.b}, {"select", p1.select}, {"start", p1.start},
                    {"up0", p1.up}, {"down0", p1.down}, {"left0", p1.left}, {"right0", p1.right},
                    {"up1", p1.up2}, {"down1", p1.down2}, {"left1", p1.left2}, {"right1", p1.right2},
                    {"l", p1.l}, {"r", p1.r}
                }},
                {"p2", {
                    {"a", p2.a}, {"b", p2.b}, {"select", p2.select}, {"start", p2.start},
                    {"up0", p2.up}, {"down0", p2.down}, {"left0", p2.left}, {"right0", p2.right},
                    {"up1", p2.up2}, {"down1", p2.down2}, {"left1", p2.left2}, {"right1", p2.right2},
                    {"l", p2.l}, {"r", p2.r}
                }}
            }},
            {"suborMouse", {
                {"p1", {{"deltaX", suborMouseP1.deltaX}, {"deltaY", suborMouseP1.deltaY}, {"left", suborMouseP1.primary}, {"right", suborMouseP1.secondary}}},
                {"p2", {{"deltaX", suborMouseP2.deltaX}, {"deltaY", suborMouseP2.deltaY}, {"left", suborMouseP2.primary}, {"right", suborMouseP2.secondary}}}
            }},
            {"keyboard", {
                {"subor", suborKeys},
                {"familyBasic", familyBasicKeys}
            }},
            {"powerPad", {
                {"p1", powerPadP1},
                {"p2", powerPadP2}
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
