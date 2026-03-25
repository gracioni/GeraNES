#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "IExpansionDevice.h"
#include "Serialization.h"
#include "Settings.h"
#include "util/CircularBuffer.h"

struct InputFrame
{
    uint32_t frame = 0;

    Settings::Device port1Device = Settings::Device::NONE;
    Settings::Device port2Device = Settings::Device::NONE;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;

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

    static InputFrame repeatedFrom(const InputFrame& previous, uint32_t targetFrame)
    {
        InputFrame repeated = previous;
        repeated.frame = targetFrame;
        repeated.snesMouseP1DeltaX = 0;
        repeated.snesMouseP1DeltaY = 0;
        repeated.snesMouseP2DeltaX = 0;
        repeated.snesMouseP2DeltaY = 0;
        repeated.suborMouseP1DeltaX = 0;
        repeated.suborMouseP1DeltaY = 0;
        repeated.suborMouseP2DeltaX = 0;
        repeated.suborMouseP2DeltaY = 0;
        return repeated;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, frame);
        SERIALIZEDATA(s, port1Device);
        SERIALIZEDATA(s, port2Device);
        SERIALIZEDATA(s, expansionDevice);
        SERIALIZEDATA(s, nesMultitapDevice);
        SERIALIZEDATA(s, famicomMultitapDevice);

        SERIALIZEDATA(s, p1A); SERIALIZEDATA(s, p1B); SERIALIZEDATA(s, p1Select); SERIALIZEDATA(s, p1Start);
        SERIALIZEDATA(s, p1Up); SERIALIZEDATA(s, p1Down); SERIALIZEDATA(s, p1Left); SERIALIZEDATA(s, p1Right);
        SERIALIZEDATA(s, p1X); SERIALIZEDATA(s, p1Y); SERIALIZEDATA(s, p1L); SERIALIZEDATA(s, p1R);

        SERIALIZEDATA(s, p2A); SERIALIZEDATA(s, p2B); SERIALIZEDATA(s, p2Select); SERIALIZEDATA(s, p2Start);
        SERIALIZEDATA(s, p2Up); SERIALIZEDATA(s, p2Down); SERIALIZEDATA(s, p2Left); SERIALIZEDATA(s, p2Right);
        SERIALIZEDATA(s, p2X); SERIALIZEDATA(s, p2Y); SERIALIZEDATA(s, p2L); SERIALIZEDATA(s, p2R);

        SERIALIZEDATA(s, p3A); SERIALIZEDATA(s, p3B); SERIALIZEDATA(s, p3Select); SERIALIZEDATA(s, p3Start);
        SERIALIZEDATA(s, p3Up); SERIALIZEDATA(s, p3Down); SERIALIZEDATA(s, p3Left); SERIALIZEDATA(s, p3Right);

        SERIALIZEDATA(s, p4A); SERIALIZEDATA(s, p4B); SERIALIZEDATA(s, p4Select); SERIALIZEDATA(s, p4Start);
        SERIALIZEDATA(s, p4Up); SERIALIZEDATA(s, p4Down); SERIALIZEDATA(s, p4Left); SERIALIZEDATA(s, p4Right);

        SERIALIZEDATA(s, zapperP1X); SERIALIZEDATA(s, zapperP1Y); SERIALIZEDATA(s, zapperP1Trigger);
        SERIALIZEDATA(s, zapperP2X); SERIALIZEDATA(s, zapperP2Y); SERIALIZEDATA(s, zapperP2Trigger);

        SERIALIZEDATA(s, bandaiA); SERIALIZEDATA(s, bandaiB); SERIALIZEDATA(s, bandaiSelect); SERIALIZEDATA(s, bandaiStart);
        SERIALIZEDATA(s, bandaiUp); SERIALIZEDATA(s, bandaiDown); SERIALIZEDATA(s, bandaiLeft); SERIALIZEDATA(s, bandaiRight);
        SERIALIZEDATA(s, bandaiX); SERIALIZEDATA(s, bandaiY); SERIALIZEDATA(s, bandaiTrigger);

        SERIALIZEDATA(s, arkanoidP1Position); SERIALIZEDATA(s, arkanoidP1Button);
        SERIALIZEDATA(s, arkanoidP2Position); SERIALIZEDATA(s, arkanoidP2Button);
        SERIALIZEDATA(s, arkanoidFamicomPosition); SERIALIZEDATA(s, arkanoidFamicomButton);

        SERIALIZEDATA(s, konamiP1Run); SERIALIZEDATA(s, konamiP1Jump); SERIALIZEDATA(s, konamiP2Run); SERIALIZEDATA(s, konamiP2Jump);

        SERIALIZEDATA(s, snesMouseP1DeltaX); SERIALIZEDATA(s, snesMouseP1DeltaY); SERIALIZEDATA(s, snesMouseP1Left); SERIALIZEDATA(s, snesMouseP1Right);
        SERIALIZEDATA(s, snesMouseP2DeltaX); SERIALIZEDATA(s, snesMouseP2DeltaY); SERIALIZEDATA(s, snesMouseP2Left); SERIALIZEDATA(s, snesMouseP2Right);

