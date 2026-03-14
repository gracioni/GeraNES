#pragma once

#include "BaseMapper.h"

// iNES Mapper 232 (Camerica BF9096)
// $8000-$BFFF: select 64KB PRG block (...BB...)
// $C000-$FFFF: select 16KB page within that block (......PP)
// CPU $C000-$FFFF is fixed to page 3 of selected 64KB block.
class Mapper232 : public BaseMapper
{
private:
    uint8_t m_outerBlock = 0; // 64KB block index (2 bits)
    uint8_t m_innerPage = 0;  // 16KB page index (2 bits)
    uint8_t m_prgMask16k = 0; // 16KB bank mask

    GERANES_INLINE uint8_t decodeOuterFromData(uint8_t data) const
    {
        // "...BB..." => bits 4..3
        return static_cast<uint8_t>((data >> 3) & 0x03);
    }

    GERANES_INLINE uint8_t map16k(uint8_t pageInBlock) const
    {
        return static_cast<uint8_t>(((m_outerBlock << 2) | (pageInBlock & 0x03)) & m_prgMask16k);
    }

public:
    Mapper232(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask16k = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(addr < 0x4000) {
            m_outerBlock = decodeOuterFromData(data);
        } else {
            m_innerPage = static_cast<uint8_t>(data & 0x03);
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) {
            return cd().readPrg<BankSize::B16K>(map16k(m_innerPage), addr);
        }

        return cd().readPrg<BankSize::B16K>(map16k(3), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr(addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) BaseMapper::writeChr(addr, data);
    }

    void reset() override
    {
        m_outerBlock = 0;
        m_innerPage = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_outerBlock);
        SERIALIZEDATA(s, m_innerPage);
        SERIALIZEDATA(s, m_prgMask16k);
    }
};
