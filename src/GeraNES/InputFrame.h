#pragma once

#include <array>
#include <cstdint>

#include "InputState.h"
#include "Serialization.h"

struct InputFrame
{
    using PadButtons = InputState::PadButtons;
    using PointerState = InputState::PointerState;
    using RelativePointerState = InputState::RelativePointerState;
    using ArkanoidState = InputState::ArkanoidState;
    using KonamiHyperShotState = InputState::KonamiHyperShotState;

    uint32_t frame = 0;
    InputState state;

    Settings::Device& port1Device = state.port1Device;
    Settings::Device& port2Device = state.port2Device;
    Settings::ExpansionDevice& expansionDevice = state.expansionDevice;
    Settings::NesMultitapDevice& nesMultitapDevice = state.nesMultitapDevice;
    Settings::FamicomMultitapDevice& famicomMultitapDevice = state.famicomMultitapDevice;
    std::vector<uint8_t>& serializedInputData = state.serializedInputData;

    InputFrame() = default;
    InputFrame(const InputFrame& other)
        : frame(other.frame), state(other.state)
    {
    }
    InputFrame(InputFrame&& other) noexcept
        : frame(other.frame), state(std::move(other.state))
    {
    }
    InputFrame& operator=(const InputFrame& other)
    {
        if(this != &other) {
            frame = other.frame;
            state = other.state;
        }
        return *this;
    }
    InputFrame& operator=(InputFrame&& other) noexcept
    {
        if(this != &other) {
            frame = other.frame;
            state = std::move(other.state);
        }
        return *this;
    }

    bool operator==(const InputFrame& other) const
    {
        return frame == other.frame &&
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
        state.serialization(s);
    }
};
