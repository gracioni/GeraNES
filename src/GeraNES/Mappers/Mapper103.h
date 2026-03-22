#pragma once

#include "BaseMapper.h"
#include <vector>

class Mapper103 : public BaseMapper
{
private:
    bool m_prgRamDisabled = false;
    uint8_t m_prgReg = 0;
    uint8_t m_prgMask = 0;
    bool m_horizontalMirroring = false;
    std::vector<uint8_t> m_workRam;

public:
    Mapper103(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_workRam.resize(0x4000, 0x00);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(!m_prgRamDisabled && addr >= 0x3800 && addr < 0x5800) {
            return m_workRam[0x2000 + addr - 0x3800];
        }

        const uint8_t pageCount = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>());
        return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(pageCount - 4 + ((addr >> 13) & 0x03)), addr);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(m_prgRamDisabled) {
            return cd().readPrg<BankSize::B8K>(m_prgReg & m_prgMask, addr);
        }
        return m_workRam[addr & 0x1FFF];
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        m_workRam[addr & 0x1FFF] = value;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        switch(absolute & 0xF000) {
        case 0x8000:
            m_prgReg = static_cast<uint8_t>(value & 0x0F);
            break;
        case 0xB000:
        case 0xC000:
        case 0xD000:
            if(absolute >= 0xB800 && absolute < 0xD800) {
                m_workRam[0x2000 + absolute - 0xB800] = value;
            }
            break;
        case 0xE000:
            m_horizontalMirroring = (value & 0x08) != 0;
            break;
        case 0xF000:
            m_prgRamDisabled = (value & 0x10) != 0;
            break;
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgRamDisabled = false;
        m_prgReg = 0;
        m_horizontalMirroring = false;
        std::fill(m_workRam.begin(), m_workRam.end(), 0x00);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgRamDisabled);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_horizontalMirroring);
        if(!m_workRam.empty()) {
            s.array(m_workRam.data(), 1, static_cast<int>(m_workRam.size()));
        }
    }
};
