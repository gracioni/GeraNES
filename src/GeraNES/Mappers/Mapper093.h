#pragma once

#include "BaseMapper.h"

class Mapper093 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    bool m_chrRamEnabled = false;

public:
    Mapper093(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        if(!hasChrRam()) {
            allocateChrRam(static_cast<int>(BankSize::B8K));
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        data &= readPrg(addr);
        m_prgBank = static_cast<uint8_t>((data >> 4) & 0x07) & m_prgMask;
        m_chrRamEnabled = (data & 0x01) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 14) & 0x01) {
        case 0: return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        default: return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(!m_chrRamEnabled) return 0xFF;
        return readChrRam<BankSize::B8K>(0, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(m_chrRamEnabled) writeChrRam<BankSize::B8K>(0, addr, data);
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrRamEnabled = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrRamEnabled);
    }
};
