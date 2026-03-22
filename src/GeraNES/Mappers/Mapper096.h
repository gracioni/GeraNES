#pragma once

#include "BaseMapper.h"

class Mapper096 : public BaseMapper
{
private:
    uint8_t m_outerChrBank = 0;
    uint8_t m_innerChrBank = 0;
    uint16_t m_lastAddress = 0;
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper096(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B4K>());
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t value) override
    {
        m_prgBank = static_cast<uint8_t>(value & 0x03) & m_prgMask;
        m_outerChrBank = static_cast<uint8_t>(value & 0x04);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        const uint8_t slot = static_cast<uint8_t>((addr >> 12) & 0x01);
        const uint8_t bank = static_cast<uint8_t>((slot == 0 ? (m_outerChrBank | m_innerChrBank) : (m_outerChrBank | 0x03)) & m_chrMask);
        return cd().readChr<BankSize::B4K>(bank, addr);
    }

    GERANES_HOT void onPpuRead(uint16_t addr) override
    {
        if((m_lastAddress & 0x3000) != 0x2000 && (addr & 0x3000) == 0x2000) {
            m_innerChrBank = static_cast<uint8_t>((addr >> 8) & 0x03);
        }
        m_lastAddress = addr;
    }

    void reset() override
    {
        m_outerChrBank = 0;
        m_innerChrBank = 0;
        m_lastAddress = 0;
        m_prgBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_outerChrBank);
        SERIALIZEDATA(s, m_innerChrBank);
        SERIALIZEDATA(s, m_lastAddress);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
    }
};
