#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper098 : public DeadEndMapper
{
public:
    Mapper098(ICartridgeData& cd)
        : DeadEndMapper(cd, 98)
    {
    }
};
