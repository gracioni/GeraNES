#pragma once

#include "Mapper166.h"

class Mapper167 : public Mapper166
{
protected:
    GERANES_INLINE bool altMode() const override
    {
        return true;
    }

public:
    Mapper167(ICartridgeData& cd) : Mapper166(cd)
    {
    }
};
