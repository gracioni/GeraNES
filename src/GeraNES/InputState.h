#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "IExpansionDevice.h"
#include "InputTopology.h"
#include "Serialization.h"

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

    InputTopology topology;
    std::vector<uint8_t> serializedInputData;

    bool operator==(const InputState&) const = default;
    bool operator!=(const InputState&) const = default;

    bool multitapActive() const
    {
        return topology.nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE ||
               topology.famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER;
    }

    Settings::Device portDevice(int port) const
    {
        if(port == 1) return topology.port1Device.value_or(Settings::Device::NONE);
        if(port == 2) return topology.port2Device.value_or(Settings::Device::NONE);
        return Settings::Device::NONE;
    }

    void setPortDevice(int port, std::optional<Settings::Device> device)
    {
        if(port == 1) topology.port1Device = device;
        else if(port == 2) topology.port2Device = device;
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
        SERIALIZEDATA(s, topology.port1Device);
        SERIALIZEDATA(s, topology.port2Device);
        SERIALIZEDATA(s, topology.expansionDevice);
        SERIALIZEDATA(s, topology.nesMultitapDevice);
        SERIALIZEDATA(s, topology.famicomMultitapDevice);
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

#include "InputState.inl"
