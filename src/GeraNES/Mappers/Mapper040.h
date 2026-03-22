#pragma once

#include "BaseMapper.h"

class Mapper040 : public BaseMapper
{
private:
    uint8_t m_bank2 = 6;
    uint8_t m_prgMask = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqFlag = false;

public:
    Mapper040(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch((addr + 0x8000) & 0xE000) {
        case 0x8000:
            m_irqCounter = 0;
            m_irqFlag = false;
            break;
        case 0xA000:
            m_irqCounter = 4096;
            m_irqFlag = false;
            break;
        case 0xE000:
            m_bank2 = data & m_prgMask;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(4 & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(5 & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_bank2 & m_prgMask, addr);
        default: return cd().readPrg<BankSize::B8K>(7 & m_prgMask, addr);
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(6 & m_prgMask, addr);
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t /*data*/) override
    {
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT void cycle() override
    {
        if(m_irqCounter > 0) {
            --m_irqCounter;
            if(m_irqCounter == 0) {
                m_irqFlag = true;
            }
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    void reset() override
    {
        m_bank2 = 6;
        m_irqCounter = 0;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bank2);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqFlag);
    }
};
