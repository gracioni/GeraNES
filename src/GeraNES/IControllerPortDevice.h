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

    virtual void setButtonsStatusExtended(bool bA, bool bB, bool bSelect, bool bStart,
                                          bool bUp, bool bDown, bool bLeft, bool bRight,
                                          bool bX, bool bY, bool bL, bool bR)
    {
        (void)bX;
        (void)bY;
        (void)bL;
        (void)bR;
        setButtonsStatus(bA, bB, bSelect, bStart, bUp, bDown, bLeft, bRight);
    }

    virtual void setVirtualBoyButtons(bool bA, bool bB, bool bSelect, bool bStart,
                                      bool bUp0, bool bDown0, bool bLeft0, bool bRight0,
                                      bool bUp1, bool bDown1, bool bLeft1, bool bRight1,
                                      bool bL, bool bR)
    {
        (void)bUp1;
        (void)bDown1;
        (void)bLeft1;
        (void)bRight1;
        setButtonsStatusExtended(bA, bB, bSelect, bStart, bUp0, bDown0, bLeft0, bRight0, false, false, bL, bR);
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

    virtual uint8_t extraRead4016Bits() const
    {
        return 0x00;
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
