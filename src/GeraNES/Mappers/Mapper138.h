#pragma once

#include "Helpers/Sachen8259Base.h"

class Mapper138 : public Sachen8259Base
{
public:
    Mapper138(ICartridgeData& cd) : Sachen8259Base(cd, Sachen8259Variant::Sachen8259B)
    {
    }
};
