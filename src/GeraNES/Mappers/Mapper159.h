#pragma once

#include "Mapper016.h"

// iNES mapper 159 is the Bandai LZ93D50 variant that behaves like
// mapper 16 submapper 5, except it uses a 128-byte X24C01 EEPROM.
class Mapper159 : public Mapper016
{
protected:
    GERANES_INLINE bool uses6000WriteRange() const override
    {
        return false;
    }

public:
    Mapper159(ICartridgeData& cd) : Mapper016(cd)
    {
    }
};
