#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper127 : public DeadEndMapper
{
public:
    Mapper127(ICartridgeData& cd)
        : DeadEndMapper(cd, 127)
    {
    }
};
