#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper130 : public DeadEndMapper
{
public:
    Mapper130(ICartridgeData& cd)
        : DeadEndMapper(cd, 130)
    {
    }
};

} // namespace GeraNES
