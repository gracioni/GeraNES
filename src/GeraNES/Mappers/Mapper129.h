#pragma once

#include "Helpers/DeadEndMapper.h"

class Mapper129 : public DeadEndMapper
{
public:
    Mapper129(ICartridgeData& cd)
        : DeadEndMapper(cd, 129, "undocumented mapper stub. No reliable NESdev or Mesen 2 implementation found.")
    {
    }
};
