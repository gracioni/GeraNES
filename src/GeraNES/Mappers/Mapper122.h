#pragma once

#include "Mapper184.h"

// Duplicate assignment of mapper 184 on some dumps.
class Mapper122 : public Mapper184
{
public:
    Mapper122(ICartridgeData& cd) : Mapper184(cd)
    {
    }
};
