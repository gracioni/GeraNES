#pragma once

#include "BaseMapper.h"

// iNES Mapper 221
class Mapper221 : public BaseMapper
{
private:
    uint16_t m_mode = 0;
    uint8_t m_prgReg = 0;
    uint8_t m_prg16Mask = 0;

public:
    Mapper221(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prg16Mask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        switch(absolute & 0xC000) {
        case 0x8000:
            m_mode = absolute;
            break;
        case 0xC000:
            m_prgReg = static_cast<uint8_t>(absolute & 0x0007);
            break;
        default:
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t outerBank = static_cast<uint8_t>((m_mode & 0x00FC) >> 2);

        if((m_mode & 0x0002) != 0) {
            if((m_mode & 0x0100) != 0) {
                if(addr < 0x4000) return cd().readPrg<BankSize::B16K>((outerBank | m_prgReg) & m_prg16Mask, addr);
                return cd().readPrg<BankSize::B16K>((outerBank | 0x07) & m_prg16Mask, addr);
            }

            const uint8_t bank = static_cast<uint8_t>((outerBank | (m_prgReg & 0x06)) & m_prg16Mask & 0xFE);
            if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(bank, addr);
            return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>(bank + 1), addr);
        }

        return cd().readPrg<BankSize::B16K>((outerBank | m_prgReg) & m_prg16Mask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return (m_mode & 0x0001) != 0 ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_mode = 0;
        m_prgReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_prg16Mask);
    }
};
