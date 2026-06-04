#pragma once

#include "Helpers/Mmc3ChrRamBase.h"

namespace GeraNES {

class Mapper191 : public Mmc3ChrRamBase
{
public:
    Mapper191(ICartridgeData& cd) : Mmc3ChrRamBase(cd, 0x80, 0xFF, 2)
    {
    }
};

} // namespace GeraNES
