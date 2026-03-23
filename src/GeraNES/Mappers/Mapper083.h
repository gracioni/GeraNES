#pragma once

#include "BaseMapper.h"

// iNES Mapper 83
class Mapper083 : public BaseMapper
{
private:
    uint8_t m_reg[11] = {0};
    uint8_t m_exReg[4] = {0};
    bool m_is2kBank = false;
    bool m_isNot2kBank = false;
    uint8_t m_mode = 0;
    uint8_t m_bank = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_interruptFlag = false;

    GERANES_INLINE uint16_t chrBank1k(uint8_t slot) const
    {
        if(m_is2kBank && !m_isNot2kBank) {
            switch(slot & 0x07) {
            case 0: return static_cast<uint16_t>((m_reg[0] << 1) + 0);
            case 1: return static_cast<uint16_t>((m_reg[0] << 1) + 1);
            case 2: return static_cast<uint16_t>((m_reg[1] << 1) + 0);
            case 3: return static_cast<uint16_t>((m_reg[1] << 1) + 1);
            case 4: return static_cast<uint16_t>((m_reg[6] << 1) + 0);
            case 5: return static_cast<uint16_t>((m_reg[6] << 1) + 1);
            case 6: return static_cast<uint16_t>((m_reg[7] << 1) + 0);
            default: return static_cast<uint16_t>((m_reg[7] << 1) + 1);
            }
        }

        return static_cast<uint16_t>(m_reg[slot & 0x07] | ((m_bank & 0x30) << 4));
    }

public:
    Mapper083(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if(addr >= 0x1100 && addr <= 0x1103) {
            m_exReg[addr & 0x03] = value;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if(addr == 0x1000) {
            return static_cast<uint8_t>(openBusData & 0xFC);
        }
        if(addr >= 0x1100 && addr <= 0x1103) {
            return m_exReg[addr & 0x03];
        }
        return openBusData;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute >= 0x8300 && absolute <= 0x8302) {
            m_mode &= 0xBF;
            m_reg[absolute - 0x8300 + 8] = value;
            return;
        }

        if(absolute >= 0x8310 && absolute <= 0x8317) {
            m_reg[absolute - 0x8310] = value;
            if(absolute >= 0x8312 && absolute <= 0x8315) {
                m_isNot2kBank = true;
            }
            return;
        }

        switch(absolute) {
        case 0x8000:
            m_is2kBank = true;
            m_bank = value;
            m_mode |= 0x40;
            break;
        case 0x8100:
            m_mode = static_cast<uint8_t>(value | (m_mode & 0x40));
            break;
        case 0x8200:
            m_irqCounter = static_cast<uint16_t>((m_irqCounter & 0xFF00) | value);
            m_interruptFlag = false;
            break;
        case 0x8201:
            m_irqEnabled = (m_mode & 0x80) != 0;
            m_irqCounter = static_cast<uint16_t>((m_irqCounter & 0x00FF) | (static_cast<uint16_t>(value) << 8));
            break;
        case 0xB000:
        case 0xB0FF:
        case 0xB1FF:
            m_bank = value;
            m_mode |= 0x40;
            break;
        default:
            break;
        }
    }

    GERANES_HOT void cycle() override
    {
        if(!m_irqEnabled) return;

        --m_irqCounter;
        if(m_irqCounter == 0) {
            m_irqEnabled = false;
            m_irqCounter = 0xFFFF;
            m_interruptFlag = true;
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if((m_mode & 0x40) != 0) {
            const uint16_t first = static_cast<uint16_t>((m_bank & 0x3F) << 1);
            const uint16_t last = static_cast<uint16_t>(((m_bank & 0x30) | 0x0F) << 1);
            switch((addr >> 13) & 0x03) {
            case 0: return cd().readPrg<BankSize::B8K>(first + 0, addr);
            case 1: return cd().readPrg<BankSize::B8K>(first + 1, addr);
            case 2: return cd().readPrg<BankSize::B8K>(last + 0, addr);
            default: return cd().readPrg<BankSize::B8K>(last + 1, addr);
            }
        }

        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_reg[8], addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_reg[9], addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_reg[10], addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint16_t bank = chrBank1k(static_cast<uint8_t>((addr >> 10) & 0x07));
        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint16_t bank = chrBank1k(static_cast<uint8_t>((addr >> 10) & 0x07));
        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mode & 0x03) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        default: return MirroringType::VERTICAL;
        }
    }

    void reset() override
    {
        memset(m_reg, 0, sizeof(m_reg));
        memset(m_exReg, 0, sizeof(m_exReg));
        m_is2kBank = false;
        m_isNot2kBank = false;
        m_mode = 0;
        m_bank = 0;
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_interruptFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_reg, 1, 11);
        s.array(m_exReg, 1, 4);
        SERIALIZEDATA(s, m_is2kBank);
        SERIALIZEDATA(s, m_isNot2kBank);
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_bank);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_interruptFlag);
    }
};
