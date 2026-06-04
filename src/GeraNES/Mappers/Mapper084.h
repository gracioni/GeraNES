#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper084 : public DeadEndMapper
{
public:
    Mapper084(ICartridgeData& cd)
        : DeadEndMapper(cd, 84)
    {
    }
};

} // namespace GeraNES
