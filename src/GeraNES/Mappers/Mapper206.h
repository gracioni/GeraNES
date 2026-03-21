#pragma once

#include "BaseMapper.h"

// iNES Mapper 206 (DxROM / Namco 108 family)
// MMC3-like bank registers on $8000/$8001, but:
// - no IRQ
// - no WRAM control
// - no mapper-controlled mirroring
class Mapper206 : public BaseMapper
{
protected:
    uint8_t m_bankSelect = 0;

    // CHR regs: [0..1]=2KB banks, [2..5]=1KB banks
    uint8_t m_chrReg[6] = {0};

    // PRG regs: two switchable 8KB slots at $8000 and $A000
    uint8_t m_prgReg0 = 0;
    uint8_t m_prgReg1 = 1;

    uint8_t m_prgMask = 0; // 8KB bank mask
    uint8_t m_chrMask = 0; // 1KB bank mask

    bool m_submapper1NoPrgBanking = false;

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr)
    {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data)
    {
        writeChrRam<bs>(bank, addr, data);
    }

public:
    Mapper206(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        } else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }

        // NES 2.0 submapper 1: boards with fixed 32KB PRG (no banking).
        m_submapper1NoPrgBanking = (cd.subMapperId() == 1);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_submapper1NoPrgBanking) {
            // Fixed 32KB PRG mapping ($8000-$FFFF).
            return cd().readPrg<BankSize::B32K>(0, addr);
        }

        switch(addr >> 13) { // addr / 8KB
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg0 & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg1 & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        case 3: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }

        return 0;
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        // In this core, PRG write addresses are relative to $8000 ($0000-$7FFF).
        // Mapper 206 uses even/odd address select for $8000/$8001 mirrors.
        switch(addr & 0x0001) {
        case 0x0000:
            // Only RRR bits are meaningful on mapper 206.
            m_bankSelect = data & 0x07;
            break;

        case 0x0001:
            switch(m_bankSelect) {
            case 0:
            case 1:
                // 2KB CHR bank regs use bits 5..1 (LSB ignored / forced even).
                m_chrReg[m_bankSelect] = static_cast<uint8_t>(data & 0x3E);
                break;
            case 2:
            case 3:
            case 4:
            case 5:
                // 1KB CHR bank regs use bits 5..0.
                m_chrReg[m_bankSelect] = static_cast<uint8_t>(data & 0x3F);
                break;
            case 6:
                // 8KB PRG bank @ $8000-$9FFF, bits 3..0
                m_prgReg0 = static_cast<uint8_t>(data & 0x0F) & m_prgMask;
                break;
            case 7:
                // 8KB PRG bank @ $A000-$BFFF, bits 3..0
                m_prgReg1 = static_cast<uint8_t>(data & 0x0F) & m_prgMask;
                break;
            }
            break;
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        switch(addr >> 10) { // addr / 1KB
        case 0:
        case 1: return readChrBank<BankSize::B2K>((m_chrReg[0] & m_chrMask) >> 1, addr);
        case 2:
        case 3: return readChrBank<BankSize::B2K>((m_chrReg[1] & m_chrMask) >> 1, addr);
        case 4: return readChrBank<BankSize::B1K>(m_chrReg[2] & m_chrMask, addr);
        case 5: return readChrBank<BankSize::B1K>(m_chrReg[3] & m_chrMask, addr);
        case 6: return readChrBank<BankSize::B1K>(m_chrReg[4] & m_chrMask, addr);
        case 7: return readChrBank<BankSize::B1K>(m_chrReg[5] & m_chrMask, addr);
        }

        return 0;
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        switch(addr >> 10) { // addr / 1KB
        case 0:
        case 1: writeChrBank<BankSize::B2K>((m_chrReg[0] & m_chrMask) >> 1, addr, data); break;
        case 2:
        case 3: writeChrBank<BankSize::B2K>((m_chrReg[1] & m_chrMask) >> 1, addr, data); break;
        case 4: writeChrBank<BankSize::B1K>(m_chrReg[2] & m_chrMask, addr, data); break;
        case 5: writeChrBank<BankSize::B1K>(m_chrReg[3] & m_chrMask, addr, data); break;
        case 6: writeChrBank<BankSize::B1K>(m_chrReg[4] & m_chrMask, addr, data); break;
        case 7: writeChrBank<BankSize::B1K>(m_chrReg[5] & m_chrMask, addr, data); break;
        }
    }

    void reset() override
    {
        m_bankSelect = 0;
        m_chrReg[0] = 0;
        m_chrReg[1] = 0;
        m_chrReg[2] = 0;
        m_chrReg[3] = 0;
        m_chrReg[4] = 0;
        m_chrReg[5] = 0;
        m_prgReg0 = 0;
        m_prgReg1 = 1;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bankSelect);
        SERIALIZEDATA(s, m_chrReg[0]);
        SERIALIZEDATA(s, m_chrReg[1]);
        SERIALIZEDATA(s, m_chrReg[2]);
        SERIALIZEDATA(s, m_chrReg[3]);
        SERIALIZEDATA(s, m_chrReg[4]);
        SERIALIZEDATA(s, m_chrReg[5]);
        SERIALIZEDATA(s, m_prgReg0);
        SERIALIZEDATA(s, m_prgReg1);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_submapper1NoPrgBanking);
    }
};
