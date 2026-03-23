#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper084 : public DeadEndMapper
{
public:
    Mapper084(ICartridgeData& cd)
        : DeadEndMapper(
            cd,
            84,
            "is a dead-end bad iNES assignment with unknown hardware behavior. Stub only."
        )
    {
    }
};
