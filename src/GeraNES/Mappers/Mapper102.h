#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper102 : public DeadEndMapper
{
public:
    Mapper102(ICartridgeData& cd)
        : DeadEndMapper(cd, 102)
    {
    }
};

} // namespace GeraNES
