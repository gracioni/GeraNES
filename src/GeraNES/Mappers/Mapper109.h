#pragma once

#include "Mapper137.h"

// Historical duplicate assignment for Sachen 8259D / The Great Wall.
class Mapper109 : public Mapper137
{
public:
    Mapper109(ICartridgeData& cd) : Mapper137(cd)
    {
    }
};
