#pragma once

#include "BaseMapper.h"

// iNES Mapper 41 (Caltron 6-in-1)
// - $6000-$67FF: outer register via address bits (data ignored)
// - $8000-$FFFF: inner CHR register via data bits (bus conflicts), only when enabled
class Mapper041 : public BaseMapper
{
private:
    uint8_t m_prgBank32k = 0;   // 3 bits: EPP
    uint8_t m_outerChr32k = 0;  // 2 bits: CC
    uint8_t m_innerChr8k = 0;   // 2 bits: cc
    bool m_innerEnable = false; // E
    bool m_horizontalMirroring = false; // M

    uint16_t m_prgCount32k = 0;
    uint16_t m_chrCount8k = 0;

    GERANES_INLINE uint16_t mapBank(uint16_t bank, uint16_t count) const
    {
        if(count == 0) return 0;
        return static_cast<uint16_t>(bank % count);
    }

    GERANES_INLINE uint16_t chrBank8k() const
    {
        // Outer is 32KB (4x8KB), inner selects one of 4 pages.
        return static_cast<uint16_t>((m_outerChr32k << 2) | m_innerChr8k);
    }

public:
    Mapper041(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgCount32k = static_cast<uint16_t>(cd.numberOfPRGBanks<BankSize::B32K>());

        if(hasChrRam()) {
            m_chrCount8k = static_cast<uint16_t>(cd.chrRamSize() / static_cast<int>(BankSize::B8K));
        } else {
            m_chrCount8k = static_cast<uint16_t>(cd.numberOfCHRBanks<BankSize::B8K>());
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(mapBank(m_prgBank32k, m_prgCount32k), addr);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t /*data*/) override
    {
        // CPU $6000-$67FF => relative $0000-$07FF.
        if(addr >= 0x0800) return;

        // Address decode from NESdev: 0110 0xxx xxMC CEPP
        const uint8_t e = static_cast<uint8_t>((addr >> 2) & 0x01);
        const uint8_t pp = static_cast<uint8_t>(addr & 0x03);

        m_prgBank32k = static_cast<uint8_t>((e << 2) | pp);           // EPP
        m_innerEnable = e != 0;                                       // E
        m_outerChr32k = static_cast<uint8_t>((addr >> 3) & 0x03);     // CC (A4..A3)
        m_horizontalMirroring = ((addr >> 5) & 0x01) != 0;            // M
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(!m_innerEnable) return;

        // Inner CHR register has bus conflicts.
        data &= readPrg(addr);
        m_innerChr8k = static_cast<uint8_t>(data & 0x03);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint16_t bank = mapBank(chrBank8k(), m_chrCount8k);
        if(hasChrRam()) return readChrRam<BankSize::B8K>(bank, addr);
        return cd().readChr<BankSize::B8K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint16_t bank = mapBank(chrBank8k(), m_chrCount8k);
        writeChrRam<BankSize::B8K>(bank, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgBank32k = 0;
        m_outerChr32k = 0;
        m_innerChr8k = 0;
        m_innerEnable = false;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank32k);
        SERIALIZEDATA(s, m_outerChr32k);
        SERIALIZEDATA(s, m_innerChr8k);
        SERIALIZEDATA(s, m_innerEnable);
        SERIALIZEDATA(s, m_horizontalMirroring);
        SERIALIZEDATA(s, m_prgCount32k);
        SERIALIZEDATA(s, m_chrCount8k);
    }
};
