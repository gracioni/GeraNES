#pragma once

#include "BaseMapper.h"

class Mapper042 : public BaseMapper
{
private:
    uint8_t m_chrBank = 0;
    uint8_t m_prgReg = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_irqFlag = false;

public:
    Mapper042(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch((addr + 0x8000) & 0xE003) {
        case 0x8000:
            if(!hasChrRam()) {
                m_chrBank = static_cast<uint8_t>(data & 0x0F) & m_chrMask;
            }
            break;
        case 0xE000:
            m_prgReg = static_cast<uint8_t>(data & 0x0F) & m_prgMask;
            break;
        case 0xE001:
            break;
        case 0xE002:
            m_irqEnabled = (data == 0x02);
            if(!m_irqEnabled) {
                m_irqCounter = 0;
                m_irqFlag = false;
            }
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t pageCount = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>());
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(pageCount - 4), addr);
        case 1: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(pageCount - 3), addr);
        case 2: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(pageCount - 2), addr);
        default: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(pageCount - 1), addr);
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgReg & m_prgMask, addr);
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t /*data*/) override
    {
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank & m_chrMask, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return cd().mirroringType();
    }

    GERANES_HOT void cycle() override
    {
        if(m_irqEnabled) {
            ++m_irqCounter;
            if(m_irqCounter >= 0x8000) {
                m_irqCounter -= 0x8000;
            }
            m_irqFlag = (m_irqCounter >= 0x6000);
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    void reset() override
    {
        m_chrBank = 0;
        m_prgReg = 0;
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqFlag);
    }
};
