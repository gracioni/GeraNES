#pragma once

#include "BaseMapper.h"

class Mapper163 : public BaseMapper
{
private:
    uint8_t m_regs[5] = {0};
    bool m_toggle = true;
    bool m_autoSwitchChr = false;
    bool m_forceBank3 = false;
    uint8_t m_chrLatch = 0;
    uint16_t m_lastPpuAddr = 0;

    GERANES_INLINE uint8_t currentPrgBank() const
    {
        return static_cast<uint8_t>((m_regs[0] & 0x0F) | ((m_regs[2] & 0x0F) << 4));
    }

    void updateState()
    {
        m_autoSwitchChr = (m_regs[0] & 0x80) != 0;
        m_forceBank3 = (m_regs[1] == 6);
    }

public:
    Mapper163(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) {
            allocateChrRam(static_cast<int>(BankSize::B8K));
        }
        reset();
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        const uint16_t absoluteAddr = static_cast<uint16_t>(addr + 0x4000);
        if(absoluteAddr == 0x5101) {
            if(m_regs[4] != 0 && value == 0) {
                m_toggle = !m_toggle;
            }
            m_regs[4] = value;
            return;
        }

        if(absoluteAddr == 0x5100 && value == 6) {
            m_regs[1] = value;
            m_forceBank3 = true;
            return;
        }

        switch(absoluteAddr & 0x7300) {
        case 0x5000:
            m_regs[0] = value;
            updateState();
            break;
        case 0x5100:
            m_regs[1] = value;
            updateState();
            break;
        case 0x5200:
            m_regs[2] = value;
            updateState();
            break;
        case 0x5300:
            m_regs[3] = value;
            break;
        default:
            break;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t /*openBusData*/) override
    {
        const uint16_t absoluteAddr = static_cast<uint16_t>(addr + 0x4000);

        switch(absoluteAddr & 0x7700) {
        case 0x5100:
            return static_cast<uint8_t>(m_regs[3] | m_regs[1] | m_regs[0] | static_cast<uint8_t>(m_regs[2] ^ 0xFF));
        case 0x5500:
            return m_toggle ? static_cast<uint8_t>(m_regs[3] | m_regs[0]) : 0;
        default:
            return 4;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t bank = m_forceBank3 ? 3 : currentPrgBank();
        return cd().readPrg<BankSize::B32K>(bank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(!hasChrRam()) {
            return cd().readChr<BankSize::B8K>(0, addr);
        }

        const uint8_t bank4k = m_autoSwitchChr ? m_chrLatch : static_cast<uint8_t>((addr >> 12) & 0x01);
        return readChrRam<BankSize::B4K>(bank4k, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t bank4k = m_autoSwitchChr ? m_chrLatch : static_cast<uint8_t>((addr >> 12) & 0x01);
        writeChrRam<BankSize::B4K>(bank4k, addr, data);
    }

    GERANES_HOT void onPpuRead(uint16_t addr) override
    {
        if(m_autoSwitchChr && (m_lastPpuAddr & 0x2000) == 0 && (addr & 0x2000) != 0) {
            m_chrLatch = static_cast<uint8_t>((addr >> 9) & 0x01);
        }
        m_lastPpuAddr = addr;
    }

    void reset() override
    {
        m_regs[0] = 0;
        m_regs[1] = 0;
        m_regs[2] = 0;
        m_regs[3] = 0;
        m_regs[4] = 0;
        m_toggle = true;
        m_autoSwitchChr = false;
        m_forceBank3 = false;
        m_chrLatch = 0;
        m_lastPpuAddr = 0;
        updateState();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_regs, 1, 5);
        SERIALIZEDATA(s, m_toggle);
        SERIALIZEDATA(s, m_autoSwitchChr);
        SERIALIZEDATA(s, m_forceBank3);
        SERIALIZEDATA(s, m_chrLatch);
        SERIALIZEDATA(s, m_lastPpuAddr);
    }
};
