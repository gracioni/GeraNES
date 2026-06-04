#pragma once

#include "Mapper090.h"

namespace GeraNES {

class Mapper211 : public Mapper090
{
public:
    Mapper211(ICartridgeData& cd) : Mapper090(cd)
    {
    }
};

} // namespace GeraNES
