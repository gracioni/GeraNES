#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper186 : public DeadEndMapper
{
public:
    Mapper186(ICartridgeData& cd)
        : DeadEndMapper(cd, 186)
    {
    }
};

} // namespace GeraNES
