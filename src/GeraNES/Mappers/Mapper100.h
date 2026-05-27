#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper100 : public DeadEndMapper
{
public:
    Mapper100(ICartridgeData& cd)
        : DeadEndMapper(cd, 100)
    {
    }
};