        SERIALIZEDATA(s, vbP1A); SERIALIZEDATA(s, vbP1B); SERIALIZEDATA(s, vbP1Select); SERIALIZEDATA(s, vbP1Start);
        SERIALIZEDATA(s, vbP1Up0); SERIALIZEDATA(s, vbP1Down0); SERIALIZEDATA(s, vbP1Left0); SERIALIZEDATA(s, vbP1Right0);
        SERIALIZEDATA(s, vbP1Up1); SERIALIZEDATA(s, vbP1Down1); SERIALIZEDATA(s, vbP1Left1); SERIALIZEDATA(s, vbP1Right1); SERIALIZEDATA(s, vbP1L); SERIALIZEDATA(s, vbP1R);

        SERIALIZEDATA(s, vbP2A); SERIALIZEDATA(s, vbP2B); SERIALIZEDATA(s, vbP2Select); SERIALIZEDATA(s, vbP2Start);
        SERIALIZEDATA(s, vbP2Up0); SERIALIZEDATA(s, vbP2Down0); SERIALIZEDATA(s, vbP2Left0); SERIALIZEDATA(s, vbP2Right0);
        SERIALIZEDATA(s, vbP2Up1); SERIALIZEDATA(s, vbP2Down1); SERIALIZEDATA(s, vbP2Left1); SERIALIZEDATA(s, vbP2Right1); SERIALIZEDATA(s, vbP2L); SERIALIZEDATA(s, vbP2R);

