#pragma once

#include "BaseMapper.h"

// Nintendo Vs. System (iNES Mapper 99)
class Mapper099 : public BaseMapper
{
private:
    bool m_bankSelect = false; // OUT2 ($4016 bit 2)

public:
    Mapper099(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        // $8000-$9FFF: switchable between bank 0 and bank 4 using OUT2
        // $A000-$FFFF: fixed to banks 1,2,3.
        switch((addr >> 13) & 0x03) {
        case 0:
        {
            const uint8_t bank = static_cast<uint8_t>(m_bankSelect ? 4 : 0);
            const uint8_t mask = calculateMask(cd().numberOfPRGBanks<BankSize::B8K>());
            return cd().readPrg<BankSize::B8K>(bank & mask, addr);
        }
        case 1: return cd().readPrg<BankSize::B8K>(1, addr);
        case 2: return cd().readPrg<BankSize::B8K>(2, addr);
        default: return cd().readPrg<BankSize::B8K>(3, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t bank = static_cast<uint8_t>(m_bankSelect ? 1 : 0);
        if(hasChrRam()) return readChrRam<BankSize::B8K>(bank, addr);
        return cd().readChr<BankSize::B8K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t bank = static_cast<uint8_t>(m_bankSelect ? 1 : 0);
        writeChrRam<BankSize::B8K>(bank, addr, data);
    }

    GERANES_HOT void onCpuWrite(uint16_t addr, uint8_t data) override
    {
        if(addr == 0x4016) {
            m_bankSelect = (data & 0x04) != 0;
        }
    }

    void reset() override
    {
        m_bankSelect = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bankSelect);
    }
};
