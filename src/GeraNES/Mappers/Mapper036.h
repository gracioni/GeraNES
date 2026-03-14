#pragma once

#include "BaseMapper.h"

// iNES Mapper 36 (TXC 01-22000-400)
// Single register in $4100-$5FFF (A8 decode)
// - PRG: 32KB bank select (high nibble)
// - CHR: 8KB bank select (low nibble)
class Mapper036 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0; // 32KB

    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0; // 8KB

public:
    Mapper036(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B8K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
        }
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        // Register decode: $4100-$41FF, $4300-$43FF, ... , $5F00-$5FFF.
        if((addr & 0x0100) == 0) return;

        m_chrBank = static_cast<uint8_t>(data & 0x0F) & m_chrMask;
        m_prgBank = static_cast<uint8_t>((data >> 4) & 0x0F) & m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(m_chrBank, addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(m_chrBank, addr, data);
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
    }
};
