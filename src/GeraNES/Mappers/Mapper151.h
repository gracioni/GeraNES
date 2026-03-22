#pragma once

#include "Mapper075.h"

// iNES mapper 151 is the historical "Vs. System VRC1" assignment.
// Per NESdev, this is effectively mapper 75 with the normal Vs./4-screen
// nametable wiring still taking precedence over the VRC1 mirroring bit.
// Mapper075 already preserves 4-screen mirroring via useFourScreenMirroring(),
// so this mapper intentionally remains a thin wrapper.
class Mapper151 : public Mapper075
{
public:
    Mapper151(ICartridgeData& cd) : Mapper075(cd)
    {
    }
};
