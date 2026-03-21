#pragma once

#include "BaseMapper.h"

class Mapper031 : public BaseMapper
{
private:
    uint8_t m_prgBank[8] = {0, 0, 0, 0, 0, 0, 0, 0xFF};
    uint8_t m_prgMask = 0;

public:
    Mapper031(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B8K));
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B4K>());
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t data) override
    {
        if(addr >= 0x5FF8 && addr <= 0x5FFF) {
            m_prgBank[addr & 0x07] = data & m_prgMask;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 12) & 0x07);
        return cd().readPrg<BankSize::B4K>(m_prgBank[slot] & m_prgMask, addr);
    }

    void reset() override
    {
        for(uint8_t& bank : m_prgBank) bank = 0;
        m_prgBank[7] = m_prgMask;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_prgBank, 1, 8);
        SERIALIZEDATA(s, m_prgMask);
    }
};
