#pragma once

#include "BaseMapper.h"

class Mapper202 : public BaseMapper
{
private:
    uint16_t m_irqReloadValue = 0;
    uint16_t m_irqCounter = 0;
    uint8_t m_irqControl = 0;
    uint8_t m_selectedReg = 0;
    uint8_t m_prgRegs[4] = {0, 0, 0, 0};
    uint8_t m_chrRegs[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    bool m_useRom = false;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    bool m_irqFlag = false;

public:
    Mapper202(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        } else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        switch(absolute & 0xF000) {
        case 0x8000: m_irqReloadValue = static_cast<uint16_t>((m_irqReloadValue & 0xFFF0) | (data & 0x0F)); break;
        case 0x9000: m_irqReloadValue = static_cast<uint16_t>((m_irqReloadValue & 0xFF0F) | ((data & 0x0F) << 4)); break;
        case 0xA000: m_irqReloadValue = static_cast<uint16_t>((m_irqReloadValue & 0xF0FF) | ((data & 0x0F) << 8)); break;
        case 0xB000: m_irqReloadValue = static_cast<uint16_t>((m_irqReloadValue & 0x0FFF) | ((data & 0x0F) << 12)); break;
        case 0xC000:
            m_irqControl = data;
            if(m_irqControl & 0x02) {
                m_irqCounter = m_irqReloadValue;
            }
            m_irqFlag = false;
            break;
        case 0xD000:
            m_irqFlag = false;
            break;
        case 0xE000:
            m_selectedReg = static_cast<uint8_t>((data & 0x07) - 1);
            break;
        case 0xF000:
            switch(m_selectedReg) {
            case 0:
            case 1:
            case 2:
            case 3:
                m_prgRegs[m_selectedReg] = static_cast<uint8_t>((m_prgRegs[m_selectedReg] & 0x10) | (data & 0x0F));
                break;
            case 4:
                m_useRom = (data & 0x04) != 0;
                break;
            default:
                break;
            }

            if(cd().mapperId() == 56) {
                switch(absolute & 0xFC00) {
                case 0xF000: {
                    const uint8_t bank = static_cast<uint8_t>(absolute & 0x03);
                    m_prgRegs[bank] = static_cast<uint8_t>((data & 0x10) | (m_prgRegs[bank] & 0x0F));
                    break;
                }
                case 0xF800:
                    break;
                case 0xFC00:
                    m_chrRegs[absolute & 0x07] = static_cast<uint8_t>(data & m_chrMask);
                    break;
                }
            }
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        if(slot == 3) {
            return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1), addr);
        }
        return cd().readPrg<BankSize::B8K>(m_prgRegs[slot] & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        if(cd().mapperId() == 56) {
            return cd().readChr<BankSize::B1K>(m_chrRegs[(addr >> 10) & 0x07] & m_chrMask, addr);
        }
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        if(cd().mapperId() == 56) {
            writeChrRam<BankSize::B1K>(m_chrRegs[(addr >> 10) & 0x07] & m_chrMask, addr, data);
        } else {
            BaseMapper::writeChr(addr, data);
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(m_useRom) {
            return cd().readPrg<BankSize::B8K>(m_prgRegs[3] & m_prgMask, addr);
        }
        return BaseMapper::readSaveRam(addr);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(!m_useRom) {
            BaseMapper::writeSaveRam(addr, data);
        }
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        if(cd().mapperId() == 56) {
            return (m_irqControl & 0x01) ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
        }
        return BaseMapper::mirroringType();
    }

    GERANES_HOT void cycle() override
    {
        if(m_irqControl & 0x02) {
            ++m_irqCounter;
            if(m_irqCounter == 0xFFFF) {
                m_irqCounter = m_irqReloadValue;
                m_irqControl &= ~0x02;
                m_irqFlag = true;
            }
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    void reset() override
    {
        m_irqReloadValue = 0;
        m_irqCounter = 0;
        m_irqControl = 0;
        m_selectedReg = 0;
        std::memset(m_prgRegs, 0, sizeof(m_prgRegs));
        std::memset(m_chrRegs, 0, sizeof(m_chrRegs));
        m_useRom = false;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_irqReloadValue);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqControl);
        SERIALIZEDATA(s, m_selectedReg);
        s.array(m_prgRegs, 1, 4);
        s.array(m_chrRegs, 1, 8);
        SERIALIZEDATA(s, m_useRom);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_irqFlag);
    }
};
