#pragma once

#include "Helpers/Sachen8259Base.h"

class Mapper137 : public Sachen8259Base
{
public:
    Mapper137(ICartridgeData& cd) : Sachen8259Base(cd, Sachen8259Variant::Sachen8259D)
    {
    }
};
