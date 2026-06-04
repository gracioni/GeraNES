#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper239 : public DeadEndMapper
{
public:
    Mapper239(ICartridgeData& cd)
        : DeadEndMapper(cd, 239)
    {
    }
};

} // namespace GeraNES
