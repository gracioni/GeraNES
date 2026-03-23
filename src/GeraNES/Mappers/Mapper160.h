#pragma once

#include "Mapper090.h"

// Duplicate assignment of mapper 90 on some dumps.
class Mapper160 : public Mapper090
{
public:
    Mapper160(ICartridgeData& cd) : Mapper090(cd)
    {
    }
};
