#pragma once

#include "Helpers/DeadEndMapper.h"

namespace GeraNES {

class Mapper129 : public DeadEndMapper
{
public:
    Mapper129(ICartridgeData& cd)
        : DeadEndMapper(cd, 129)
    {
    }
};

} // namespace GeraNES
