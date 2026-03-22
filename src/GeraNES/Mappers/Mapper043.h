#pragma once

#include "BaseMapper.h"

class Mapper043 : public BaseMapper
{
private:
    uint8_t m_reg = 0;
    bool m_swap = false;
    uint8_t m_prgMask = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_irqFlag = false;

    void updateState()
    {
    }

public:
    Mapper043(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(1 & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(0 & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_reg & m_prgMask, addr);
        default: return cd().readPrg<BankSize::B8K>((m_swap ? 8 : 9) & m_prgMask, addr);
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>((m_swap ? 0 : 2) & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if(addr >= 0x1000) {
            return cd().readPrg<BankSize::B8K>(8 & m_prgMask, addr & 0x0FFF);
        }
        return openBusData;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        switch(addr & 0xF1FF) {
        case 0x0022: {
            static const uint8_t lut[8] = {4, 3, 5, 3, 6, 3, 7, 3};
            m_reg = lut[data & 0x07];
            break;
        }
        case 0x0120:
            m_swap = (data & 0x01) != 0;
            break;
        case 0x0122:
            m_irqEnabled = (data & 0x01) != 0;
            m_irqCounter = 0;
            m_irqFlag = false;
            break;
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(((addr + 0x8000) & 0xF1FF) == 0x8122) {
            m_irqEnabled = (data & 0x01) != 0;
            m_irqCounter = 0;
            m_irqFlag = false;
        }
    }

    GERANES_HOT void cycle() override
    {
        if(m_irqEnabled) {
            ++m_irqCounter;
            if(m_irqCounter >= 4096) {
                m_irqEnabled = false;
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
        m_reg = 0;
        m_swap = false;
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_reg);
        SERIALIZEDATA(s, m_swap);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqFlag);
    }
};
