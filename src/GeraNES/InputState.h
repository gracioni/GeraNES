#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

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

};
