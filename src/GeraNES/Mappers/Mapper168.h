#pragma once

#include "BaseMapper.h"

class Mapper168 : public BaseMapper
{
private:
    uint16_t m_irqCounter = 0;
    bool m_irqFlag = false;

public:
    Mapper168(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() < 0x10000) {
            allocateChrRam(0x10000);
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        switch((addr + 0x8000) & 0xC000) {
        case 0x8000:
            m_prgBank = static_cast<uint8_t>((value >> 6) & 0x03);
            m_chrBankHigh = static_cast<uint8_t>(value & 0x0F);
            break;
        case 0xC000:
            m_irqCounter = 1024;
            m_irqFlag = false;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B16K>() - 1), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t bank = (addr < 0x1000) ? 0 : m_chrBankHigh;
        return readChrRam<BankSize::B4K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        const uint8_t bank = (addr < 0x1000) ? 0 : m_chrBankHigh;
        writeChrRam<BankSize::B4K>(bank, addr, data);
    }

    GERANES_HOT void cycle() override
    {
        if(m_irqCounter == 0) return;
        --m_irqCounter;
        if(m_irqCounter == 0) {
            m_irqCounter = 1024;
            m_irqFlag = true;
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBankHigh = 0;
        m_irqCounter = 0;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBankHigh);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqFlag);
    }

private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBankHigh = 0;
};
