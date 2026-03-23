#pragma once

#include "Mapper243.h"

// Historical duplicate assignment for Sachen SA-020A / Honey Peach.
class Mapper110 : public Mapper243
{
public:
    Mapper110(ICartridgeData& cd) : Mapper243(cd)
    {
    }
};
