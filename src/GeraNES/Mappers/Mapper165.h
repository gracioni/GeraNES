#pragma once

#include "Mapper004.h"

class Mapper165 : public Mapper004
{
private:
    bool m_chrLatch[2] = {false, false};
    bool m_needUpdate = true;

    GERANES_INLINE void updateChrState()
    {
        for(int half = 0; half < 2; ++half) {
            const int regIndex = (half == 0) ? (m_chrLatch[0] ? 1 : 0) : (m_chrLatch[1] ? 4 : 2);
            const uint8_t page = m_chrReg[regIndex];
            m_chrPage[half] = page;
        }
        m_needUpdate = false;
    }

public:
    Mapper165(ICartridgeData& cd) : Mapper004(cd)
    {
        if(cd.chrRamSize() < 0x1000) {
            allocateChrRam(0x1000);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(m_needUpdate) updateChrState();

        const uint8_t half = static_cast<uint8_t>((addr >> 12) & 0x01);
        const uint8_t page = m_chrPage[half];

        if(page == 0) {
            return readChrRam<BankSize::B4K>(0, addr);
        }
        return cd().readChr<BankSize::B4K>(page >> 2, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(m_needUpdate) updateChrState();

        const uint8_t half = static_cast<uint8_t>((addr >> 12) & 0x01);
        const uint8_t page = m_chrPage[half];

        if(page == 0) {
            writeChrRam<BankSize::B4K>(0, addr, data);
        }
    }

    GERANES_HOT void onPpuRead(uint16_t addr) override
    {
        if(m_needUpdate) updateChrState();

        switch(addr & 0x2FF8) {
        case 0x0FD0:
        case 0x1FD0:
            m_chrLatch[(addr >> 12) & 0x01] = false;
            m_needUpdate = true;
            break;
        case 0x0FE8:
        case 0x1FE8:
            m_chrLatch[(addr >> 12) & 0x01] = true;
            m_needUpdate = true;
            break;
        default:
            break;
        }
    }

    void reset() override
    {
        Mapper004::reset();
        m_chrLatch[0] = false;
        m_chrLatch[1] = false;
        m_needUpdate = true;
        m_chrPage[0] = 0;
        m_chrPage[1] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_chrLatch[0]);
        SERIALIZEDATA(s, m_chrLatch[1]);
        SERIALIZEDATA(s, m_needUpdate);
        SERIALIZEDATA(s, m_chrPage[0]);
        SERIALIZEDATA(s, m_chrPage[1]);
    }

private:
    uint8_t m_chrPage[2] = {0, 0};
};
