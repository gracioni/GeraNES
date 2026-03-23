#pragma once

#include "Helpers/Mmc3ChrRamBase.h"

class Mapper194 : public Mmc3ChrRamBase
{
public:
    Mapper194(ICartridgeData& cd) : Mmc3ChrRamBase(cd, 0x00, 0x01, 2)
    {
    }
};
