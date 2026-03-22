#pragma once

#include "BaseMapper.h"

class Mapper035 : public BaseMapper
{
private:
    uint8_t m_prgBank[4] = {0, 1, 2, 3};
    uint8_t m_chrBank[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

    uint8_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_irqFlag = false;
    bool m_horizontalMirroring = false;

    bool m_a12LastState = true;
    uint8_t m_cycleCounter = 0;

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr)
    {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data)
    {
        if(hasChrRam()) writeChrRam<bs>(bank, addr, data);
    }

    GERANES_INLINE void clockIrq()
    {
        if(!m_irqEnabled) return;

        --m_irqCounter;
        if(m_irqCounter == 0) {
            m_irqEnabled = false;
            m_irqFlag = true;
        }
    }

public:
    Mapper035(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch((addr + 0x8000) & 0xF007) {
        case 0x8000:
        case 0x8001:
        case 0x8002:
        case 0x8003:
            m_prgBank[addr & 0x03] = data & m_prgMask;
            break;

        case 0x9000:
        case 0x9001:
        case 0x9002:
        case 0x9003:
        case 0x9004:
        case 0x9005:
        case 0x9006:
        case 0x9007:
            m_chrBank[addr & 0x07] = data & m_chrMask;
            break;

        case 0xC002:
            m_irqEnabled = false;
            m_irqFlag = false;
            break;

        case 0xC003:
            m_irqEnabled = true;
            m_irqFlag = false;
            break;

        case 0xC005:
            m_irqCounter = data;
            m_irqFlag = false;
            break;

        case 0xD001:
            m_horizontalMirroring = (data & 0x01) != 0;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgBank[(addr >> 13) & 0x03] & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return readChrBank<BankSize::B1K>(m_chrBank[(addr >> 10) & 0x07] & m_chrMask, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        writeChrBank<BankSize::B1K>(m_chrBank[(addr >> 10) & 0x07] & m_chrMask, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return cd().useFourScreenMirroring()
            ? MirroringType::FOUR_SCREEN
            : (m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL);
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    GERANES_HOT void setA12State(bool state) override
    {
        if(!m_a12LastState && state) {
            if(m_cycleCounter > 3) {
                clockIrq();
            }
        }
        else if(m_a12LastState && !state) {
            m_cycleCounter = 0;
        }

        m_a12LastState = state;
    }

    GERANES_HOT void cycle() override
    {
        if(static_cast<uint8_t>(m_cycleCounter + 1) != 0) {
            ++m_cycleCounter;
        }
    }

    void reset() override
    {
        m_prgBank[0] = 0;
        m_prgBank[1] = 1;
        m_prgBank[2] = 2;
        m_prgBank[3] = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);

        for(int i = 0; i < 8; ++i) {
            m_chrBank[i] = static_cast<uint8_t>(i & m_chrMask);
        }

        m_irqCounter = 0;
        m_irqEnabled = false;
        m_irqFlag = false;
        m_horizontalMirroring = false;
        m_a12LastState = true;
        m_cycleCounter = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_prgBank, 1, 4);
        s.array(m_chrBank, 1, 8);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqFlag);
        SERIALIZEDATA(s, m_horizontalMirroring);
        SERIALIZEDATA(s, m_a12LastState);
        SERIALIZEDATA(s, m_cycleCounter);
    }
};
