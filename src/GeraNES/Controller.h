#pragma once

#include "defines.h"

#include "Serialization.h"

class Controller
{
private:

    uint8_t m_register;
    uint8_t m_load;
    bool m_strobe;

public:

    Controller()
    {
        m_load = 0;
        m_register = 0;
        m_strobe = false;
    }

    uint8_t read(bool outputEnabled)
    {
        if(m_strobe) {
            // While strobe is high, reads always return current A button state.
            return (m_load & 1) ? 0x01 : 0x00;
        }

        uint8_t ret = (m_register & 1) ? 0x01 : 0x00;

        if(outputEnabled) {
            m_register >>= 1;
            m_register |= 0x80; //shift 1's into register
        }

        return ret;
    }

    void write(uint8_t data)
    {
        m_strobe = (data & 0x01) != 0;
        if(m_strobe) m_register = m_load;
    }

    void setButtonsStatus(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_load = 0;
        m_load |= bA ? 1 : 0;
        m_load |= bB ? (1 << 1) : 0;        
        m_load |= bSelect ? (1 << 2) : 0;
        m_load |= bStart ? (1 << 3) : 0;
        m_load |= bUp ? (1 << 4) : 0;
        m_load |= (bDown && !bUp) ? (1 << 5) : 0;
        m_load |= (bLeft && !bRight) ? (1 << 6) : 0;
        m_load |= bRight ? (1 << 7) : 0;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_register);
        SERIALIZEDATA(s, m_load);
        SERIALIZEDATA(s, m_strobe);
    }

};
