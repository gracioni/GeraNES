#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper186 : public DeadEndMapper
{
public:
    Mapper186(ICartridgeData& cd)
        : DeadEndMapper(cd, 186)
    {
    }
};
