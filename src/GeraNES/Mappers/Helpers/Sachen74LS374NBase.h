#pragma once

#include "../BaseMapper.h"

class Sachen74LS374NBase : public BaseMapper
{
private:
    bool m_mapper150Mode = false;
    uint8_t m_currentRegister = 0;
    uint8_t m_reg[8] = {0};

    GERANES_INLINE void updateState()
    {
        uint8_t chrPage = 0;
        if(m_mapper150Mode) {
            chrPage = static_cast<uint8_t>(((m_reg[4] & 0x01) << 2) | (m_reg[6] & 0x03));
        } else {
            chrPage = static_cast<uint8_t>((m_reg[2] & 0x01) | ((m_reg[4] & 0x01) << 1) | ((m_reg[6] & 0x03) << 2));
        }

        m_prgBank = static_cast<uint8_t>(m_reg[5] & 0x03);
        m_chrBank = chrPage;

        switch((m_reg[7] >> 1) & 0x03) {
        case 0:
            m_mirroringMode = 0xFE; // custom 0001
            break;
        case 1:
            m_mirroringMode = 1; // horizontal
            break;
        case 2:
            m_mirroringMode = 0; // vertical
            break;
        default:
            m_mirroringMode = 2; // single-screen A
            break;
        }
    }

protected:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_mirroringMode = 0;

public:
    Sachen74LS374NBase(ICartridgeData& cd, bool mapper150Mode) : BaseMapper(cd), m_mapper150Mode(mapper150Mode)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        switch(addr & 0x0101) {
        case 0x0100:
            m_currentRegister = value & 0x07;
            break;
        case 0x0101:
            m_reg[m_currentRegister] = value & 0x07;
            updateState();
            break;
        default:
            break;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if((addr & 0x0101) == 0x0101) {
            return static_cast<uint8_t>((openBusData & 0xF8) | (m_reg[m_currentRegister] & 0x07));
        }
        return openBusData;
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        writeMapperRegister(addr + 0x1000, value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroringMode) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 0xFE: return MirroringType::CUSTOM;
        default: return MirroringType::VERTICAL;
        }
    }

    GERANES_HOT uint8_t customMirroring(uint8_t blockIndex) override
    {
        if(m_mirroringMode == 0xFE) {
            static const uint8_t map[4] = {0, 0, 0, 1};
            return map[blockIndex & 0x03];
        }
        return BaseMapper::customMirroring(blockIndex);
    }

    void reset() override
    {
        m_currentRegister = 0;
        memset(m_reg, 0, sizeof(m_reg));
        m_prgBank = 0;
        m_chrBank = 0;
        m_mirroringMode = 0;
        updateState();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_mapper150Mode);
        SERIALIZEDATA(s, m_currentRegister);
        s.array(m_reg, 1, 8);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_mirroringMode);
    }
};
