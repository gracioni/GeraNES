#pragma once

#include "Mapper115.h"

// Duplicate assignment of mapper 115 on some dumps.
class Mapper248 : public Mapper115
{
public:
    Mapper248(ICartridgeData& cd) : Mapper115(cd)
    {
    }
};
