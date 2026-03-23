#pragma once

#include "../BaseMapper.h"

enum class Sachen8259Variant : uint8_t
{
    Sachen8259A,
    Sachen8259B,
    Sachen8259C,
    Sachen8259D,
};

class Sachen8259Base : public BaseMapper
{
private:
    Sachen8259Variant m_variant;
    uint8_t m_currentReg = 0;
    uint8_t m_reg[8] = {0};
    uint8_t m_shift = 0;
    uint8_t m_chrOr[3] = {0};

protected:
    bool m_verticalMirroring = true;
    uint8_t m_singleScreen = 0xFF;

    GERANES_INLINE void updateState()
    {
        const bool simpleMode = (m_reg[7] & 0x01) != 0;

        switch((m_reg[7] >> 1) & 0x03) {
        case 0:
            m_verticalMirroring = (m_variant != Sachen8259Variant::Sachen8259D);
            m_singleScreen = 0xFF;
            break;
        case 1:
            m_verticalMirroring = (m_variant == Sachen8259Variant::Sachen8259D);
            m_singleScreen = 0xFF;
            break;
        case 2:
            m_singleScreen = 0xFE;
            break;
        default:
            m_singleScreen = 0;
            break;
        }

        if(simpleMode) {
            m_verticalMirroring = (m_variant != Sachen8259Variant::Sachen8259D);
            m_singleScreen = 0xFF;
        }
    }

public:
    Sachen8259Base(ICartridgeData& cd, Sachen8259Variant variant) : BaseMapper(cd), m_variant(variant)
    {
        switch(variant) {
        case Sachen8259Variant::Sachen8259A:
            m_shift = 1;
            m_chrOr[0] = 1;
            m_chrOr[1] = 0;
            m_chrOr[2] = 1;
            break;
        case Sachen8259Variant::Sachen8259B:
            m_shift = 0;
            break;
        case Sachen8259Variant::Sachen8259C:
            m_shift = 2;
            m_chrOr[0] = 1;
            m_chrOr[1] = 2;
            m_chrOr[2] = 3;
            break;
        case Sachen8259Variant::Sachen8259D:
            break;
        }
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        switch(addr & 0x0101) {
        case 0x0100:
            m_currentReg = value & 0x07;
            break;
        case 0x0101:
            m_reg[m_currentReg] = value & 0x07;
            updateState();
            break;
        default:
            break;
        }
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        writeMapperRegister(addr + 0x1000, value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_reg[5], addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        const bool simpleMode = (m_reg[7] & 0x01) != 0;
        if(m_variant == Sachen8259Variant::Sachen8259D) {
            const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x03);
            uint16_t bank = 0;
            switch(slot) {
            case 0: bank = m_reg[0]; break;
            case 1: bank = static_cast<uint16_t>(((m_reg[4] & 0x01) << 4) | m_reg[simpleMode ? 0 : 1]); break;
            case 2: bank = static_cast<uint16_t>(((m_reg[4] & 0x02) << 3) | m_reg[simpleMode ? 0 : 2]); break;
            default: bank = static_cast<uint16_t>(((m_reg[4] & 0x04) << 2) | ((m_reg[6] & 0x01) << 3) | m_reg[simpleMode ? 0 : 3]); break;
            }
            return cd().readChr<BankSize::B1K>(bank, addr);
        }

        const uint8_t chrHigh = static_cast<uint8_t>(m_reg[4] << 3);
        const uint8_t slot2k = static_cast<uint8_t>((addr >> 11) & 0x03);
        uint16_t bank = 0;
        switch(slot2k) {
        case 0: bank = static_cast<uint16_t>((chrHigh | m_reg[0]) << m_shift); break;
        case 1: bank = static_cast<uint16_t>(((chrHigh | m_reg[simpleMode ? 0 : 1]) << m_shift) | m_chrOr[0]); break;
        case 2: bank = static_cast<uint16_t>(((chrHigh | m_reg[simpleMode ? 0 : 2]) << m_shift) | m_chrOr[1]); break;
        default: bank = static_cast<uint16_t>(((chrHigh | m_reg[simpleMode ? 0 : 3]) << m_shift) | m_chrOr[2]); break;
        }
        return cd().readChr<BankSize::B2K>(bank, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_singleScreen == 0xFE) return MirroringType::CUSTOM;
        if(m_singleScreen == 0) return MirroringType::SINGLE_SCREEN_A;
        if(m_singleScreen == 1) return MirroringType::SINGLE_SCREEN_B;
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    GERANES_HOT uint8_t customMirroring(uint8_t blockIndex) override
    {
        if(m_singleScreen == 0xFE) {
            static const uint8_t map[4] = {0, 1, 1, 1};
            return map[blockIndex & 0x03];
        }
        return BaseMapper::customMirroring(blockIndex);
    }

    void reset() override
    {
        m_currentReg = 0;
        memset(m_reg, 0, sizeof(m_reg));
        m_verticalMirroring = true;
        m_singleScreen = 0xFF;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_currentReg);
        s.array(m_reg, 1, 8);
        SERIALIZEDATA(s, m_shift);
        s.array(m_chrOr, 1, 3);
        SERIALIZEDATA(s, m_verticalMirroring);
        SERIALIZEDATA(s, m_singleScreen);
    }
};
