#pragma once

#include "BaseMapper.h"

// iNES Mapper 112
class Mapper112 : public BaseMapper
{
private:
    uint8_t m_currentReg = 0;
    uint8_t m_outerChrBank = 0;
    uint8_t m_reg[8] = {0};
    bool m_horizontalMirroring = false;

    GERANES_INLINE void handleWrite(uint16_t absolute, uint8_t value)
    {
        switch(absolute & 0xE001) {
        case 0x8000: m_currentReg = value & 0x07; break;
        case 0xA000: m_reg[m_currentReg] = value; break;
        case 0xC000: m_outerChrBank = value; break;
        case 0xE000: m_horizontalMirroring = (value & 0x01) != 0; break;
        default: break;
        }
    }

public:
    Mapper112(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        handleWrite(static_cast<uint16_t>(addr + 0x4000), value);
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        handleWrite(static_cast<uint16_t>(addr + 0x8000), value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_reg[0], addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_reg[1], addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        uint16_t bank = 0;

        switch(slot) {
        case 0:
        case 1: bank = static_cast<uint16_t>((m_reg[2] & 0xFE) + (slot & 0x01)); break;
        case 2:
        case 3: bank = static_cast<uint16_t>((m_reg[3] & 0xFE) + (slot & 0x01)); break;
        case 4: bank = static_cast<uint16_t>(m_reg[4] | ((m_outerChrBank & 0x10) << 4)); break;
        case 5: bank = static_cast<uint16_t>(m_reg[5] | ((m_outerChrBank & 0x20) << 3)); break;
        case 6: bank = static_cast<uint16_t>(m_reg[6] | ((m_outerChrBank & 0x40) << 2)); break;
        default: bank = static_cast<uint16_t>(m_reg[7] | ((m_outerChrBank & 0x80) << 1)); break;
        }

        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        uint16_t bank = 0;

        switch(slot) {
        case 0:
        case 1: bank = static_cast<uint16_t>((m_reg[2] & 0xFE) + (slot & 0x01)); break;
        case 2:
        case 3: bank = static_cast<uint16_t>((m_reg[3] & 0xFE) + (slot & 0x01)); break;
        case 4: bank = static_cast<uint16_t>(m_reg[4] | ((m_outerChrBank & 0x10) << 4)); break;
        case 5: bank = static_cast<uint16_t>(m_reg[5] | ((m_outerChrBank & 0x20) << 3)); break;
        case 6: bank = static_cast<uint16_t>(m_reg[6] | ((m_outerChrBank & 0x40) << 2)); break;
        default: bank = static_cast<uint16_t>(m_reg[7] | ((m_outerChrBank & 0x80) << 1)); break;
        }

        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_currentReg = 0;
        m_outerChrBank = 0;
        memset(m_reg, 0, sizeof(m_reg));
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_currentReg);
        SERIALIZEDATA(s, m_outerChrBank);
        s.array(m_reg, 1, 8);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
