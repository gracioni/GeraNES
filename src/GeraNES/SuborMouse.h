#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "IControllerPortDevice.h"

class SuborMouse : public IControllerPortDevice
{
private:
    bool m_strobe = false;
    bool m_leftButton = false;
    bool m_rightButton = false;
    int m_accumDx = 0;
    int m_accumDy = 0;
    uint32_t m_report = 0;
    uint8_t m_bitsRemaining = 0;

    void refreshReport()
    {
        const int dx = std::clamp(m_accumDx, -127, 127);
        const int dy = std::clamp(m_accumDy, -127, 127);
        m_accumDx -= dx;
        m_accumDy -= dy;

        const bool right = dx > 0;
        const bool left = dx < 0;
        const bool down = dy > 0;
        const bool up = dy < 0;
        const uint32_t absDx = static_cast<uint32_t>(std::abs(dx)) & 0x7F;
        const uint32_t absDy = static_cast<uint32_t>(std::abs(dy)) & 0x7F;
        const bool extended = absDx > 1 || absDy > 1;

        uint32_t report = 0;
        if(m_leftButton) report |= (1u << 23);
        if(m_rightButton) report |= (1u << 22);
        if(extended) report |= (1u << 21);
        if(up) report |= (1u << 19);
        if(down) report |= (1u << 18);
        if(left) report |= (1u << 17);
        if(right) report |= (1u << 16);
        if(extended) {
            report |= (absDx << 8);
            report |= absDy;
        }

        m_report = report;
        m_bitsRemaining = 24;
    }

public:
    uint8_t read(bool outputEnabled) override
    {
        if(m_strobe) {
            refreshReport();
        }

        const uint8_t output = static_cast<uint8_t>((m_report >> 23) & 0x01);
        if(outputEnabled && m_bitsRemaining > 0) {
            m_report <<= 1;
            --m_bitsRemaining;
        }
        return output;
    }

    void write(uint8_t data) override
    {
        const bool nextStrobe = (data & 0x01) != 0;
        if(m_strobe && !nextStrobe) {
            refreshReport();
        }
        m_strobe = nextStrobe;
    }

    void addRelativeMotion(int dx, int dy) override
    {
        m_accumDx = std::clamp(m_accumDx + dx, -1024, 1024);
        m_accumDy = std::clamp(m_accumDy + dy, -1024, 1024);
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
        SERIALIZEDATA(s, m_accumDx);
        SERIALIZEDATA(s, m_accumDy);
        SERIALIZEDATA(s, m_report);
        SERIALIZEDATA(s, m_bitsRemaining);
    }
};
