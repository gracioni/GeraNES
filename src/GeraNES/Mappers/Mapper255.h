#pragma once

#include "BaseMapper.h"

// iNES Mapper 255
// BMC 110-in-1 style address-latched banking.
class Mapper255 : public BaseMapper
{
private:
    uint8_t m_prgLowBank = 0;
    uint8_t m_prgHighBank = 1;
    uint8_t m_chrBank = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper255(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        const uint8_t prgBit = (absolute & 0x1000) ? 0 : 1;
        const uint8_t bank = static_cast<uint8_t>(((absolute >> 8) & 0x40) | ((absolute >> 6) & 0x3F));

        m_prgLowBank = static_cast<uint8_t>(bank & ~prgBit);
        m_prgHighBank = static_cast<uint8_t>(bank | prgBit);
        m_chrBank = static_cast<uint8_t>(((absolute >> 8) & 0x40) | (absolute & 0x3F));
        m_horizontalMirroring = (absolute & 0x2000) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgLowBank, addr);
        return cd().readPrg<BankSize::B16K>(m_prgHighBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgLowBank = 0;
        m_prgHighBank = 1;
        m_chrBank = 0;
        m_horizontalMirroring = false;
        writePrg(0, 0);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgLowBank);
        SERIALIZEDATA(s, m_prgHighBank);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
