#pragma once

#include "Mapper185.h"

// Historical checksum-patch assignment; NESdev recommends treating it like mapper 185.
class Mapper181 : public Mapper185
{
public:
    Mapper181(ICartridgeData& cd) : Mapper185(cd)
    {
    }
};
