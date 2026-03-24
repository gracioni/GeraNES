#pragma once

#include <algorithm>
#include <cstdint>

#include "IControllerPortDevice.h"

class ArkanoidControllerNes : public IControllerPortDevice
{
private:
    static constexpr uint32_t MIN_VALUE = 0x54;
    static constexpr uint32_t MAX_VALUE = 0xF4;

    bool m_buttonPressed = false;
    float m_positionNormalized = 0.5f;
    uint32_t m_currentValue = (MAX_VALUE - MIN_VALUE) / 2 + MIN_VALUE;
    uint32_t m_stateBuffer = m_currentValue;

    void refreshStateBuffer()
    {
        const float pos = std::clamp(m_positionNormalized, 0.0f, 1.0f);
        const uint32_t range = MAX_VALUE - MIN_VALUE;
        m_currentValue = MIN_VALUE + static_cast<uint32_t>(pos * range);
        m_stateBuffer = m_currentValue;
    }

public:
    uint8_t read(bool outputEnabled) override
    {
        uint8_t output = ((~m_stateBuffer) >> 3) & 0x10;
        if(outputEnabled) {
            m_stateBuffer <<= 1;
        }

        if(m_buttonPressed) {
            output |= 0x08;
        }

        return output;
    }

    void write(uint8_t data) override
    {
        if(data & 0x01) {
            refreshStateBuffer();
        }
    }

    void setPositionNormalized(float position) override
    {
        m_positionNormalized = std::clamp(position, 0.0f, 1.0f);
    }

    void setTrigger(bool pressed) override
    {
        m_buttonPressed = pressed;
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_buttonPressed);
        SERIALIZEDATA(s, m_positionNormalized);
        SERIALIZEDATA(s, m_currentValue);
        SERIALIZEDATA(s, m_stateBuffer);
    }
};
