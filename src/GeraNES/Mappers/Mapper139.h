#pragma once

#include "Helpers/Sachen8259Base.h"

class Mapper139 : public Sachen8259Base
{
public:
    Mapper139(ICartridgeData& cd) : Sachen8259Base(cd, Sachen8259Variant::Sachen8259C)
    {
    }
};
