#pragma once

#include "Mapper119.h"

// Waixing MMC3 clone with mixed CHR-ROM/CHR-RAM behavior close to TQROM.
class Mapper074 : public Mapper119
{
public:
    Mapper074(ICartridgeData& cd) : Mapper119(cd)
    {
    }
};
