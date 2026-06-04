#pragma once

#include "Mapper202.h"

namespace GeraNES {

class Mapper056 : public Mapper202
{
public:
    Mapper056(ICartridgeData& cd) : Mapper202(cd)
    {
    }
};

} // namespace GeraNES
