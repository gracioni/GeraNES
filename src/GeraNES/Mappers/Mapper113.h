#pragma once

#include "BaseMapper.h"

// NINA-03/NINA-06 multicart variant (iNES Mapper 113)
class Mapper113 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;

    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0;

    bool m_verticalMirroring = false;

public:
    Mapper113(ICartridgeData& cd) : BaseMapper(cd)
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
        // Full-address decode is (addr & 0xE100) == 0x4100.
        // This core receives addr masked to 0x1FFF, so only A8 is available.
        if((addr & 0x0100) == 0) return;

        m_prgBank = static_cast<uint8_t>((data >> 3) & 0x07) & m_prgMask;
        m_chrBank = static_cast<uint8_t>((data & 0x07) | ((data >> 3) & 0x08)) & m_chrMask;
        m_verticalMirroring = (data & 0x80) != 0;
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

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
        m_verticalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_verticalMirroring);
    }
};
