#pragma once

#include "defines.h"

class Ibus
{
public:

    virtual uint8_t read(int addr) = 0;
    virtual void write(int addr, uint8_t data) = 0;
    virtual uint8_t getOpenBus() const = 0;
    virtual void onCpuGetToPutTransition() = 0;
    virtual ~Ibus(){}
};
