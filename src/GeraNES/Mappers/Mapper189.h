#pragma once

#include "Mapper004.h"

// iNES Mapper 189
// MMC3-compatible CHR/IRQ/mirroring logic, with an outer PRG register at $4120-$7FFF
// selecting a 32KB block:
//   [AAAA BBBB] -> ((reg | (reg >> 4)) & 0x07) * 4  (in 8KB pages)
class Mapper189 : public Mapper004
{
private:
    uint8_t m_outerPrgReg = 0;

    GERANES_INLINE uint8_t outerPrgBase8k() const
    {
        return static_cast<uint8_t>((((m_outerPrgReg | (m_outerPrgReg >> 4)) & 0x07) * 4) & m_prgMask);
    }

    GERANES_INLINE void writeOuterPrgReg(uint8_t value)
    {
        m_outerPrgReg = value;
    }

public:
    Mapper189(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t base = outerPrgBase8k();
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(base + slot) & m_prgMask, addr);
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        if(addr >= 0x0120) {
            writeOuterPrgReg(data);
        }
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        writeOuterPrgReg(data);
    }

    void reset() override
    {
        Mapper004::reset();
        m_outerPrgReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_outerPrgReg);
    }
};
