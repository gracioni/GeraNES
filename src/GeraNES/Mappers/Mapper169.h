#pragma once

#include "Mapper015.h"

// Historical GoodNES assignment; NESdev notes the known dump behaves like mapper 15.
class Mapper169 : public Mapper015
{
public:
    Mapper169(ICartridgeData& cd) : Mapper015(cd)
    {
    }
};
