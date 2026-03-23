#pragma once

#include "BaseMapper.h"

// iNES Mapper 226
class Mapper226 : public BaseMapper
{
protected:
    uint8_t m_registers[2] = {0, 0};
    uint8_t m_prgMask = 0;

    virtual uint8_t getPrgPage() const
    {
        return static_cast<uint8_t>((m_registers[0] & 0x1F)
            | ((m_registers[0] & 0x80) >> 2)
            | ((m_registers[1] & 0x01) << 6));
    }

    void updatePrg()
    {
        const uint8_t prgPage = static_cast<uint8_t>(getPrgPage() & m_prgMask);
        if((m_registers[0] & 0x20) != 0) {
            m_prgBankLo = prgPage;
            m_prgBankHi = prgPage;
        }
        else {
            m_prgBankLo = static_cast<uint8_t>(prgPage & 0xFE) & m_prgMask;
            m_prgBankHi = static_cast<uint8_t>(m_prgBankLo + 1) & m_prgMask;
        }
    }

    uint8_t m_prgBankLo = 0;
    uint8_t m_prgBankHi = 1;

public:
    Mapper226(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B8K));
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch(addr & 0x0001) {
        case 0: m_registers[0] = data; break;
        case 1: m_registers[1] = data; break;
        }

        updatePrg();
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBankLo, addr);
        return cd().readPrg<BankSize::B16K>(m_prgBankHi, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return BaseMapper::readChr(addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        BaseMapper::writeChr(addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return (m_registers[0] & 0x40) != 0 ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    void reset() override
    {
        m_registers[0] = 0;
        m_registers[1] = 0;
        m_prgBankLo = 0;
        m_prgBankHi = 1 & m_prgMask;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_registers, 1, 2);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_prgBankLo);
        SERIALIZEDATA(s, m_prgBankHi);
    }
};
