#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper128 : public DeadEndMapper
{
public:
    Mapper128(ICartridgeData& cd)
        : DeadEndMapper(cd, 128, "undocumented mapper stub. No reliable NESdev or Mesen 2 implementation found.")
    {
    }
};
