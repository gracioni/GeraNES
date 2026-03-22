#pragma once

#include "BaseMapper.h"

class Mapper106 : public BaseMapper
{
private:
    uint8_t m_prgReg[4] = {0, 0, 0, 0};
    uint8_t m_chrReg[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_irqFlag = false;

public:
    Mapper106(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        switch((addr + 0x8000) & 0x0F) {
        case 0:
        case 2: m_chrReg[(addr + 0x8000) & 0x0F] = static_cast<uint8_t>(value & 0xFE); break;
        case 1:
        case 3: m_chrReg[(addr + 0x8000) & 0x0F] = static_cast<uint8_t>(value | 0x01); break;
        case 4:
        case 5:
        case 6:
        case 7: m_chrReg[(addr + 0x8000) & 0x0F] = value; break;
        case 8:
        case 0x0B: m_prgReg[((addr + 0x8000) & 0x0F) - 8] = static_cast<uint8_t>((value & 0x0F) | 0x10); break;
        case 9:
        case 0x0A: m_prgReg[((addr + 0x8000) & 0x0F) - 8] = static_cast<uint8_t>(value & 0x1F); break;
        case 0x0D:
            m_irqEnabled = false;
            m_irqCounter = 0;
            m_irqFlag = false;
            break;
        case 0x0E:
            m_irqCounter = static_cast<uint16_t>((m_irqCounter & 0xFF00) | value);
            break;
        case 0x0F:
            m_irqCounter = static_cast<uint16_t>((m_irqCounter & 0x00FF) | (value << 8));
            m_irqEnabled = true;
            m_irqFlag = false;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgReg[(addr >> 13) & 0x03] & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B1K>(m_chrReg[(addr >> 10) & 0x07] & m_chrMask, addr);
        return cd().readChr<BankSize::B1K>(m_chrReg[(addr >> 10) & 0x07] & m_chrMask, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B1K>(m_chrReg[(addr >> 10) & 0x07] & m_chrMask, addr, data);
    }

    GERANES_HOT void cycle() override
    {
        if(m_irqEnabled) {
            ++m_irqCounter;
            if(m_irqCounter == 0) {
                m_irqFlag = true;
                m_irqEnabled = false;
            }
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    void reset() override
    {
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_irqFlag = false;
        for(int i = 0; i < 4; ++i) m_prgReg[i] = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);
        for(int i = 0; i < 8; ++i) m_chrReg[i] = static_cast<uint8_t>(i & m_chrMask);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_prgReg, 1, 4);
        s.array(m_chrReg, 1, 8);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqFlag);
    }
};
