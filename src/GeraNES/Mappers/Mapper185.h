#pragma once

#include "BaseMapper.h"

// CNROM with CHR-ROM disable protection
class Mapper185 : public BaseMapper
{
private:

    uint8_t m_chrReg = 0;
    uint8_t m_chrRegRaw = 0;
    uint8_t m_chrRegMask = 0;
    uint8_t m_subMapper = 0;
    uint8_t m_initialReadBlockCount = 2; // submapper 0 heuristic
    bool m_blockNextChrRead = false;

    GERANES_INLINE bool chrEnabled() const
    {
        switch(m_subMapper)
        {
        case 4: return (m_chrRegRaw & 0x03) == 0;
        case 5: return (m_chrRegRaw & 0x03) == 1;
        case 6: return (m_chrRegRaw & 0x03) == 2;
        case 7: return (m_chrRegRaw & 0x03) == 3;
        default: return true;
        }
    }

public:

    Mapper185(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_chrRegMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
        m_subMapper = static_cast<uint8_t>(cd.subMapperId() & 0x0F);
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        // Mapper 185 always has AND-type bus conflicts.
        data &= readPrg(addr);
        m_chrRegRaw = data;
        m_chrReg = data & m_chrRegMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(0, addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() == 2 ? 1 : 0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        if(m_blockNextChrRead) {
            m_blockNextChrRead = false;
            if(m_initialReadBlockCount > 0) m_initialReadBlockCount--;
            return 0xFF;
        }

        if(!chrEnabled()) {
            return 0xFF;
        }

        return cd().readChr<BankSize::B8K>(m_chrReg, addr);
    }

    GERANES_HOT void onCpuRead(uint16_t addr) override
    {
        // NES 2.0 Mapper 185 submapper 0 heuristic:
        // block CHR on first 2 CPU reads from $2007 after reset.
        if(m_subMapper == 0 && m_initialReadBlockCount > 0 && (addr & 0x2007) == 0x2007) {
            m_blockNextChrRead = true;
        }
    }

    void reset() override
    {
        m_chrReg = 0;
        m_chrRegRaw = 0;
        m_initialReadBlockCount = 2;
        m_blockNextChrRead = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrReg);
        SERIALIZEDATA(s, m_chrRegRaw);
        SERIALIZEDATA(s, m_chrRegMask);
        SERIALIZEDATA(s, m_subMapper);
        SERIALIZEDATA(s, m_initialReadBlockCount);
        SERIALIZEDATA(s, m_blockNextChrRead);
    }
};
