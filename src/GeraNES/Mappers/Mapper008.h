#pragma once

#include "Mapper006.h"

// FFE F3xxx (Mapper 8)
// iNES mapper 8 is equivalent to mapper 6 submapper 4
// (GNROM latch mode with CHR write-protected behavior).
class Mapper008 : public Mapper006
{
public:
    Mapper008(ICartridgeData& cd) : Mapper006(cd, true)
    {
    }
};
