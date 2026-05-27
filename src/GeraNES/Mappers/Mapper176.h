#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper176 : public DeadEndMapper
{
public:
    Mapper176(ICartridgeData& cd)
        : DeadEndMapper(cd, 176)
    {
    }
};
