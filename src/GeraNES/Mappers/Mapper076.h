#pragma once

#include "BaseMapper.h"

class Mapper076 : public BaseMapper
{
private:
    uint8_t m_bankSelect = 0;
    uint8_t m_chrReg[4] = {0};
    uint8_t m_prgReg0 = 0;
    uint8_t m_prgReg1 = 1;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr)
    {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data)
    {
        writeChrRam<bs>(bank, addr, data);
    }

public:
    Mapper076(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B2K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch(addr & 0x0001) {
        case 0x0000:
            m_bankSelect = data & 0x07;
            break;
        case 0x0001:
            switch(m_bankSelect) {
            case 2: m_chrReg[0] = data & m_chrMask; break;
            case 3: m_chrReg[1] = data & m_chrMask; break;
            case 4: m_chrReg[2] = data & m_chrMask; break;
            case 5: m_chrReg[3] = data & m_chrMask; break;
            case 6: m_prgReg0 = static_cast<uint8_t>(data & 0x3F) & m_prgMask; break;
            case 7: m_prgReg1 = static_cast<uint8_t>(data & 0x3F) & m_prgMask; break;
            }
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg0, addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg1, addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return readChrBank<BankSize::B2K>(m_chrReg[(addr >> 11) & 0x03], addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrBank<BankSize::B2K>(m_chrReg[(addr >> 11) & 0x03], addr, data);
    }

    void reset() override
    {
        m_bankSelect = 0;
        m_chrReg[0] = 0;
        m_chrReg[1] = 0;
        m_chrReg[2] = 0;
        m_chrReg[3] = 0;
        m_prgReg0 = 0;
        m_prgReg1 = 1;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bankSelect);
        s.array(m_chrReg, 1, 4);
        SERIALIZEDATA(s, m_prgReg0);
        SERIALIZEDATA(s, m_prgReg1);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
    }
};
