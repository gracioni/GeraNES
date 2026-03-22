#pragma once

#include "BaseMapper.h"

class Mapper050 : public BaseMapper
{
private:
    uint8_t m_prgBank2 = 0x0A;
    uint8_t m_prgMask = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_irqFlag = false;

public:
    Mapper050(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(0x08 & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(0x09 & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_prgBank2 & m_prgMask, addr);
        default: return cd().readPrg<BankSize::B8K>(0x0B & m_prgMask, addr);
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(0x0F & m_prgMask, addr);
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        switch(addr & 0x0120) {
        case 0x0020:
            m_prgBank2 = static_cast<uint8_t>((data & 0x08) | ((data & 0x01) << 2) | ((data & 0x06) >> 1));
            break;
        case 0x0120:
            if(data & 0x01) {
                m_irqEnabled = true;
                m_irqFlag = false;
            }
            else {
                m_irqEnabled = false;
                m_irqCounter = 0;
                m_irqFlag = false;
            }
            break;
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT void cycle() override
    {
        if(m_irqEnabled) {
            ++m_irqCounter;
            if(m_irqCounter == 0x1000) {
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
        m_prgBank2 = 0x0A;
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank2);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqFlag);
    }
};
