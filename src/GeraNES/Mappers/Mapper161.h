#pragma once

#include "Mapper001.h"

namespace GeraNES {

// Duplicate assignment of mapper 1 on some dumps.
class Mapper161 : public Mapper001
{
public:
    Mapper161(ICartridgeData& cd) : Mapper001(cd)
    {
    }
};

} // namespace GeraNES
