#pragma once

#include "BaseMapper.h"

class Mapper028 : public BaseMapper
{
private:
    uint8_t m_selectedReg = 0;
    uint8_t m_reg[4] = {0, 0, 0, 0};
    uint8_t m_mirroringBit = 0;
    uint16_t m_prgMask16k = 0;
    uint8_t m_chrMask8k = 0;

    static uint16_t calculateMask16(int nBanks)
    {
        uint16_t mask = 0;
        int n = nBanks - 1;
        while(n > 0) {
            mask <<= 1;
            mask |= 1;
            n >>= 1;
        }
        return mask;
    }

    GERANES_INLINE uint16_t mapPrg16k(uint16_t bank) const
    {
        return static_cast<uint16_t>(bank & m_prgMask16k);
    }

    void updateState()
    {
        // Mapping is derived directly from the 4 registers during reads.
    }

    GERANES_INLINE uint16_t bankAt16kSlot(int slot) const
    {
        const uint8_t mode = m_reg[2];
        const uint8_t gameSize = static_cast<uint8_t>((mode >> 4) & 0x03);
        const uint8_t prgSize = static_cast<uint8_t>((mode >> 3) & 0x01);
        const uint8_t slotSelect = static_cast<uint8_t>((mode >> 2) & 0x01);
        uint8_t prgSelect = static_cast<uint8_t>(m_reg[1] & 0x0F);
        const uint16_t outerPrgSelect = static_cast<uint16_t>(m_reg[3] << 1);

        if(prgSize == 0) {
            prgSelect <<= 1;
            const uint16_t outerAnd[4] = {0x1FE, 0x1FC, 0x1F8, 0x1F0};
            const uint8_t innerAnd[4] = {0x01, 0x03, 0x07, 0x0F};
            const uint16_t base = static_cast<uint16_t>(outerPrgSelect & outerAnd[gameSize]);
            if(slot == 0) return mapPrg16k(static_cast<uint16_t>(base | (prgSelect & innerAnd[gameSize])));
            return mapPrg16k(static_cast<uint16_t>(base | ((prgSelect | 0x01) & innerAnd[gameSize])));
        }

        const uint8_t variableSlot = slotSelect ? 0 : 1;
        if(slot == variableSlot) {
            switch(gameSize) {
            case 0: return mapPrg16k(static_cast<uint16_t>((outerPrgSelect & 0x1FE) | (prgSelect & 0x01)));
            case 1: return mapPrg16k(static_cast<uint16_t>((outerPrgSelect & 0x1FC) | (prgSelect & 0x03)));
            case 2: return mapPrg16k(static_cast<uint16_t>((outerPrgSelect & 0x1F8) | (prgSelect & 0x07)));
            default: return mapPrg16k(static_cast<uint16_t>((outerPrgSelect & 0x1F0) | (prgSelect & 0x0F)));
            }
        }

        return mapPrg16k(static_cast<uint16_t>((outerPrgSelect & 0x1FE) | slotSelect));
    }

public:
    Mapper028(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask16k = calculateMask16(cd.numberOfPRGBanks<BankSize::B16K>());
        if(cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B32K));
        m_chrMask8k = calculateMask(std::max(1, cd.chrRamSize() > 0 ? cd.chrRamSize() / static_cast<int>(BankSize::B8K) : 4));
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t data) override
    {
        if(addr >= 0x5000 && addr <= 0x5FFF) {
            m_selectedReg = static_cast<uint8_t>(((data & 0x80) >> 6) | (data & 0x01));
        }
        else {
            BaseMapper::writeMapperRegisterAbsolute(addr, data);
        }
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        if(m_selectedReg <= 1) {
            m_mirroringBit = static_cast<uint8_t>((data >> 4) & 0x01);
        }
        else if(m_selectedReg == 2) {
            m_mirroringBit = static_cast<uint8_t>(data & 0x01);
        }

        m_reg[m_selectedReg] = data;
        updateState();
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const int slot = (addr >> 14) & 0x01;
        return cd().readPrg<BankSize::B16K>(bankAt16kSlot(slot), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(m_reg[0] & m_chrMask8k, addr);
        return cd().readChr<BankSize::B8K>(m_reg[0] & m_chrMask8k, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(m_reg[0] & m_chrMask8k, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        uint8_t mirroring = static_cast<uint8_t>(m_reg[2] & 0x03);
        if((mirroring & 0x02) == 0) {
            mirroring = m_mirroringBit;
        }

        switch(mirroring) {
        case 0: return MirroringType::SINGLE_SCREEN_A;
        case 1: return MirroringType::SINGLE_SCREEN_B;
        case 2: return MirroringType::VERTICAL;
        default: return MirroringType::HORIZONTAL;
        }
    }

    void reset() override
    {
        m_selectedReg = 0;
        m_reg[0] = 0;
        m_reg[1] = 0;
        m_reg[2] = 0;
        m_reg[3] = 0;
        m_mirroringBit = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_selectedReg);
        s.array(m_reg, 1, 4);
        SERIALIZEDATA(s, m_mirroringBit);
        SERIALIZEDATA(s, m_prgMask16k);
        SERIALIZEDATA(s, m_chrMask8k);
    }
};
