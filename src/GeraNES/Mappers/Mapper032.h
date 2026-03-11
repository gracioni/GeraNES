#pragma once

#include "BaseMapper.h"

// Irem G-101 (iNES Mapper 32)
class Mapper032 : public BaseMapper
{
private:
    uint8_t m_prgReg[2] = {0, 0};
    uint8_t m_prgMask = 0;
    uint8_t m_prgMode = 0;

    bool m_mirroring = false; // 0=vertical, 1=horizontal
    bool m_majorLeagueVariant = false; // submapper 1

    uint8_t m_chrReg[8] = {0};
    uint8_t m_chrMask = 0;

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
    Mapper032(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }

        m_majorLeagueVariant = (cd.subMapperId() == 1);
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int absolute = addr + 0x8000;
        if(absolute < 0x8000 || absolute > 0xFFFF) return;

        switch(absolute & 0xF000) {
        case 0x8000:
            m_prgReg[0] = (data & 0x1F) & m_prgMask;
            break;
        case 0x9000:
            m_prgMode = (data & 0x02) >> 1;
            if(m_majorLeagueVariant) {
                m_prgMode = 0;
            }

            if(!m_majorLeagueVariant) {
                m_mirroring = (data & 0x01) != 0;
            }
            break;
        case 0xA000:
            m_prgReg[1] = (data & 0x1F) & m_prgMask;
            break;
        case 0xB000:
            m_chrReg[absolute & 0x07] = data & m_chrMask;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t secondLast = (cd().numberOfPRGBanks<BankSize::B8K>() - 2) & m_prgMask;
        const uint8_t last = (cd().numberOfPRGBanks<BankSize::B8K>() - 1) & m_prgMask;

        if(m_prgMode == 0) {
            switch((addr >> 13) & 0x03) {
            case 0: return cd().readPrg<BankSize::B8K>(m_prgReg[0], addr);
            case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1], addr);
            case 2: return cd().readPrg<BankSize::B8K>(secondLast, addr);
            default: return cd().readPrg<BankSize::B8K>(last, addr);
            }
        }
        else {
            switch((addr >> 13) & 0x03) {
            case 0: return cd().readPrg<BankSize::B8K>(secondLast, addr);
            case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1], addr);
            case 2: return cd().readPrg<BankSize::B8K>(m_prgReg[0], addr);
            default: return cd().readPrg<BankSize::B8K>(last, addr);
            }
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return readChrBank<BankSize::B1K>(m_chrReg[(addr >> 10) & 0x07], addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        writeChrBank<BankSize::B1K>(m_chrReg[(addr >> 10) & 0x07], addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_majorLeagueVariant) return MirroringType::SINGLE_SCREEN_A;
        return m_mirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgReg[0] = 0;
        m_prgReg[1] = 0;
        m_prgMode = 0;

        m_mirroring = (cd().mirroringType() == MirroringType::HORIZONTAL);

        for(int i = 0; i < 8; i++) m_chrReg[i] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        s.array(m_prgReg, 1, 2);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_prgMode);
        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_majorLeagueVariant);

        s.array(m_chrReg, 1, 8);
        SERIALIZEDATA(s, m_chrMask);
    }
};
