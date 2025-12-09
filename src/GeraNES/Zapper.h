#pragma once

#include <cstdint>
#include <functional>
#include <iostream>

#include "defines.h"
#include "Serialization.h"

class Zapper
{
private:

    const float LUMA_THRESHHOLD = 200;
    const float LUMA_DECAY_PER_SCANLINE = 5;

    bool m_triggerPressed = false;
    int m_cursorX = -1;
    int m_cursorY = -1;    
    int m_luma = 0;

    // Callback provided by the emulator to check brightness
    std::function<int(int x, int y)> m_pixelIsBright;

public:
    Zapper() = default;

    void setCursorPosition(int x, int y) {
        m_cursorX = x;
        m_cursorY = y;
    }

    bool m_flag = true;

    void setTrigger(bool pressed) {
        m_triggerPressed = pressed;
    }

    void setPixelChecker(std::function<float(int,int)> func) {
        m_pixelIsBright = func;
    }

    uint8_t read()
    {
        uint8_t ret = 0x00;
        
        if (m_pixelIsBright) {
            float luma = m_pixelIsBright(m_cursorX, m_cursorY);
            if(luma > m_luma) m_luma = luma;
        }

        // bit3 (light): 0 = detecting, 1 = no light
        bool lightDetected = m_luma > LUMA_THRESHHOLD;

        if (lightDetected)
            ret &= ~(1 << 3);   // bit3 = 0
        else
            ret |=  (1 << 3);   // bit3 = 1

        // bit4 (trigger): 0 = pulled, 1 = released
        if (m_triggerPressed)
            ret &= ~(1 << 4);   // bit4 = 0        
        else
            ret |=  (1 << 4);   // bit4 = 1

        return ret;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_triggerPressed);
        SERIALIZEDATA(s, m_cursorX);
        SERIALIZEDATA(s, m_cursorY);
        SERIALIZEDATA(s, m_luma);
    }

    void onScanlineChanged()
    {
        m_luma -= LUMA_DECAY_PER_SCANLINE;
        if(m_luma < 0) m_luma = 0;
    }
};
