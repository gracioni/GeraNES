#pragma once

#include "BaseMapper.h"

class Mapper108 : public BaseMapper
{
private:
    uint8_t m_prgReg = 0xFF;
    uint8_t m_chrReg = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper108(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        if(((absolute & 0x9000) == 0x8000) || absolute >= 0xF000) {
            m_prgReg = value;
            m_chrReg = value;
        } else {
            m_chrReg = value & 0x01;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x6000) {
            const uint8_t pageCount = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>());
            return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(pageCount - 4 + ((addr >> 13) & 0x03)), addr);
        }
        return cd().readPrg<BankSize::B8K>(m_prgReg & m_prgMask, addr & 0x1FFF);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgReg & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrReg & m_chrMask, addr);
    }

    void reset() override
    {
        m_prgReg = 0xFF;
        m_chrReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_chrReg);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
    }
};
