#pragma once

#include <cstdint>
#include <functional>

#include "defines.h"
#include "Serialization.h"

// Bandai Hyper Shot (Famicom expansion port)
// - $4016 read: serial buttons on bit1
// - $4017 read: light sensor (bit3) + fire (bit4)
class BandaiHyperShot
{
private:
    static constexpr float LUMA_THRESHOLD = 50.0f;
    static constexpr int LUMA_DECAY_PER_SCANLINE = 5;

    uint8_t m_register = 0;
    uint8_t m_load = 0;

    bool m_triggerPressed = false;
    int m_cursorX = -1;
    int m_cursorY = -1;
    int m_luma = 0;

    std::function<float(int x, int y)> m_pixelIsBright;

public:
    BandaiHyperShot() = default;

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

    void setCursorPosition(int x, int y)
    {
        m_cursorX = x;
        m_cursorY = y;
    }

    void setTrigger(bool pressed)
    {
        m_triggerPressed = pressed;
    }

    void setPixelChecker(std::function<float(int, int)> func)
    {
        m_pixelIsBright = std::move(func);
    }

    void write4016(uint8_t data)
    {
        if(data & 0x01) m_register = m_load;
    }

    uint8_t read4016(bool outputEnabled)
    {
        uint8_t ret = (m_register & 1) ? 0x02 : 0x00;

        if(outputEnabled) {
            m_register >>= 1;
            m_register |= 0x80; // shift 1's
        }

        return ret;
    }

    uint8_t read4017()
    {
        uint8_t ret = 0x00;

        if(m_pixelIsBright) {
            const float luma = m_pixelIsBright(m_cursorX, m_cursorY);
            if(luma > m_luma) m_luma = static_cast<int>(luma);
        }

        // bit3 (light): 0 = light detected, 1 = no light
        const bool lightDetected = m_luma > LUMA_THRESHOLD;
        if(!lightDetected) ret |= (1 << 3);

        // bit4 (fire): 1 = pressed
        if(m_triggerPressed) ret |= (1 << 4);

        return ret;
    }

    void onScanlineChanged()
    {
        m_luma -= LUMA_DECAY_PER_SCANLINE;
        if(m_luma < 0) m_luma = 0;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_register);
        SERIALIZEDATA(s, m_load);
        SERIALIZEDATA(s, m_triggerPressed);
        SERIALIZEDATA(s, m_cursorX);
        SERIALIZEDATA(s, m_cursorY);
        SERIALIZEDATA(s, m_luma);
    }
};

