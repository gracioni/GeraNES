#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper124 : public DeadEndMapper
{
public:
    Mapper124(ICartridgeData& cd)
        : DeadEndMapper(cd, 124)
    {
    }
};

} // namespace GeraNES
