#pragma once

#include "BaseMapper.h"

// iNES Mapper 105
class Mapper105 : public BaseMapper
{
private:
    int m_shiftCounter = 0;
    uint8_t m_shiftRegister = 0;
    uint8_t m_control = 0x0C;
    uint8_t m_chrBank0 = 0;
    uint8_t m_chrBank1 = 0;
    uint8_t m_prgBank = 0;

    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

    uint8_t m_initState = 0;
    uint32_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_interruptFlag = false;

    GERANES_INLINE void updateState()
    {
        if(m_initState == 0 && (m_chrBank0 & 0x10) == 0x00) {
            m_initState = 1;
        } else if(m_initState == 1 && (m_chrBank0 & 0x10) != 0) {
            m_initState = 2;
        }

        if((m_chrBank0 & 0x10) != 0) {
            m_irqEnabled = false;
            m_irqCounter = 0;
            m_interruptFlag = false;
        } else {
            m_irqEnabled = true;
        }
    }

public:
    Mapper105(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B4K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(data & 0x80) {
            m_shiftCounter = 0;
            m_shiftRegister = 0;
            m_control |= 0x0C;
            updateState();
            return;
        }

        m_shiftRegister |= static_cast<uint8_t>((data & 0x01) << m_shiftCounter);
        ++m_shiftCounter;

        if(m_shiftCounter == 5) {
            if(addr < 0x2000) {
                m_control = m_shiftRegister & 0x1F;
            } else if(addr < 0x4000) {
                m_chrBank0 = m_shiftRegister & 0x1F;
            } else if(addr < 0x6000) {
                m_chrBank1 = m_shiftRegister & m_chrMask;
            } else {
                m_prgBank = m_shiftRegister & 0x1F;
            }

            m_shiftCounter = 0;
            m_shiftRegister = 0;
            updateState();
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_initState != 2) {
            return cd().readPrg<BankSize::B32K>(0, addr);
        }

        if((m_chrBank0 & 0x08) != 0) {
            const uint8_t prgReg = static_cast<uint8_t>((m_prgBank & 0x07) | 0x08);
            if((m_control & 0x08) != 0) {
                if((m_control & 0x04) != 0) {
                    if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(prgReg & m_prgMask, addr);
                    return cd().readPrg<BankSize::B16K>(0x0F & m_prgMask, addr);
                }

                if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(0x08 & m_prgMask, addr);
                return cd().readPrg<BankSize::B16K>(prgReg & m_prgMask, addr);
            }

            return cd().readPrg<BankSize::B32K>(static_cast<uint8_t>((prgReg & 0x0E) >> 1), addr);
        }

        return cd().readPrg<BankSize::B32K>(static_cast<uint8_t>((m_chrBank0 & 0x06) >> 1), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        if((m_control & 0x10) == 0) {
            return cd().readChr<BankSize::B8K>(static_cast<uint8_t>(m_chrBank0 >> 1), addr);
        }

        if(addr < 0x1000) return cd().readChr<BankSize::B4K>(m_chrBank0 & m_chrMask, addr);
        return cd().readChr<BankSize::B4K>(m_chrBank1 & m_chrMask, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_control & 0x03) {
        case 0: return MirroringType::SINGLE_SCREEN_A;
        case 1: return MirroringType::SINGLE_SCREEN_B;
        case 2: return MirroringType::VERTICAL;
        default: return MirroringType::HORIZONTAL;
        }
    }

    GERANES_HOT void cycle() override
    {
        if(!m_irqEnabled) return;

        ++m_irqCounter;
        const uint32_t maxCounter = 0x20000000;
        if(m_irqCounter >= maxCounter) {
            m_interruptFlag = true;
            m_irqEnabled = false;
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    void reset() override
    {
        m_shiftCounter = 0;
        m_shiftRegister = 0;
        m_control = 0x0C;
        m_chrBank0 = 0x10;
        m_chrBank1 = 0;
        m_prgBank = 0;
        m_initState = 0;
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_interruptFlag = false;
        updateState();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_shiftCounter);
        SERIALIZEDATA(s, m_shiftRegister);
        SERIALIZEDATA(s, m_control);
        SERIALIZEDATA(s, m_chrBank0);
        SERIALIZEDATA(s, m_chrBank1);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_initState);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_interruptFlag);
    }
};
