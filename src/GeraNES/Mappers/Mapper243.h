#pragma once

#include "Helpers/Sachen74LS374NBase.h"

class Mapper243 : public Sachen74LS374NBase
{
public:
    Mapper243(ICartridgeData& cd) : Sachen74LS374NBase(cd, false)
    {
    }
};
