#pragma once

#include "Mapper205.h"

namespace GeraNES {

// Duplicate assignment of mapper 205 on some dumps.
class Mapper131 : public Mapper205
{
public:
    Mapper131(ICartridgeData& cd) : Mapper205(cd)
    {
    }
};

} // namespace GeraNES
