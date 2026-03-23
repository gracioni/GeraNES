#pragma once

#include "BaseMapper.h"

// iNES Mapper 117
class Mapper117 : public BaseMapper
{
private:
    uint8_t m_prgReg[4] = {0xFC, 0xFD, 0xFE, 0xFF};
    uint8_t m_chrReg[8] = {0};
    uint8_t m_irqCounter = 0;
    uint8_t m_irqReloadValue = 0;
    bool m_irqEnabled = false;
    bool m_irqEnabledAlt = false;
    bool m_interruptFlag = false;
    bool m_horizontalMirroring = false;
    bool m_a12LastState = true;
    uint8_t m_cycleCounter = 0;

public:
    Mapper117(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute >= 0x8000 && absolute <= 0x8003) {
            m_prgReg[absolute & 0x03] = value;
            return;
        }

        if(absolute >= 0xA000 && absolute <= 0xA007) {
            m_chrReg[absolute & 0x07] = value;
            return;
        }

        switch(absolute) {
        case 0xC001:
            m_irqReloadValue = value;
            break;
        case 0xC002:
            m_interruptFlag = false;
            break;
        case 0xC003:
            m_irqCounter = m_irqReloadValue;
            m_irqEnabledAlt = true;
            break;
        case 0xD000:
            m_horizontalMirroring = (value & 0x01) != 0;
            break;
        case 0xE000:
            m_irqEnabled = (value & 0x01) != 0;
            m_interruptFlag = false;
            break;
        default:
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgReg[(addr >> 13) & 0x03], addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t bank = m_chrReg[(addr >> 10) & 0x07];
        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t bank = m_chrReg[(addr >> 10) & 0x07];
        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    void setA12State(bool state) override
    {
        if(!m_a12LastState && state) {
            if(m_cycleCounter > 3 && m_irqEnabled && m_irqEnabledAlt && m_irqCounter != 0) {
                --m_irqCounter;
                if(m_irqCounter == 0) {
                    m_interruptFlag = true;
                    m_irqEnabledAlt = false;
                }
            }
        } else if(m_a12LastState && !state) {
            m_cycleCounter = 0;
        }

        m_a12LastState = state;
    }

    void cycle() override
    {
        if(static_cast<uint8_t>(m_cycleCounter + 1) != 0) {
            ++m_cycleCounter;
        }
    }

    void reset() override
    {
        m_prgReg[0] = 0xFC;
        m_prgReg[1] = 0xFD;
        m_prgReg[2] = 0xFE;
        m_prgReg[3] = 0xFF;
        memset(m_chrReg, 0, sizeof(m_chrReg));
        m_irqCounter = 0;
        m_irqReloadValue = 0;
        m_irqEnabled = false;
        m_irqEnabledAlt = false;
        m_interruptFlag = false;
        m_horizontalMirroring = false;
        m_a12LastState = true;
        m_cycleCounter = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_prgReg, 1, 4);
        s.array(m_chrReg, 1, 8);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqReloadValue);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqEnabledAlt);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_horizontalMirroring);
        SERIALIZEDATA(s, m_a12LastState);
        SERIALIZEDATA(s, m_cycleCounter);
    }
};
