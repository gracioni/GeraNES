#pragma once

#include "BaseMapper.h"

// iNES Mapper 242
// Address-latch multicart with 8KB unbanked CHR-RAM and 32KB PRG bank selection.
class Mapper242 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper242(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B8K));
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        m_horizontalMirroring = (absolute & 0x02) != 0;
        m_prgBank = static_cast<uint8_t>((absolute >> 3) & 0x0F) & m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return BaseMapper::readChr(addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        BaseMapper::writeChr(addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
