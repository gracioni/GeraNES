#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper176 : public DeadEndMapper
{
public:
    Mapper176(ICartridgeData& cd)
        : DeadEndMapper(cd, 176, "8025/FK23C family stub. NESdev documents multiple incompatible submappers and Mesen 2 uses a dedicated Fk23C implementation not yet ported here.")
    {
    }
};
