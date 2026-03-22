#pragma once

#include "BaseMapper.h"

class Mapper051 : public BaseMapper
{
private:
    uint8_t m_bank = 0;
    uint8_t m_mode = 1;
    uint8_t m_prgMask = 0;

public:
    Mapper051(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t data) override
    {
        m_mode = static_cast<uint8_t>(((data >> 3) & 0x02) | ((data >> 1) & 0x01));
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(addr >= 0x4000 && addr < 0x6000) {
            m_bank = data & 0x0F;
            m_mode = static_cast<uint8_t>(((data >> 3) & 0x02) | (m_mode & 0x01));
        }
        else {
            m_bank = data & 0x0F;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        if(m_mode & 0x01) {
            return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(((m_bank << 2) | slot) & m_prgMask), addr);
        }

        if(slot < 2) {
            return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>((((m_bank << 2) | m_mode) + slot) & m_prgMask), addr);
        }

        return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>((((m_bank << 2) | 0x0E) + (slot - 2)) & m_prgMask), addr);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        uint8_t page = 0;
        if(m_mode & 0x01) {
            page = static_cast<uint8_t>((0x23 | (m_bank << 2)) & m_prgMask);
        }
        else {
            page = static_cast<uint8_t>((0x2F | (m_bank << 2)) & m_prgMask);
        }
        return cd().readPrg<BankSize::B8K>(page, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return (m_mode == 0x03) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_bank = 0;
        m_mode = 1;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bank);
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_prgMask);
    }
};
