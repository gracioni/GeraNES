#pragma once

#include "Helpers/Mmc3ChrRamBase.h"

class Mapper195 : public Mmc3ChrRamBase
{
public:
    Mapper195(ICartridgeData& cd) : Mmc3ChrRamBase(cd, 0x00, 0x03, 4)
    {
    }
};
