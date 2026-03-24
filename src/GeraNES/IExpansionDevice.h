#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "Serialization.h"

class IExpansionDevice
{
public:
    virtual ~IExpansionDevice() = default;

    virtual void write4016(uint8_t data)
    {
        (void)data;
    }

    virtual uint8_t read4016(bool outputEnabled)
    {
        (void)outputEnabled;
        return 0x00;
    }

    virtual uint8_t read4017(bool outputEnabled)
    {
        (void)outputEnabled;
        return 0x00;
    }

    virtual void onScanlineChanged() {}
    virtual void serialization(SerializationBase& s)
    {
        (void)s;
    }

    virtual void setButtonsStatus(bool bA, bool bB, bool bSelect, bool bStart,
                                  bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        (void)bA;
        (void)bB;
        (void)bSelect;
        (void)bStart;
        (void)bUp;
        (void)bDown;
        (void)bLeft;
        (void)bRight;
    }

    virtual void setCursorPosition(int x, int y)
    {
        (void)x;
        (void)y;
    }

    virtual void setPositionNormalized(float position)
    {
        (void)position;
    }

    virtual void setTrigger(bool pressed)
    {
        (void)pressed;
    }

    virtual void setPlayersButtons(bool p1Run, bool p1Jump, bool p2Run, bool p2Jump)
    {
        (void)p1Run;
        (void)p1Jump;
        (void)p2Run;
        (void)p2Jump;
    }

    virtual void setPowerPadButtons(const std::array<bool, 12>& buttons)
    {
        (void)buttons;
    }

    virtual void setPixelChecker(std::function<float(int, int)> func)
    {
        (void)func;
    }
};
