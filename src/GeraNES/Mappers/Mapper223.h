#pragma once

#include "Mapper199.h"

namespace GeraNES {

// Duplicate assignment of mapper 199 on some dumps.
class Mapper223 : public Mapper199
{
public:
    Mapper223(ICartridgeData& cd) : Mapper199(cd)
    {
    }
};

} // namespace GeraNES
