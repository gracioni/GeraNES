#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper239 : public DeadEndMapper
{
public:
    Mapper239(ICartridgeData& cd)
        : DeadEndMapper(cd, 239, "undocumented mapper stub. No reliable NESdev or Mesen 2 implementation found.")
    {
    }
};
