#pragma once

#include "BaseMapper.h"
#include <array>

// Taito X1-017 (iNES Mapper 82)
class Mapper082 : public BaseMapper
{
private:
    uint8_t m_prgReg[3] = {0, 1, 2}; // 8K banks at $8000/$A000/$C000
    uint8_t m_chrReg2k[2] = {0, 2};  // two 2K banks
    uint8_t m_chrReg1k[4] = {4, 5, 6, 7}; // four 1K banks

    uint8_t m_prgMask = 0; // 8K
    uint8_t m_chrMask = 0; // 1K

    bool m_verticalMirroring = false;
    bool m_chrMode = false; // $7EF6 bit1: CHR A12 inversion
    uint8_t m_prgRamEnable[3] = {0, 0, 0}; // $7EF7/$7EF8/$7EF9
    std::array<uint8_t, 0x1400> m_internalPrgRam = {}; // $6000-$73FF

    // IRQ registers exist on X1-017, but commercial games are not known to use IRQ.
    uint8_t m_irqLatch = 0;
    uint8_t m_irqControl = 0;

    GERANES_INLINE bool isControlRegisterAddr(int addr) const
    {
        // Registers are at $7EF0-$7EFF. If CPU A7 is ignored, they also appear at $7E70-$7E7F.
        // In this core, $6000-$7FFF arrives as addr&0x1FFF.
        return (addr & 0x1F70) == 0x1E70;
    }

    GERANES_INLINE uint8_t decodePrgBankFromData(int data) const
    {
        // iNES Mapper 82 encodes PRG bank in bits 5..2 (..DCBA..).
        return static_cast<uint8_t>((data >> 2) & 0x0F) & m_prgMask;
    }

    GERANES_INLINE bool prgRamRegionEnabled(int addr) const
    {
        if(addr < 0x0800) return m_prgRamEnable[0] == 0xCA; // $6000-$67FF
        if(addr < 0x1000) return m_prgRamEnable[1] == 0x69; // $6800-$6FFF
        if(addr < 0x1400) return m_prgRamEnable[2] == 0x84; // $7000-$73FF
        return false;
    }

    GERANES_INLINE uint8_t mapChr1kPage(int slot) const
    {
        // mode 0: 2K regs at $0000-$0FFF, 1K regs at $1000-$1FFF
        // mode 1: 1K regs at $0000-$0FFF, 2K regs at $1000-$1FFF
        if(!m_chrMode) {
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

        switch(slot & 0x07) {
        case 0: return m_chrReg1k[0] & m_chrMask;
        case 1: return m_chrReg1k[1] & m_chrMask;
        case 2: return m_chrReg1k[2] & m_chrMask;
        case 3: return m_chrReg1k[3] & m_chrMask;
        case 4: return static_cast<uint8_t>(m_chrReg2k[0] + 0) & m_chrMask;
        case 5: return static_cast<uint8_t>(m_chrReg2k[0] + 1) & m_chrMask;
        case 6: return static_cast<uint8_t>(m_chrReg2k[1] + 0) & m_chrMask;
        default: return static_cast<uint8_t>(m_chrReg2k[1] + 1) & m_chrMask;
        }
    }

public:
    Mapper082(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        } else {
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
            case 0x0: m_chrReg2k[0] = static_cast<uint8_t>(data & 0xFE); break;
            case 0x1: m_chrReg2k[1] = static_cast<uint8_t>(data & 0xFE); break;
            case 0x2: m_chrReg1k[0] = data; break;
            case 0x3: m_chrReg1k[1] = data; break;
            case 0x4: m_chrReg1k[2] = data; break;
            case 0x5: m_chrReg1k[3] = data; break;
            case 0x6:
                m_verticalMirroring = (data & 0x01) != 0;
                m_chrMode = (data & 0x02) != 0;
                break;
            case 0x7: m_prgRamEnable[0] = data; break;       // $7EF7
            case 0x8: m_prgRamEnable[1] = data; break;       // $7EF8
            case 0x9: m_prgRamEnable[2] = data; break;       // $7EF9
            case 0xA: m_prgReg[0] = decodePrgBankFromData(data); break; // $7EFA
            case 0xB: m_prgReg[1] = decodePrgBankFromData(data); break; // $7EFB
            case 0xC: m_prgReg[2] = decodePrgBankFromData(data); break; // $7EFC
            case 0xD: m_irqLatch = data; break;              // $7EFD
            case 0xE: m_irqControl = data; break;            // $7EFE
            case 0xF: break;                                 // $7EFF ack/reload (IRQ not emulated)
            }
            return;
        }

        if(addr < 0x1400 && prgRamRegionEnabled(addr)) {
            m_internalPrgRam[static_cast<size_t>(addr & 0x13FF)] = data;
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(addr < 0x1400 && prgRamRegionEnabled(addr)) {
            return m_internalPrgRam[static_cast<size_t>(addr & 0x13FF)];
        }

        // X1-017 has strong pull-downs; disabled/open areas read as 0.
        return 0;
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
        m_chrMode = false;
        m_prgRamEnable[0] = 0;
        m_prgRamEnable[1] = 0;
        m_prgRamEnable[2] = 0;
        m_internalPrgRam.fill(0x00);
        m_irqLatch = 0;
        m_irqControl = 0;
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
        SERIALIZEDATA(s, m_chrMode);
        SERIALIZEDATA(s, m_prgRamEnable[0]);
        SERIALIZEDATA(s, m_prgRamEnable[1]);
        SERIALIZEDATA(s, m_prgRamEnable[2]);
        s.array(m_internalPrgRam.data(), 1, static_cast<int>(m_internalPrgRam.size()));
        SERIALIZEDATA(s, m_irqLatch);
        SERIALIZEDATA(s, m_irqControl);
    }
};
