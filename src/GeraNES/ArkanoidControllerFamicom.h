#pragma once

#include <algorithm>
#include <cstdint>

#include "IExpansionDevice.h"

class ArkanoidControllerFamicom : public IExpansionDevice
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
    void write4016(uint8_t data) override
    {
        if(data & 0x01) {
            refreshStateBuffer();
        }
    }

    uint8_t read4016(bool /*outputEnabled*/) override
    {
        return m_buttonPressed ? static_cast<uint8_t>(0x02) : 0x00;
    }

    uint8_t read4017() override
    {
        const uint8_t output = static_cast<uint8_t>(((~m_stateBuffer) >> 6) & 0x02);
        m_stateBuffer <<= 1;
        return output;
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
