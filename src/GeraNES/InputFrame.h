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

    bool operator==(const InputFrame& other) const
    {
        return frame == other.frame &&
               state == other.state;
    }
    bool operator!=(const InputFrame& other) const
    {
        return !(*this == other);
    }

    static InputFrame repeatedFrom(const InputFrame& previous, uint32_t targetFrame)
    {
        InputFrame repeated = previous;
        repeated.frame = targetFrame;
        InputState::RelativePointerState snes1 = repeated.state.snesMouse(1);
        InputState::RelativePointerState snes2 = repeated.state.snesMouse(2);
        InputState::RelativePointerState subor1 = repeated.state.suborMouse(1);
        InputState::RelativePointerState subor2 = repeated.state.suborMouse(2);
        snes1.deltaX = 0;
        snes1.deltaY = 0;
        snes2.deltaX = 0;
        snes2.deltaY = 0;
        subor1.deltaX = 0;
        subor1.deltaY = 0;
        subor2.deltaX = 0;
        subor2.deltaY = 0;
        repeated.state.setSnesMouse(1, snes1);
        repeated.state.setSnesMouse(2, snes2);
        repeated.state.setSuborMouse(1, subor1);
        repeated.state.setSuborMouse(2, subor2);
        return repeated;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, frame);
        state.serialization(s);
    }
};
