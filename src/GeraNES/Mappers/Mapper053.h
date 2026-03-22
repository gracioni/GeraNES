#pragma once

#include "BaseMapper.h"
#include "util/Crc32.h"

class Mapper053 : public BaseMapper
{
private:
    static constexpr uint32_t EPROM_CRC = 0x63794E25;

    uint8_t m_regs[2] = {0, 0};
    bool m_epromFirst = false;
    uint8_t m_savePage = 0;
    uint8_t m_prgPage[4] = {0, 1, 2, 3};
    uint8_t m_prgMask = 0;

    void updateState()
    {
        const uint16_t r = static_cast<uint16_t>((m_regs[0] << 3) & 0x78);

        m_savePage = static_cast<uint8_t>(((r << 1) | 0x0F) + (m_epromFirst ? 0x04 : 0x00));
        m_savePage &= m_prgMask;

        uint16_t low16k = 0;
        uint16_t high16k = 0;

        if(m_regs[0] & 0x10) {
            low16k = static_cast<uint16_t>((r | (m_regs[1] & 0x07)) + (m_epromFirst ? 0x02 : 0x00));
            high16k = static_cast<uint16_t>((r | 0x07) + (m_epromFirst ? 0x02 : 0x00));
        }
        else {
            low16k = static_cast<uint16_t>(m_epromFirst ? 0x00 : 0x80);
            high16k = static_cast<uint16_t>(m_epromFirst ? 0x01 : 0x81);
        }

        const uint8_t low8k = static_cast<uint8_t>((low16k << 1) & m_prgMask);
        const uint8_t high8k = static_cast<uint8_t>((high16k << 1) & m_prgMask);

        m_prgPage[0] = low8k;
        m_prgPage[1] = static_cast<uint8_t>((low8k + 1) & m_prgMask);
        m_prgPage[2] = high8k;
        m_prgPage[3] = static_cast<uint8_t>((high8k + 1) & m_prgMask);
    }

public:
    Mapper053(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        Crc32 crc;
        const int size = (cd.prgSize() >= 0x8000) ? 0x8000 : cd.prgSize();
        for(int i = 0; i < size; ++i) {
            const uint8_t value = cd.readPrg(i);
            crc.add(reinterpret_cast<const char*>(&value), 1);
        }
        m_epromFirst = (cd.prgSize() >= 0x8000) && (crc.get() == EPROM_CRC);
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t data) override
    {
        m_regs[0] = data;
        updateState();
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_regs[1] = data;
        updateState();
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgPage[(addr >> 13) & 0x03], addr);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_savePage, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return (m_regs[0] & 0x20) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_regs[0] = 0;
        m_regs[1] = 0;
        updateState();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_regs, 1, 2);
        SERIALIZEDATA(s, m_epromFirst);
        SERIALIZEDATA(s, m_savePage);
        s.array(m_prgPage, 1, 4);
        SERIALIZEDATA(s, m_prgMask);
    }
};
