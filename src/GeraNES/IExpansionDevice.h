#pragma once

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

    virtual uint8_t read4017()
    {
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

    virtual void setPixelChecker(std::function<float(int, int)> func)
    {
        (void)func;
    }
};
