#pragma once

#include "Mapper088.h"

// iNES Mapper 154 (NAMCOT-3453)
// Same as mapper 88, plus mapper-controlled one-screen mirroring:
// any write in $8000-$FFFF uses bit 6:
//   0 = 1ScA, 1 = 1ScB
class Mapper154 : public Mapper088
{
private:
    bool m_mirroring = false; // 0=1ScA, 1=1ScB

public:
    Mapper154(ICartridgeData& cd) : Mapper088(cd) {}

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        // Mapper 154 mirroring bit is visible on any write in $8000-$FFFF.
        m_mirroring = (data & 0x40) != 0;
        Mapper088::writePrg(addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_mirroring ? MirroringType::SINGLE_SCREEN_B : MirroringType::SINGLE_SCREEN_A;
    }

    void reset() override
    {
        Mapper088::reset();
        m_mirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper088::serialization(s);
        SERIALIZEDATA(s, m_mirroring);
    }
};
