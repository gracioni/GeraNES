#pragma once

#include "Mapper016.h"

// Datach Joint ROM System.
// Base Bandai LZ93D50 behavior is shared with mapper 16; barcode input is not yet modeled here.
class Mapper157 : public Mapper016
{
public:
    Mapper157(ICartridgeData& cd) : Mapper016(cd)
    {
    }
};
