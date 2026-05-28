#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "IExpansionDevice.h"
#include "Serialization.h"
#include "Settings.h"

struct InputState
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

    Settings::Device port1Device = Settings::Device::NONE;
    Settings::Device port2Device = Settings::Device::NONE;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
    std::vector<uint8_t> serializedInputData;

    bool operator==(const InputState&) const = default;
    bool operator!=(const InputState&) const = default;

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

    void serialization(SerializationBase& s)
    {
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
