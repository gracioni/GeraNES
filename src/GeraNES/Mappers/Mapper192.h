#pragma once

#include "Helpers/Mmc3ChrRamBase.h"

class Mapper192 : public Mmc3ChrRamBase
{
public:
    Mapper192(ICartridgeData& cd) : Mmc3ChrRamBase(cd, 0x08, 0x0B, 4)
    {
    }
};
