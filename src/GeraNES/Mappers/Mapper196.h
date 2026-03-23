#pragma once

#include "Mapper004.h"

class Mapper196 : public Mapper004
{
private:
    bool m_outerPrgActive = false;
    uint8_t m_outerPrgReg = 0;
    uint8_t m_addrMode = 0;

    GERANES_INLINE uint8_t outerPrgBase8k() const
    {
        return static_cast<uint8_t>((((m_outerPrgReg & 0x0F) | (m_outerPrgReg >> 4)) & 0x0F) * 4) & m_prgMask;
    }

    GERANES_INLINE uint8_t remapLowBit(uint16_t absoluteAddr) const
    {
        switch(m_addrMode) {
        case 1:
            return static_cast<uint8_t>((absoluteAddr >> 1) & 0x01);
        case 2:
            return static_cast<uint8_t>((absoluteAddr >> 2) & 0x01);
        default:
            if(absoluteAddr >= 0xC000) {
                return static_cast<uint8_t>(((absoluteAddr >> 2) & 0x01) | ((absoluteAddr >> 3) & 0x01));
            }
            return static_cast<uint8_t>(((absoluteAddr >> 1) & 0x01) | ((absoluteAddr >> 2) & 0x01) | ((absoluteAddr >> 3) & 0x01));
        }
    }

public:
    Mapper196(ICartridgeData& cd) : Mapper004(cd)
    {
        m_addrMode = static_cast<uint8_t>(cd.subMapperId());
        if(m_addrMode > 3) m_addrMode = 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_outerPrgActive) {
            const uint8_t base = outerPrgBase8k();
            const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
            return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(base + slot) & m_prgMask, addr);
        }
        return Mapper004::readPrg(addr);
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        uint16_t absoluteAddr = static_cast<uint16_t>(addr + 0x8000);
        absoluteAddr = static_cast<uint16_t>((absoluteAddr & 0xFFFE) | remapLowBit(absoluteAddr));
        Mapper004::writePrg(static_cast<int>(absoluteAddr - 0x8000), data);
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t data) override
    {
        m_outerPrgReg = data;
        if(m_addrMode == 0 || m_addrMode == 3) {
            m_outerPrgActive = true;
        }
    }

    void reset() override
    {
        Mapper004::reset();
        m_outerPrgReg = 0;
        m_outerPrgActive = (m_addrMode == 3);
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_outerPrgActive);
        SERIALIZEDATA(s, m_outerPrgReg);
        SERIALIZEDATA(s, m_addrMode);
    }
};
