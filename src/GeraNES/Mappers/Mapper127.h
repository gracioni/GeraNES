#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper127 : public DeadEndMapper
{
public:
    Mapper127(ICartridgeData& cd)
        : DeadEndMapper(cd, 127, "specific pirate Double Dragon II assignment with no usable public hardware documentation; stub only.")
    {
    }
};
