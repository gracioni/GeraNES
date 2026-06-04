#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper100 : public DeadEndMapper
{
public:
    Mapper100(ICartridgeData& cd)
        : DeadEndMapper(cd, 100)
    {
    }
};

} // namespace GeraNES
