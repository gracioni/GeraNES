#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper130 : public DeadEndMapper
{
public:
    Mapper130(ICartridgeData& cd)
        : DeadEndMapper(cd, 130, "undocumented mapper stub. No reliable NESdev or Mesen 2 implementation found.")
    {
    }
};
