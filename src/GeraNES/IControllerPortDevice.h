#pragma once

#include <cstdint>
#include <functional>
#include <array>

#include "Serialization.h"

class IControllerPortDevice
{
public:
    virtual ~IControllerPortDevice() = default;

    virtual uint8_t read(bool outputEnabled) = 0;
    virtual void write(uint8_t data) = 0;
    virtual void serialization(SerializationBase& s) = 0;

    virtual void onCpuGetToPutTransition() {}
    virtual void onScanlineChanged() {}

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

    virtual void addRelativeMotion(int dx, int dy)
    {
        (void)dx;
        (void)dy;
    }

    virtual void setPositionNormalized(float position)
    {
        (void)position;
    }

    virtual void setTrigger(bool pressed)
    {
        (void)pressed;
    }

    virtual void setSecondaryTrigger(bool pressed)
    {
        (void)pressed;
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
