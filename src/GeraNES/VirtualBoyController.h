#pragma once

#include <cstdint>

#include "IControllerPortDevice.h"
#include "Serialization.h"

class VirtualBoyController : public IControllerPortDevice
{
private:
    uint16_t m_stateBuffer = 0xFFFF;
    uint16_t m_load = 0xFFFF;
    bool m_strobe = false;

public:
    uint8_t read(bool outputEnabled) override
    {
        if(m_strobe) {
            return (m_load & 0x01) ? 0x01 : 0x00;
        }

        const uint8_t ret = (m_stateBuffer & 0x01) ? 0x01 : 0x00;
        if(outputEnabled) {
            m_stateBuffer >>= 1;
            m_stateBuffer |= 0x8000;
        }
        return ret;
    }

    void write(uint8_t data) override
    {
        m_strobe = (data & 0x01) != 0;
    }

    void onCpuGetToPutTransition() override
    {
        if(m_strobe) {
            m_stateBuffer = m_load;
        }
    }

    void setVirtualBoyButtons(bool bA, bool bB, bool bSelect, bool bStart,
                              bool bUp0, bool bDown0, bool bLeft0, bool bRight0,
                              bool bUp1, bool bDown1, bool bLeft1, bool bRight1,
                              bool bL, bool bR) override
    {
        m_load = 0;
        m_load |= bDown1 ? 1 : 0;
        m_load |= bLeft1 ? (1 << 1) : 0;
        m_load |= bSelect ? (1 << 2) : 0;
        m_load |= bStart ? (1 << 3) : 0;
        m_load |= bUp0 ? (1 << 4) : 0;
        m_load |= (bDown0 && !bUp0) ? (1 << 5) : 0;
        m_load |= (bLeft0 && !bRight0) ? (1 << 6) : 0;
        m_load |= bRight0 ? (1 << 7) : 0;
        m_load |= bRight1 ? (1 << 8) : 0;
        m_load |= bUp1 ? (1 << 9) : 0;
        m_load |= bL ? (1 << 10) : 0;
        m_load |= bR ? (1 << 11) : 0;
        m_load |= bB ? (1 << 12) : 0;
        m_load |= bA ? (1 << 13) : 0;
        m_load |= (1 << 14);
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_stateBuffer);
        SERIALIZEDATA(s, m_load);
        SERIALIZEDATA(s, m_strobe);
    }
};
