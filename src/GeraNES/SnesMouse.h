#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "IControllerPortDevice.h"

class SnesMouse : public IControllerPortDevice
{
private:
    bool m_strobe = false;
    bool m_leftButton = false;
    bool m_rightButton = false;
    uint8_t m_sensitivity = 0;
    int m_accumDx = 0;
    int m_accumDy = 0;
    bool m_lastXNegative = false;
    bool m_lastYNegative = false;
    uint32_t m_shiftRegister = 0xFFFFFFFFu;

    static uint8_t remapMagnitude(uint8_t magnitude, uint8_t sensitivity)
    {
        static constexpr std::array<uint8_t, 8> MEDIUM_TABLE = {0, 1, 2, 3, 8, 10, 12, 21};
        static constexpr std::array<uint8_t, 8> HIGH_TABLE = {0, 1, 4, 9, 12, 20, 24, 28};

        magnitude = static_cast<uint8_t>(std::min<int>(magnitude, 0x7F));
        if(sensitivity == 1) {
            return MEDIUM_TABLE[std::min<size_t>(magnitude, MEDIUM_TABLE.size() - 1)];
        }
        if(sensitivity >= 2) {
            return HIGH_TABLE[std::min<size_t>(magnitude, HIGH_TABLE.size() - 1)];
        }
        return magnitude;
    }

    static int takeDeltaChunk(int& delta)
    {
        if(delta > 0) {
            const int chunk = std::min(delta, 0x7F);
            delta -= chunk;
            return chunk;
        }
        if(delta < 0) {
            const int chunk = std::max(delta, -0x7F);
            delta -= chunk;
            return chunk;
        }
        return 0;
    }

    uint8_t encodeSignedMagnitude(int delta, bool& lastNegative) const
    {
        const bool negative = delta == 0 ? lastNegative : (delta < 0);
        const uint8_t magnitude = remapMagnitude(
            static_cast<uint8_t>(std::min<int>(std::abs(delta), 0x7F)),
            m_sensitivity);
        return static_cast<uint8_t>((negative ? 0x80 : 0x00) | magnitude);
    }

    void latchReport()
    {
        const int dyChunk = takeDeltaChunk(m_accumDy);
        const int dxChunk = takeDeltaChunk(m_accumDx);
        const uint8_t first = 0x00;
        const uint8_t second =
            static_cast<uint8_t>((m_rightButton ? 0x80 : 0x00) |
                                 (m_leftButton ? 0x40 : 0x00) |
                                 ((m_sensitivity & 0x03) << 4) |
                                 0x01);
        const uint8_t third = encodeSignedMagnitude(dyChunk, m_lastYNegative);
        const uint8_t fourth = encodeSignedMagnitude(dxChunk, m_lastXNegative);

        if(dxChunk != 0) m_lastXNegative = (dxChunk < 0);
        if(dyChunk != 0) m_lastYNegative = (dyChunk < 0);

        m_shiftRegister =
            (static_cast<uint32_t>(first) << 24) |
            (static_cast<uint32_t>(second) << 16) |
            (static_cast<uint32_t>(third) << 8) |
            static_cast<uint32_t>(fourth);
    }

    void cycleSensitivity()
    {
        m_sensitivity = static_cast<uint8_t>((m_sensitivity + 1) % 3);
    }

public:
    uint8_t read(bool outputEnabled) override
    {
        if(m_strobe) {
            if(outputEnabled) {
                cycleSensitivity();
            }
            return 0x00;
        }

        const uint8_t output = static_cast<uint8_t>((m_shiftRegister & 0x80000000u) ? 0x01 : 0x00);
        if(outputEnabled) {
            m_shiftRegister <<= 1;
            m_shiftRegister |= 0x00000001u;
        }
        return output;
    }

    void write(uint8_t data) override
    {
        const bool nextStrobe = (data & 0x01) != 0;
        if(m_strobe && !nextStrobe) {
            latchReport();
        }
        m_strobe = nextStrobe;
    }

    void addRelativeMotion(int dx, int dy) override
    {
        m_accumDx = std::clamp(m_accumDx + dx, -0x7FFF, 0x7FFF);
        m_accumDy = std::clamp(m_accumDy + dy, -0x7FFF, 0x7FFF);
    }

    void setTrigger(bool pressed) override
    {
        m_leftButton = pressed;
    }

    void setSecondaryTrigger(bool pressed) override
    {
        m_rightButton = pressed;
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_strobe);
        SERIALIZEDATA(s, m_leftButton);
        SERIALIZEDATA(s, m_rightButton);
        SERIALIZEDATA(s, m_sensitivity);
        SERIALIZEDATA(s, m_accumDx);
        SERIALIZEDATA(s, m_accumDy);
        SERIALIZEDATA(s, m_lastXNegative);
        SERIALIZEDATA(s, m_lastYNegative);
        SERIALIZEDATA(s, m_shiftRegister);
    }
};
