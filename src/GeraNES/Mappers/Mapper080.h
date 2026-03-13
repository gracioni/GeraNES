#pragma once

#include "BaseMapper.h"
#include <array>

// Taito X1-005 (iNES Mapper 80)
class Mapper080 : public BaseMapper
{
private:
    uint8_t m_prgReg[3] = {0, 1, 2}; // 8K banks at $8000/$A000/$C000
    uint8_t m_chrReg2k[2] = {0, 2};  // 2K banks at $0000/$0800
    uint8_t m_chrReg1k[4] = {4, 5, 6, 7}; // 1K banks at $1000/$1400/$1800/$1C00

    uint8_t m_prgMask = 0; // 8K
    uint8_t m_chrMask = 0; // 1K

    bool m_verticalMirroring = false;
    uint8_t m_ramPermission = 0; // $A3 enables internal RAM

    // 128B internal RAM at $7F00-$7FFF, mirrored once.
    std::array<uint8_t, 0x80> m_internalRam = {};

    GERANES_INLINE bool internalRamEnabled() const
    {
        return m_ramPermission == 0xA3;
    }

    GERANES_INLINE bool isControlRegisterAddr(int addr) const
    {
        // Registers are at $7EF0-$7EFF. If CPU A7 is ignored, they also appear at $7E70-$7E7F.
        // In this core, $6000-$7FFF arrives as addr&0x1FFF.
        return (addr & 0x1F70) == 0x1E70;
    }

    GERANES_INLINE uint8_t mapChr1kPage(int slot) const
    {
        switch(slot & 0x07) {
        case 0: return static_cast<uint8_t>(m_chrReg2k[0] + 0) & m_chrMask;
        case 1: return static_cast<uint8_t>(m_chrReg2k[0] + 1) & m_chrMask;
        case 2: return static_cast<uint8_t>(m_chrReg2k[1] + 0) & m_chrMask;
        case 3: return static_cast<uint8_t>(m_chrReg2k[1] + 1) & m_chrMask;
        case 4: return m_chrReg1k[0] & m_chrMask;
        case 5: return m_chrReg1k[1] & m_chrMask;
        case 6: return m_chrReg1k[2] & m_chrMask;
        default: return m_chrReg1k[3] & m_chrMask;
        }
    }

public:
    Mapper080(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }

        m_prgReg[0] &= m_prgMask;
        m_prgReg[1] &= m_prgMask;
        m_prgReg[2] &= m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg[0] & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1] & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_prgReg[2] & m_prgMask, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t page = mapChr1kPage((addr >> 10) & 0x07);
        if(hasChrRam()) return readChrRam<BankSize::B1K>(page, addr);
        return cd().readChr<BankSize::B1K>(page, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t page = mapChr1kPage((addr >> 10) & 0x07);
        writeChrRam<BankSize::B1K>(page, addr, data);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(isControlRegisterAddr(addr)) {
            switch(addr & 0x0F) {
            case 0x0: m_chrReg2k[0] = data; break;
            case 0x1: m_chrReg2k[1] = data; break;
            case 0x2: m_chrReg1k[0] = data; break;
            case 0x3: m_chrReg1k[1] = data; break;
            case 0x4: m_chrReg1k[2] = data; break;
            case 0x5: m_chrReg1k[3] = data; break;
            case 0x6:
            case 0x7:
                m_verticalMirroring = (data & 0x01) != 0;
                break;
            case 0x8:
            case 0x9:
                m_ramPermission = data;
                break;
            case 0xA:
            case 0xB:
                m_prgReg[0] = data & m_prgMask;
                break;
            case 0xC:
            case 0xD:
                m_prgReg[1] = data & m_prgMask;
                break;
            case 0xE:
            case 0xF:
                m_prgReg[2] = data & m_prgMask;
                break;
            }
            return;
        }

        if(addr >= 0x1F00) {
            if(internalRamEnabled()) {
                m_internalRam[static_cast<size_t>((addr - 0x1F00) & 0x7F)] = data;
            }
            return;
        }

        // Optional external PRG-RAM at $6000-$7EFF, if present in header/DB.
        BaseMapper::writeSaveRam(addr, data);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(addr >= 0x1F00) {
            if(internalRamEnabled()) {
                return m_internalRam[static_cast<size_t>((addr - 0x1F00) & 0x7F)];
            }
            return 0;
        }

        return BaseMapper::readSaveRam(addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    void reset() override
    {
        m_prgReg[0] = 0 & m_prgMask;
        m_prgReg[1] = 1 & m_prgMask;
        m_prgReg[2] = 2 & m_prgMask;

        m_chrReg2k[0] = 0;
        m_chrReg2k[1] = 2;
        m_chrReg1k[0] = 4;
        m_chrReg1k[1] = 5;
        m_chrReg1k[2] = 6;
        m_chrReg1k[3] = 7;

        m_verticalMirroring = false;
        m_ramPermission = 0;
        m_internalRam.fill(0x00);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_prgReg[0]);
        SERIALIZEDATA(s, m_prgReg[1]);
        SERIALIZEDATA(s, m_prgReg[2]);
        SERIALIZEDATA(s, m_chrReg2k[0]);
        SERIALIZEDATA(s, m_chrReg2k[1]);
        SERIALIZEDATA(s, m_chrReg1k[0]);
        SERIALIZEDATA(s, m_chrReg1k[1]);
        SERIALIZEDATA(s, m_chrReg1k[2]);
        SERIALIZEDATA(s, m_chrReg1k[3]);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_verticalMirroring);
        SERIALIZEDATA(s, m_ramPermission);
        s.array(m_internalRam.data(), 1, static_cast<int>(m_internalRam.size()));
    }
};

