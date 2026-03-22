#pragma once

#include "BaseMapper.h"

class Mapper073 : public BaseMapper
{
private:
    bool m_irqEnableOnAck = false;
    bool m_irqEnabled = false;
    bool m_smallCounter = false;
    uint16_t m_irqReload = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqFlag = false;
    uint8_t m_prgMask = 0;

public:
    Mapper073(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        switch((addr + 0x8000) & 0xF000) {
        case 0x8000: m_irqReload = static_cast<uint16_t>((m_irqReload & 0xFFF0) | (value & 0x0F)); break;
        case 0x9000: m_irqReload = static_cast<uint16_t>((m_irqReload & 0xFF0F) | ((value & 0x0F) << 4)); break;
        case 0xA000: m_irqReload = static_cast<uint16_t>((m_irqReload & 0xF0FF) | ((value & 0x0F) << 8)); break;
        case 0xB000: m_irqReload = static_cast<uint16_t>((m_irqReload & 0x0FFF) | ((value & 0x0F) << 12)); break;
        case 0xC000:
            m_irqEnabled = (value & 0x02) != 0;
            if(m_irqEnabled) m_irqCounter = m_irqReload;
            m_smallCounter = (value & 0x04) != 0;
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqFlag = false;
            break;
        case 0xD000:
            m_irqFlag = false;
            m_irqEnabled = m_irqEnableOnAck;
            break;
        case 0xF000:
            m_irqFlag = false;
            break;
        }

        if(((addr + 0x8000) & 0xF000) == 0xF000) {
            // Bank select shares $Fxxx writes on VRC3.
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) {
            return cd().readPrg<BankSize::B16K>(0, addr);
        }
        return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>(m_irqReload & 0x07) & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT void cycle() override
    {
        if(!m_irqEnabled) return;

        if(m_smallCounter) {
            uint8_t small = static_cast<uint8_t>(m_irqCounter & 0xFF);
            ++small;
            if(small == 0) {
                small = static_cast<uint8_t>(m_irqReload & 0xFF);
                m_irqFlag = true;
            }
            m_irqCounter = static_cast<uint16_t>((m_irqCounter & 0xFF00) | small);
        } else {
            ++m_irqCounter;
            if(m_irqCounter == 0) {
                m_irqCounter = m_irqReload;
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
        m_irqEnableOnAck = false;
        m_irqEnabled = false;
        m_smallCounter = false;
        m_irqReload = 0;
        m_irqCounter = 0;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_irqEnableOnAck);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_smallCounter);
        SERIALIZEDATA(s, m_irqReload);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqFlag);
        SERIALIZEDATA(s, m_prgMask);
    }
};
