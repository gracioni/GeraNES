#pragma once

#include "Mapper001.h"

namespace GeraNES {

class Mapper155 : public Mapper001
{
public:
    Mapper155(ICartridgeData& cd) : Mapper001(cd)
    {
    }
};

} // namespace GeraNES
