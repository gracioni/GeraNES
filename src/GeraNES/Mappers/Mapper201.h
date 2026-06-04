#pragma once

#include "Mapper054.h"

namespace GeraNES {

class Mapper201 : public Mapper054
{
public:
    Mapper201(ICartridgeData& cd) : Mapper054(cd)
    {
    }
};

} // namespace GeraNES
