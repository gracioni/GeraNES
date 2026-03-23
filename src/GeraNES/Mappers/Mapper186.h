#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper186 : public DeadEndMapper
{
public:
    Mapper186(ICartridgeData& cd)
        : DeadEndMapper(
            cd,
            186,
            "is a dead-end bad iNES assignment for the Fukutake Study Box BIOS. Stub only."
        )
    {
    }
};
