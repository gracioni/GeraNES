#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper098 : public DeadEndMapper
{
public:
    Mapper098(ICartridgeData& cd)
        : DeadEndMapper(cd, 98)
    {
    }
};

} // namespace GeraNES
