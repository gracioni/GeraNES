#pragma once

#include "Mapper004.h"

// Waixing SH2
// iNES Mapper 245
class Mapper245 : public Mapper004 {

private:

    uint8_t m_outerPrgBank = 0;
    uint8_t m_innerPrgMask = 0;

    GERANES_INLINE uint8_t activeChrRegIndex(int addr) const
    {
        switch((addr >> 10) & 0x07) {
        case 0: return m_chrMode ? 2 : 0;
        case 1: return m_chrMode ? 3 : 0;
        case 2: return m_chrMode ? 4 : 1;
        case 3: return m_chrMode ? 5 : 1;
        case 4: return m_chrMode ? 0 : 2;
        case 5: return m_chrMode ? 0 : 3;
        case 6: return m_chrMode ? 1 : 4;
        case 7: return m_chrMode ? 1 : 5;
        default: return 0;
        }
    }

    GERANES_INLINE void updateOuterPrgBank(int chrAddr)
    {
        if(cd().numberOfPRGBanks<BankSize::B8K>() <= 64) {
            m_outerPrgBank = 0;
            return;
        }

        m_outerPrgBank = (m_chrReg[activeChrRegIndex(chrAddr)] >> 1) & 0x01;
    }

    GERANES_INLINE uint8_t mapPrgBank(uint8_t innerBank) const
    {
        const int totalPrgBanks = cd().numberOfPRGBanks<BankSize::B8K>();
        const int outerBase = (m_outerPrgBank & 0x01) ? 64 : 0;
        const int bank = outerBase + (innerBank & m_innerPrgMask);

        return static_cast<uint8_t>(bank % totalPrgBanks);
    }

public:

    Mapper245(ICartridgeData& cd) : Mapper004(cd)
    {
        const int visibleBanks = cd.numberOfPRGBanks<BankSize::B8K>() > 64 ? 64 : cd.numberOfPRGBanks<BankSize::B8K>();
        m_innerPrgMask = calculateMask(visibleBanks);

        // MMC3 inner PRG registers only control the low 6 bits on this board.
        m_prgMask = m_innerPrgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t fixedSecondLast = (m_innerPrgMask - 1) & m_innerPrgMask;
        const uint8_t fixedLast = m_innerPrgMask;

        if(!m_prgMode)
        {
            switch(addr >> 13) { // addr/8k
            case 0: return cd().readPrg<BankSize::B8K>(mapPrgBank(m_prgReg0), addr);
            case 1: return cd().readPrg<BankSize::B8K>(mapPrgBank(m_prgReg1), addr);
            case 2: return cd().readPrg<BankSize::B8K>(mapPrgBank(fixedSecondLast), addr);
            case 3: return cd().readPrg<BankSize::B8K>(mapPrgBank(fixedLast), addr);
            }
        }
        else
        {
            switch(addr >> 13) {
            case 0: return cd().readPrg<BankSize::B8K>(mapPrgBank(fixedSecondLast), addr);
            case 1: return cd().readPrg<BankSize::B8K>(mapPrgBank(m_prgReg1), addr);
            case 2: return cd().readPrg<BankSize::B8K>(mapPrgBank(m_prgReg0), addr);
            case 3: return cd().readPrg<BankSize::B8K>(mapPrgBank(fixedLast), addr);
            }
        }

        return 0;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        updateOuterPrgBank(addr);

        if(hasChrRam()) return readChrRam<BankSize::B8K>(0, addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        updateOuterPrgBank(addr);

        if(hasChrRam()) {
            writeChrRam<BankSize::B8K>(0, addr, data);
        }
    }

    void reset() override
    {
        Mapper004::reset();
        m_outerPrgBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_outerPrgBank);
        SERIALIZEDATA(s, m_innerPrgMask);
    }
};
