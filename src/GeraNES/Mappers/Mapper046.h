#pragma once

#include "BaseMapper.h"

class Mapper046 : public BaseMapper
{
private:
    uint8_t m_regs[2] = {0, 0};
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

    void updateState()
    {
        m_prgBank = static_cast<uint8_t>((((m_regs[0] & 0x0F) << 1) | (m_regs[1] & 0x01)) & m_prgMask);
        m_chrBank = static_cast<uint8_t>((((m_regs[0] & 0xF0) >> 1) | ((m_regs[1] & 0x70) >> 4)) & m_chrMask);
    }

public:
    Mapper046(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t data) override
    {
        m_regs[0] = data;
        updateState();
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_regs[1] = data;
        updateState();
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    void reset() override
    {
        m_regs[0] = 0;
        m_regs[1] = 0;
        updateState();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_regs, 1, 2);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
    }
};
