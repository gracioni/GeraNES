#pragma once

#include "Mapper141.h"

// Duplicate assignment of mapper 141 on some dumps.
class Mapper135 : public Mapper141
{
public:
    Mapper135(ICartridgeData& cd) : Mapper141(cd)
    {
    }
};
