#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper128 : public DeadEndMapper
{
public:
    Mapper128(ICartridgeData& cd)
        : DeadEndMapper(cd, 128)
    {
    }
};

} // namespace GeraNES
