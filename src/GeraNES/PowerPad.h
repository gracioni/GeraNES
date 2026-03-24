#pragma once

#include <array>
#include <cstdint>

#include "IControllerPortDevice.h"

class PowerPad : public IControllerPortDevice
{
private:
    std::array<bool, 12> m_pressed = {};
    uint8_t m_stateBufferL = 0xFF;
    uint8_t m_stateBufferH = 0xFF;
    bool m_strobe = false;
    bool m_sideB = false;

    void refreshStateBuffer()
    {
        std::array<uint8_t, 12> pressedKeys = {};
        for(int i = 0; i < 12; ++i) {
            const int sourceIndex = m_sideB ? ((i / 4) * 4 + (3 - (i % 4))) : i;
            pressedKeys[i] = m_pressed[sourceIndex] ? 1 : 0;
        }

        m_stateBufferL = static_cast<uint8_t>(
            pressedKeys[1] |
            (pressedKeys[0] << 1) |
            (pressedKeys[4] << 2) |
            (pressedKeys[8] << 3) |
            (pressedKeys[5] << 4) |
            (pressedKeys[9] << 5) |
            (pressedKeys[10] << 6) |
            (pressedKeys[6] << 7));

        m_stateBufferH = static_cast<uint8_t>(
            pressedKeys[3] |
            (pressedKeys[2] << 1) |
            (pressedKeys[11] << 2) |
            (pressedKeys[7] << 3) |
            0xF0);
    }

public:
    explicit PowerPad(bool sideB) : m_sideB(sideB)
    {
    }

    bool isSideB() const
    {
        return m_sideB;
    }

    uint8_t read(bool outputEnabled) override
    {
        if(m_strobe) {
            refreshStateBuffer();
        }

        uint8_t output = static_cast<uint8_t>(((m_stateBufferH & 0x01) << 4) | ((m_stateBufferL & 0x01) << 3));
        if(outputEnabled) {
            m_stateBufferL >>= 1;
            m_stateBufferH >>= 1;
            m_stateBufferL |= 0x80;
            m_stateBufferH |= 0x80;
        }
        return output;
    }

    void write(uint8_t data) override
    {
        const bool prevStrobe = m_strobe;
        m_strobe = (data & 0x01) != 0;
        if(prevStrobe && !m_strobe) {
            refreshStateBuffer();
        }
    }

    void setPowerPadButtons(const std::array<bool, 12>& buttons) override
    {
        m_pressed = buttons;
    }

    void serialization(SerializationBase& s) override
    {
        for(bool& pressed : m_pressed) {
            SERIALIZEDATA(s, pressed);
        }
        SERIALIZEDATA(s, m_stateBufferL);
        SERIALIZEDATA(s, m_stateBufferH);
        SERIALIZEDATA(s, m_strobe);
        SERIALIZEDATA(s, m_sideB);
    }
};
