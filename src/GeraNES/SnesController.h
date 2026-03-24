#pragma once

#include <cstdint>

#include "Serialization.h"
#include "IControllerPortDevice.h"

class SnesController : public IControllerPortDevice
{
private:
    uint16_t m_register = 0xFFFF;
    uint16_t m_load = 0xFFFF;
    bool m_strobe = false;

public:
    uint8_t read(bool outputEnabled) override
    {
        if(m_strobe) {
            return (m_load & 0x01) ? 0x01 : 0x00;
        }

        const uint8_t ret = (m_register & 0x01) ? 0x01 : 0x00;
        if(outputEnabled) {
            m_register >>= 1;
            m_register |= 0x8000;
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
            m_register = m_load;
        }
    }

    void setButtonsStatus(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight) override
    {
        setButtonsStatusExtended(bA, bB, bSelect, bStart, bUp, bDown, bLeft, bRight, false, false, false, false);
    }

    void setButtonsStatusExtended(bool bA, bool bB, bool bSelect, bool bStart,
                                  bool bUp, bool bDown, bool bLeft, bool bRight,
                                  bool bX, bool bY, bool bL, bool bR) override
    {
        m_load = 0;
        m_load |= bB ? 1 : 0;
        m_load |= bY ? (1 << 1) : 0;
        m_load |= bSelect ? (1 << 2) : 0;
        m_load |= bStart ? (1 << 3) : 0;
        m_load |= bUp ? (1 << 4) : 0;
        m_load |= (bDown && !bUp) ? (1 << 5) : 0;
        m_load |= (bLeft && !bRight) ? (1 << 6) : 0;
        m_load |= bRight ? (1 << 7) : 0;
        m_load |= bA ? (1 << 8) : 0;
        m_load |= bX ? (1 << 9) : 0;
        m_load |= bL ? (1 << 10) : 0;
        m_load |= bR ? (1 << 11) : 0;
        m_load |= 0xF000;
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_register);
        SERIALIZEDATA(s, m_load);
        SERIALIZEDATA(s, m_strobe);
    }
};
