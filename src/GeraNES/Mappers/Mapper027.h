#pragma once

#include "Mapper023.h"

// Mapper 027 behaves like the VRC4 wiring variant already covered by Mapper 023.
class Mapper027 : public Mapper023
{
public:
    Mapper027(ICartridgeData& cd) : Mapper023(cd)
    {
    }
};
