#pragma once

#include "BaseMapper.h"

// iNES Mapper 252
// Waixing board with 8 x 1KB CHR banks, 2 switchable 8KB PRG banks, and VRC-style IRQs.
class Mapper252 : public BaseMapper
{
private:
    static constexpr int16_t PRESCALER_RELOAD = 341;
    static constexpr int16_t PRESCALER_DEC = 3;

    uint16_t m_chrReg[8] = {0};
    uint8_t m_prgReg[2] = {0};

    uint8_t m_prgMask = 0;
    int m_chr1kBanks = 0;

    bool m_interruptFlag = false;
    uint8_t m_irqCounter = 0;
    uint8_t m_irqReload = 0;
    bool m_irqMode = false;
    bool m_irqEnable = false;
    bool m_irqEnableOnAck = false;
    int16_t m_prescaler = 0;

public:
    Mapper252(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chr1kBanks = hasChrRam()
            ? static_cast<int>(cd.chrRamSize() / static_cast<int>(BankSize::B1K))
            : cd.numberOfCHRBanks<BankSize::B1K>();
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute <= 0x8FFF) {
            m_prgReg[0] = data & m_prgMask;
            return;
        }

        if(absolute >= 0xA000 && absolute <= 0xAFFF) {
            m_prgReg[1] = data & m_prgMask;
            return;
        }

        if(absolute >= 0xB000 && absolute <= 0xEFFF) {
            const uint8_t shift = static_cast<uint8_t>(absolute & 0x0004);
            const uint8_t bank = static_cast<uint8_t>(((((absolute - 0xB000) >> 1) & 0x1800) | ((absolute << 7) & 0x0400)) / 0x0400);
            m_chrReg[bank] = static_cast<uint16_t>((m_chrReg[bank] & (0xF0 >> shift)) | ((static_cast<uint16_t>(data & 0x0F)) << shift));
            return;
        }

        switch(absolute & 0xF00C) {
        case 0xF000:
            m_irqReload = static_cast<uint8_t>((m_irqReload & 0xF0) | (data & 0x0F));
            break;
        case 0xF004:
            m_irqReload = static_cast<uint8_t>((m_irqReload & 0x0F) | ((data & 0x0F) << 4));
            break;
        case 0xF008:
            m_irqMode = (data & 0x04) != 0;
            m_irqEnable = (data & 0x02) != 0;
            m_irqEnableOnAck = (data & 0x01) != 0;
            m_interruptFlag = false;
            if(m_irqEnable) {
                m_irqCounter = m_irqReload;
                m_prescaler = PRESCALER_RELOAD;
            }
            break;
        case 0xF00C:
            m_interruptFlag = false;
            m_irqEnable = m_irqEnableOnAck;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg[0], addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1], addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = m_chr1kBanks > 0 ? static_cast<uint16_t>(m_chrReg[slot] % m_chr1kBanks) : 0;

        if(hasChrRam()) {
            return readChrRam<BankSize::B1K>(bank, addr);
        }

        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = m_chr1kBanks > 0 ? static_cast<uint16_t>(m_chrReg[slot] % m_chr1kBanks) : 0;
        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT void cycle() override
    {
        if(!m_irqEnable) return;

        if(!m_irqMode) {
            if(m_prescaler > 0) {
                m_prescaler -= PRESCALER_DEC;
                return;
            }

            m_prescaler += PRESCALER_RELOAD;
        }

        if(m_irqCounter != 0xFF) {
            ++m_irqCounter;
            return;
        }

        m_irqCounter = m_irqReload;
        m_interruptFlag = true;
    }

    void reset() override
    {
        memset(m_chrReg, 0, sizeof(m_chrReg));
        m_prgReg[0] = 0;
        m_prgReg[1] = 0;
        m_interruptFlag = false;
        m_irqCounter = 0;
        m_irqReload = 0;
        m_irqMode = false;
        m_irqEnable = false;
        m_irqEnableOnAck = false;
        m_prescaler = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(reinterpret_cast<uint8_t*>(m_chrReg), 2, 8);
        s.array(m_prgReg, 1, 2);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chr1kBanks);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqReload);
        SERIALIZEDATA(s, m_irqMode);
        SERIALIZEDATA(s, m_irqEnable);
        SERIALIZEDATA(s, m_irqEnableOnAck);
        SERIALIZEDATA(s, m_prescaler);
    }
};