        SERIALIZEDATA(s, suborMouseP1DeltaX); SERIALIZEDATA(s, suborMouseP1DeltaY); SERIALIZEDATA(s, suborMouseP1Left); SERIALIZEDATA(s, suborMouseP1Right);
        SERIALIZEDATA(s, suborMouseP2DeltaX); SERIALIZEDATA(s, suborMouseP2DeltaY); SERIALIZEDATA(s, suborMouseP2Left); SERIALIZEDATA(s, suborMouseP2Right);
        SERIALIZEDATA(s, suborKeyboardKeys); SERIALIZEDATA(s, familyBasicKeyboardKeys);
        s.array(reinterpret_cast<uint8_t*>(powerPadP1Buttons.data()), 1, powerPadP1Buttons.size());
        s.array(reinterpret_cast<uint8_t*>(powerPadP2Buttons.data()), 1, powerPadP2Buttons.size());
    }

    nlohmann::json toJson() const
    {
        return {
            {"frame", frame},
            {"devices", {
                {"port1", static_cast<int>(port1Device)},
                {"port2", static_cast<int>(port2Device)},
                {"expansion", static_cast<int>(expansionDevice)},
                {"nesMultitap", static_cast<int>(nesMultitapDevice)},
                {"famicomMultitap", static_cast<int>(famicomMultitapDevice)}
            }},
            {"p1", {
                {"a", p1A}, {"b", p1B}, {"select", p1Select}, {"start", p1Start},
                {"up", p1Up}, {"down", p1Down}, {"left", p1Left}, {"right", p1Right},
                {"x", p1X}, {"y", p1Y}, {"l", p1L}, {"r", p1R}
            }},
            {"p2", {
                {"a", p2A}, {"b", p2B}, {"select", p2Select}, {"start", p2Start},
                {"up", p2Up}, {"down", p2Down}, {"left", p2Left}, {"right", p2Right},
                {"x", p2X}, {"y", p2Y}, {"l", p2L}, {"r", p2R}
            }},
            {"p3", {
                {"a", p3A}, {"b", p3B}, {"select", p3Select}, {"start", p3Start},
                {"up", p3Up}, {"down", p3Down}, {"left", p3Left}, {"right", p3Right}
            }},
            {"p4", {
                {"a", p4A}, {"b", p4B}, {"select", p4Select}, {"start", p4Start},
                {"up", p4Up}, {"down", p4Down}, {"left", p4Left}, {"right", p4Right}
            }},
            {"zapper", {
                {"p1", {{"x", zapperP1X}, {"y", zapperP1Y}, {"trigger", zapperP1Trigger}}},
                {"p2", {{"x", zapperP2X}, {"y", zapperP2Y}, {"trigger", zapperP2Trigger}}}
            }},
            {"bandai", {
                {"a", bandaiA}, {"b", bandaiB}, {"select", bandaiSelect}, {"start", bandaiStart},
                {"up", bandaiUp}, {"down", bandaiDown}, {"left", bandaiLeft}, {"right", bandaiRight},
                {"x", bandaiX}, {"y", bandaiY}, {"trigger", bandaiTrigger}
            }},
            {"arkanoid", {
                {"p1", {{"position", arkanoidP1Position}, {"button", arkanoidP1Button}}},
                {"p2", {{"position", arkanoidP2Position}, {"button", arkanoidP2Button}}},
                {"famicom", {{"position", arkanoidFamicomPosition}, {"button", arkanoidFamicomButton}}}
            }},
            {"konami", {
                {"p1Run", konamiP1Run}, {"p1Jump", konamiP1Jump},
                {"p2Run", konamiP2Run}, {"p2Jump", konamiP2Jump}
            }},
            {"snesMouse", {
                {"p1", {{"deltaX", snesMouseP1DeltaX}, {"deltaY", snesMouseP1DeltaY}, {"left", snesMouseP1Left}, {"right", snesMouseP1Right}}},
                {"p2", {{"deltaX", snesMouseP2DeltaX}, {"deltaY", snesMouseP2DeltaY}, {"left", snesMouseP2Left}, {"right", snesMouseP2Right}}}
            }},
            {"virtualBoy", {
                {"p1", {
                    {"a", vbP1A}, {"b", vbP1B}, {"select", vbP1Select}, {"start", vbP1Start},
                    {"up0", vbP1Up0}, {"down0", vbP1Down0}, {"left0", vbP1Left0}, {"right0", vbP1Right0},
                    {"up1", vbP1Up1}, {"down1", vbP1Down1}, {"left1", vbP1Left1}, {"right1", vbP1Right1},
                    {"l", vbP1L}, {"r", vbP1R}
                }},
                {"p2", {
                    {"a", vbP2A}, {"b", vbP2B}, {"select", vbP2Select}, {"start", vbP2Start},
                    {"up0", vbP2Up0}, {"down0", vbP2Down0}, {"left0", vbP2Left0}, {"right0", vbP2Right0},
                    {"up1", vbP2Up1}, {"down1", vbP2Down1}, {"left1", vbP2Left1}, {"right1", vbP2Right1},
                    {"l", vbP2L}, {"r", vbP2R}
                }}
            }},
            {"suborMouse", {
                {"p1", {{"deltaX", suborMouseP1DeltaX}, {"deltaY", suborMouseP1DeltaY}, {"left", suborMouseP1Left}, {"right", suborMouseP1Right}}},
                {"p2", {{"deltaX", suborMouseP2DeltaX}, {"deltaY", suborMouseP2DeltaY}, {"left", suborMouseP2Left}, {"right", suborMouseP2Right}}}
            }},
            {"keyboard", {
                {"subor", suborKeyboardKeys},
                {"familyBasic", familyBasicKeyboardKeys}
            }},
            {"powerPad", {
                {"p1", powerPadP1Buttons},
                {"p2", powerPadP2Buttons}
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
private:
    size_t m_capacity = 1000;
    CircularBuffer<InputFrame> m_frames;

public:
    explicit InputBuffer(size_t capacity = 1000)
        : m_capacity(std::max<size_t>(1, capacity))
        , m_frames(m_capacity, CircularBuffer<InputFrame>::REPLACE)
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
    }

    const InputFrame* findByFrame(uint32_t targetFrame) const
    {
        for(size_t i = m_frames.size(); i > 0; --i) {
            const InputFrame& frame = m_frames.peakAt(i - 1);
            if(frame.frame == targetFrame) return &frame;
            if(frame.frame < targetFrame) break;
        }
        return nullptr;
    }

    void push(const InputFrame& frame)
    {
        const size_t frameCount = m_frames.size();

        for(size_t i = 0; i < frameCount; ++i) {
            InputFrame& existing = m_frames.peakAt(i);
            if(existing.frame == frame.frame) {
                existing = frame;
                return;
            }
        }

        if(frameCount == 0) {
            m_frames.write(frame);
            return;
        }

        const InputFrame& latest = m_frames.peakAt(frameCount - 1);
        if(frame.frame > latest.frame) {
            m_frames.write(frame);
            return;
        }

        CircularBuffer<InputFrame> rebuilt(m_capacity, CircularBuffer<InputFrame>::REPLACE);
        bool inserted = false;
        for(size_t i = 0; i < frameCount; ++i) {
            const InputFrame& current = m_frames.peakAt(i);
            if(!inserted && frame.frame < current.frame) {
                rebuilt.write(frame);
                inserted = true;
            }
            rebuilt.write(current);
        }
        if(!inserted) rebuilt.write(frame);
        m_frames = rebuilt;
    }

    void serialization(SerializationBase& s)
    {
        uint32_t capacity = static_cast<uint32_t>(m_capacity);
        SERIALIZEDATA(s, capacity);
        if(auto* deserialize = dynamic_cast<Deserialize*>(&s); deserialize != nullptr) {
            m_capacity = std::max<size_t>(1, capacity);
            m_frames = CircularBuffer<InputFrame>(m_capacity, CircularBuffer<InputFrame>::REPLACE);
        }

        uint32_t frameCount = static_cast<uint32_t>(m_frames.size());
        SERIALIZEDATA(s, frameCount);
        if(auto* deserialize = dynamic_cast<Deserialize*>(&s); deserialize != nullptr) {
            m_frames.clear();
        }

        for(uint32_t i = 0; i < frameCount; ++i) {
            if(auto* deserialize = dynamic_cast<Deserialize*>(&s); deserialize != nullptr) {
                InputFrame frame;
                frame.serialization(*deserialize);
                m_frames.write(frame);
            } else {
                m_frames.peakAt(i).serialization(s);
            }
        }
    }
};
