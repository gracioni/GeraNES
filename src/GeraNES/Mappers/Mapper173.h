#pragma once

#include "Mapper132.h"

class Mapper173 : public Mapper132
{
public:
    Mapper173(ICartridgeData& cd) : Mapper132(cd)
    {
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        if(cd().numberOfCHRBanks<BankSize::B8K>() > 1) {
            const uint8_t bank = static_cast<uint8_t>((m_txc.output() & 0x01) | (m_txc.yFlag() ? 0x02 : 0x00) | ((m_txc.output() & 0x02) << 1));
            return cd().readChr<BankSize::B8K>(bank, addr);
        }

        return m_txc.yFlag() ? cd().readChr<BankSize::B8K>(0, addr) : 0;
    }
};
