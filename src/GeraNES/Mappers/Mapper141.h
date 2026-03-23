#pragma once

#include "Helpers/Sachen8259Base.h"

class Mapper141 : public Sachen8259Base
{
public:
    Mapper141(ICartridgeData& cd) : Sachen8259Base(cd, Sachen8259Variant::Sachen8259A)
    {
    }
};
