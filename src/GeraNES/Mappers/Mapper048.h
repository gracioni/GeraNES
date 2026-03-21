#pragma once

#include "BaseMapper.h"

class Mapper048 : public BaseMapper
{
private:
    uint8_t m_prgReg[2] = {0, 0};
    uint8_t m_prgMask = 0;

    uint8_t m_chrReg2k[2] = {0, 0};
    uint8_t m_chrReg1k[4] = {0, 0, 0, 0};
    uint8_t m_chr2kMask = 0;
    uint8_t m_chr1kMask = 0;

    bool m_mirroring = false;

    uint8_t m_reloadValue = 0;
    uint8_t m_irqCounter = 0;
    bool m_irqClearFlag = false;
    bool m_enableInterrupt = false;
    bool m_interruptFlag = false;

    bool m_a12LastState = true;
    uint8_t m_cycleCounter = 0;
    uint8_t m_irqDelay = 0;

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

    GERANES_INLINE void triggerIrq()
    {
        m_irqDelay = (cd().subMapperId() == 1) ? 6 : 22;
    }

    void count()
    {
        if(m_irqCounter == 0 || m_irqClearFlag) {
            m_irqCounter = m_reloadValue;
        }
        else {
            --m_irqCounter;
        }

        if(m_irqCounter == 0 && m_enableInterrupt) {
            triggerIrq();
        }

        m_irqClearFlag = false;
    }

public:
    Mapper048(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        if(hasChrRam()) {
            m_chr2kMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B2K));
            m_chr1kMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chr2kMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
            m_chr1kMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int absolute = addr + 0x8000;
        switch(absolute & 0xE003) {
        case 0x8000:
            m_prgReg[0] = (data & 0x3F) & m_prgMask;
            break;
        case 0x8001:
            m_prgReg[1] = (data & 0x3F) & m_prgMask;
            break;
        case 0x8002:
            m_chrReg2k[0] = data & m_chr2kMask;
            break;
        case 0x8003:
            m_chrReg2k[1] = data & m_chr2kMask;
            break;
        case 0xA000:
            m_chrReg1k[0] = data & m_chr1kMask;
            break;
        case 0xA001:
            m_chrReg1k[1] = data & m_chr1kMask;
            break;
        case 0xA002:
            m_chrReg1k[2] = data & m_chr1kMask;
            break;
        case 0xA003:
            m_chrReg1k[3] = data & m_chr1kMask;
            break;
        case 0xC000:
            m_interruptFlag = false;
            m_reloadValue = static_cast<uint8_t>(data ^ 0xFF);
            if(cd().subMapperId() == 1) {
                ++m_reloadValue;
            }
            break;
        case 0xC001:
            m_interruptFlag = false;
            m_irqClearFlag = true;
            m_irqCounter = 0;
            break;
        case 0xC002:
            m_enableInterrupt = true;
            break;
        case 0xC003:
            m_enableInterrupt = false;
            m_interruptFlag = false;
            m_irqDelay = 0;
            break;
        case 0xE000:
            m_mirroring = (data & 0x40) != 0;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg[0] & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1] & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(addr < 0x0800) return readChrBank<BankSize::B2K>(m_chrReg2k[0] & m_chr2kMask, addr);
        if(addr < 0x1000) return readChrBank<BankSize::B2K>(m_chrReg2k[1] & m_chr2kMask, addr);

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) - 4);
        return readChrBank<BankSize::B1K>(m_chrReg1k[slot] & m_chr1kMask, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(addr < 0x0800) {
            writeChrBank<BankSize::B2K>(m_chrReg2k[0] & m_chr2kMask, addr, data);
            return;
        }
        if(addr < 0x1000) {
            writeChrBank<BankSize::B2K>(m_chrReg2k[1] & m_chr2kMask, addr, data);
            return;
        }

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) - 4);
        writeChrBank<BankSize::B1K>(m_chrReg1k[slot] & m_chr1kMask, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_mirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT void setA12State(bool state) override
    {
        if(!m_a12LastState && state) {
            if(m_cycleCounter > 3) {
                count();
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

        if(m_irqDelay > 0) {
            --m_irqDelay;
            if(m_irqDelay == 0 && m_enableInterrupt) {
                m_interruptFlag = true;
            }
        }
    }

    void reset() override
    {
        m_prgReg[0] = 0;
        m_prgReg[1] = 0;
        m_chrReg2k[0] = 0;
        m_chrReg2k[1] = 0;
        m_chrReg1k[0] = 0;
        m_chrReg1k[1] = 0;
        m_chrReg1k[2] = 0;
        m_chrReg1k[3] = 0;
        m_mirroring = false;
        m_reloadValue = 0;
        m_irqCounter = 0;
        m_irqClearFlag = false;
        m_enableInterrupt = false;
        m_interruptFlag = false;
        m_a12LastState = true;
        m_cycleCounter = 0;
        m_irqDelay = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_prgReg, 1, 2);
        SERIALIZEDATA(s, m_prgMask);
        s.array(m_chrReg2k, 1, 2);
        s.array(m_chrReg1k, 1, 4);
        SERIALIZEDATA(s, m_chr2kMask);
        SERIALIZEDATA(s, m_chr1kMask);
        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_reloadValue);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqClearFlag);
        SERIALIZEDATA(s, m_enableInterrupt);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_a12LastState);
        SERIALIZEDATA(s, m_cycleCounter);
        SERIALIZEDATA(s, m_irqDelay);
    }
};
