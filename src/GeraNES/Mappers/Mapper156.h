#pragma once

#include "BaseMapper.h"

class Mapper156 : public BaseMapper
{
private:
    uint8_t m_chrLow[8] = {0};
    uint8_t m_chrHigh[8] = {0};

    GERANES_INLINE void updateChrBanks()
    {
        if(!hasChrRam()) return;
    }

public:
    Mapper156(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute >= 0xC000 && absolute <= 0xC00F) {
            const uint8_t bank = static_cast<uint8_t>((absolute & 0x03) + ((absolute >= 0xC008) ? 4 : 0));
            if((absolute & 0x04) != 0) {
                m_chrHigh[bank] = value;
            } else {
                m_chrLow[bank] = value;
            }
            return;
        }

        switch(absolute) {
        case 0xC010:
            break;
        case 0xC014:
            break;
        default:
            break;
        }

        if(absolute == 0xC010) {
            m_prgBank = value;
        } else if(absolute == 0xC014) {
            m_horizontalMirroring = (value & 0x01) != 0;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B16K>() - 1), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = static_cast<uint16_t>((static_cast<uint16_t>(m_chrHigh[slot]) << 8) | m_chrLow[slot]);

        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = static_cast<uint16_t>((static_cast<uint16_t>(m_chrHigh[slot]) << 8) | m_chrLow[slot]);
        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        memset(m_chrLow, 0, sizeof(m_chrLow));
        memset(m_chrHigh, 0, sizeof(m_chrHigh));
        m_prgBank = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_chrLow, 1, 8);
        s.array(m_chrHigh, 1, 8);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }

private:
    uint8_t m_prgBank = 0;
    bool m_horizontalMirroring = false;
};
