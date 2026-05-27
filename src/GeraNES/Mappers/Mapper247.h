#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper247 : public DeadEndMapper
{
public:
    Mapper247(ICartridgeData& cd)
        : DeadEndMapper(cd, 247)
    {
    }
};
