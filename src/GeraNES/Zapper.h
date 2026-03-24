#pragma once

#include <cstdint>
#include <functional>
#include <iostream>

#include "defines.h"
#include "Serialization.h"
#include "IControllerPortDevice.h"

class Zapper : public IControllerPortDevice
{
private:

    const float LUMA_THRESHHOLD = 50;
    const float LUMA_DECAY_PER_SCANLINE = 5;
    const int SENSOR_RADIUS = 1; // 3x3 neighborhood
    const int LIGHT_HOLD_SCANLINES = 4;

    bool m_triggerPressed = false;
    int m_cursorX = -1;
    int m_cursorY = -1;    
    int m_luma = 0;
    int m_lightHoldCounter = 0;

    // Callback provided by the emulator to check brightness
    std::function<float(int x, int y)> m_pixelIsBright;

public:
    Zapper() = default;

    void setCursorPosition(int x, int y) override {
        m_cursorX = x;
        m_cursorY = y;
    }

    bool m_flag = true;

    void setTrigger(bool pressed) override {
        m_triggerPressed = pressed;
    }

    void setPixelChecker(std::function<float(int,int)> func) override {
        m_pixelIsBright = func;
    }

    GERANES_INLINE float sampleLumaAtCursor() const
    {
        if(!m_pixelIsBright) return 0.0f;
        if(m_cursorX < 0 || m_cursorY < 0) return 0.0f;

        float maxLuma = 0.0f;
        for(int dy = -SENSOR_RADIUS; dy <= SENSOR_RADIUS; ++dy) {
            for(int dx = -SENSOR_RADIUS; dx <= SENSOR_RADIUS; ++dx) {
                float luma = m_pixelIsBright(m_cursorX + dx, m_cursorY + dy);
                if(luma > maxLuma) maxLuma = luma;
            }
        }

        return maxLuma;
    }

    uint8_t read(bool /*outputEnabled*/) override
    {
        uint8_t ret = 0x00;
        
        if(m_pixelIsBright) {
            float luma = sampleLumaAtCursor();
            if(luma > m_luma) m_luma = static_cast<int>(luma);
            if(luma > LUMA_THRESHHOLD) m_lightHoldCounter = LIGHT_HOLD_SCANLINES;
        }

        // bit3 (light): 0 = detecting, 1 = no light
        bool lightDetected = (m_luma > LUMA_THRESHHOLD) || (m_lightHoldCounter > 0);

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

    void write(uint8_t /*data*/) override
    {
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_triggerPressed);
        SERIALIZEDATA(s, m_cursorX);
        SERIALIZEDATA(s, m_cursorY);
        SERIALIZEDATA(s, m_luma);
        SERIALIZEDATA(s, m_lightHoldCounter);
    }

    void onScanlineChanged() override
    {
        m_luma -= LUMA_DECAY_PER_SCANLINE;
        if(m_luma < 0) m_luma = 0;
        if(m_lightHoldCounter > 0) --m_lightHoldCounter;
    }
};
