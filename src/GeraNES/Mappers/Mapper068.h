#pragma once

#include "BaseMapper.h"

// Sunsoft-4 (iNES Mapper 68)
class Mapper068 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;

    uint8_t m_chrBank2k[4] = {0, 0, 0, 0};
    uint8_t m_chr2kMask = 0;

    uint8_t m_nameTableBank[2] = {0x80, 0x80}; // D7 is forced high in HW
    uint8_t m_chr1kMask = 0;

    uint8_t m_nameTableControl = 0; // bit4: NT source, bits1..0 mirroring mode
    bool m_prgRamEnabled = false;   // $F000 bit4

    GERANES_INLINE uint8_t mapNameTableToPair(uint8_t index) const
    {
        switch(m_nameTableControl & 0x03) {
        case 0: return static_cast<uint8_t>(index & 0x01);            // vertical
        case 1: return static_cast<uint8_t>((index >> 1) & 0x01);     // horizontal
        case 2: return 0;                                              // single-screen low
        case 3: return 1;                                              // single-screen high
        }
        return 0;
    }

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr)
    {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data)
    {
        if(hasChrRam()) writeChrRam<bs>(bank, addr, data);
    }

public:
    Mapper068(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());

        if(hasChrRam()) {
            m_chr2kMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B2K));
            m_chr1kMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chr2kMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
            m_chr1kMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int absolute = addr + 0x8000;

        switch(absolute & 0xF000) {
        case 0x8000: m_chrBank2k[0] = data & m_chr2kMask; break;
        case 0x9000: m_chrBank2k[1] = data & m_chr2kMask; break;
        case 0xA000: m_chrBank2k[2] = data & m_chr2kMask; break;
        case 0xB000: m_chrBank2k[3] = data & m_chr2kMask; break;

        case 0xC000: m_nameTableBank[0] = (data | 0x80) & m_chr1kMask; break;
        case 0xD000: m_nameTableBank[1] = (data | 0x80) & m_chr1kMask; break;

        case 0xE000:
            m_nameTableControl = static_cast<uint8_t>(data & 0x13);
            break;

        case 0xF000:
            m_prgBank = (data & 0x0F) & m_prgMask;
            m_prgRamEnabled = (data & 0x10) != 0;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 11) & 0x03);
        return readChrBank<BankSize::B2K>(m_chrBank2k[slot], addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 11) & 0x03);
        writeChrBank<BankSize::B2K>(m_chrBank2k[slot], addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_nameTableControl & 0x03) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        }
        return MirroringType::VERTICAL;
    }

    GERANES_HOT bool useCustomNameTable(uint8_t /*index*/) override
    {
        return (m_nameTableControl & 0x10) != 0;
    }

    GERANES_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr) override
    {
        const uint8_t bankPair = mapNameTableToPair(index);
        const uint8_t bank = m_nameTableBank[bankPair] & m_chr1kMask;
        return readChrBank<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t bankPair = mapNameTableToPair(index);
        const uint8_t bank = m_nameTableBank[bankPair] & m_chr1kMask;
        writeChrBank<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(m_prgRamEnabled) BaseMapper::writeSaveRam(addr, data);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(m_prgRamEnabled) return BaseMapper::readSaveRam(addr);
        return 0;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank2k[0] = 0;
        m_chrBank2k[1] = 0;
        m_chrBank2k[2] = 0;
        m_chrBank2k[3] = 0;
        m_nameTableBank[0] = static_cast<uint8_t>(0x80 & m_chr1kMask);
        m_nameTableBank[1] = static_cast<uint8_t>(0x80 & m_chr1kMask);
        m_nameTableControl = 0;
        m_prgRamEnabled = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        s.array(m_chrBank2k, 1, 4);
        SERIALIZEDATA(s, m_chr2kMask);
        s.array(m_nameTableBank, 1, 2);
        SERIALIZEDATA(s, m_chr1kMask);
        SERIALIZEDATA(s, m_nameTableControl);
        SERIALIZEDATA(s, m_prgRamEnabled);
    }
};
