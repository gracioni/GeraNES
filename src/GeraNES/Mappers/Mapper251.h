#pragma once

#include "Mapper045.h"

// Duplicate assignment of mapper 45 on some multicart dumps.
class Mapper251 : public Mapper045
{
public:
    Mapper251(ICartridgeData& cd) : Mapper045(cd)
    {
    }
};
