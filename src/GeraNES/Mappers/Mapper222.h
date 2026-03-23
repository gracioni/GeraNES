#pragma once

#include "BaseMapper.h"

// iNES Mapper 222
class Mapper222 : public BaseMapper
{
private:
    uint8_t m_prgReg[2] = {0};
    uint8_t m_chrReg[8] = {0};
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    bool m_horizontalMirroring = false;

    uint16_t m_irqCounter = 0;
    bool m_interruptFlag = false;
    bool m_a12LastState = true;
    uint8_t m_cycleCounter = 0;

public:
    Mapper222(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        switch(absolute & 0xF003) {
        case 0x8000: m_prgReg[0] = data & m_prgMask; break;
        case 0x9000: m_horizontalMirroring = (data & 0x01) != 0; break;
        case 0xA000: m_prgReg[1] = data & m_prgMask; break;
        case 0xB000: m_chrReg[0] = data & m_chrMask; break;
        case 0xB002: m_chrReg[1] = data & m_chrMask; break;
        case 0xC000: m_chrReg[2] = data & m_chrMask; break;
        case 0xC002: m_chrReg[3] = data & m_chrMask; break;
        case 0xD000: m_chrReg[4] = data & m_chrMask; break;
        case 0xD002: m_chrReg[5] = data & m_chrMask; break;
        case 0xE000: m_chrReg[6] = data & m_chrMask; break;
        case 0xE002: m_chrReg[7] = data & m_chrMask; break;
        case 0xF000:
            m_irqCounter = data;
            m_interruptFlag = false;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg[0], addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1], addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B1K>(m_chrReg[(addr >> 10) & 0x07], addr);
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
            if(m_cycleCounter > 3 && m_irqCounter != 0) {
                ++m_irqCounter;
                if(m_irqCounter >= 240) {
                    m_interruptFlag = true;
                    m_irqCounter = 0;
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
        memset(m_prgReg, 0, sizeof(m_prgReg));
        memset(m_chrReg, 0, sizeof(m_chrReg));
        m_horizontalMirroring = false;
        m_irqCounter = 0;
        m_interruptFlag = false;
        m_a12LastState = true;
        m_cycleCounter = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_prgReg, 1, 2);
        s.array(m_chrReg, 1, 8);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_horizontalMirroring);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_a12LastState);
        SERIALIZEDATA(s, m_cycleCounter);
    }
};
