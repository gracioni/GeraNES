#pragma once

#include "Helpers/Sachen74LS374NBase.h"

class Mapper150 : public Sachen74LS374NBase
{
public:
    Mapper150(ICartridgeData& cd) : Sachen74LS374NBase(cd, true)
    {
    }
};
