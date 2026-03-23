#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper124 : public DeadEndMapper
{
public:
    Mapper124(ICartridgeData& cd)
        : DeadEndMapper(cd, 124, "undocumented mapper stub. No reliable NESdev or Mesen 2 implementation found.")
    {
    }
};
