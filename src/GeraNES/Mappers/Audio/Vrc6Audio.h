#pragma once

#include <algorithm>
#include <cstdint>
#include <sstream>

#include "../../Serialization.h"

class Vrc6Audio
{
private:
    uint8_t m_pulseRegs[2][3] = {{0, 0, 0}, {0, 0, 0}};
    uint16_t m_pulseTimer[2] = {0, 0};
    uint8_t m_pulseStep[2] = {0, 0};

    uint8_t m_sawRegs[3] = {0, 0, 0};
    uint16_t m_sawTimer = 0;
    uint8_t m_sawPhase = 0;
    uint8_t m_sawAccumulator = 0;

    float m_sample = 0.0f;
    float m_volPulse1 = 1.0f;
    float m_volPulse2 = 1.0f;
    float m_volSaw = 1.0f;

    uint16_t pulsePeriod(int ch) const
    {
        return static_cast<uint16_t>(
            static_cast<uint16_t>(m_pulseRegs[ch][1]) |
            (static_cast<uint16_t>(m_pulseRegs[ch][2] & 0x0F) << 8));
    }

    bool pulseEnabled(int ch) const
    {
        return (m_pulseRegs[ch][2] & 0x80) != 0;
    }

    float pulseOutput(int ch) const
    {
        if(!pulseEnabled(ch)) return 0.0f;

        const int volume = static_cast<int>(m_pulseRegs[ch][0] & 0x0F);
        if(volume == 0) return 0.0f;

        const bool gateMode = (m_pulseRegs[ch][0] & 0x80) != 0;
        const uint8_t duty = static_cast<uint8_t>((m_pulseRegs[ch][0] >> 4) & 0x07);

        bool high = gateMode;
        if(!gateMode) {
            high = m_pulseStep[ch] <= duty;
        }

        const float sample = high ? (static_cast<float>(volume) / 15.0f) : 0.0f;
        return sample * 2.0f - 1.0f;
    }

    uint16_t sawPeriod() const
    {
        return static_cast<uint16_t>(
            static_cast<uint16_t>(m_sawRegs[1]) |
            (static_cast<uint16_t>(m_sawRegs[2] & 0x0F) << 8));
    }

    bool sawEnabled() const
    {
        return (m_sawRegs[2] & 0x80) != 0;
    }

    float sawOutput() const
    {
        if(!sawEnabled()) return 0.0f;
        const float out = static_cast<float>(m_sawAccumulator & 0x1F) / 31.0f;
        return out * 2.0f - 1.0f;
    }

    void updateSample()
    {
        const float p1 = pulseOutput(0) * m_volPulse1;
        const float p2 = pulseOutput(1) * m_volPulse2;
        const float saw = sawOutput() * m_volSaw;

        m_sample = std::clamp((p1 + p2 + saw) / 3.0f, -1.0f, 1.0f);
    }

public:
    void reset()
    {
        m_pulseRegs[0][0] = m_pulseRegs[0][1] = m_pulseRegs[0][2] = 0;
        m_pulseRegs[1][0] = m_pulseRegs[1][1] = m_pulseRegs[1][2] = 0;
        m_pulseTimer[0] = m_pulseTimer[1] = 0;
        m_pulseStep[0] = m_pulseStep[1] = 0;
        m_sawRegs[0] = m_sawRegs[1] = m_sawRegs[2] = 0;
        m_sawTimer = 0;
        m_sawPhase = 0;
        m_sawAccumulator = 0;
        m_sample = 0.0f;
        m_volPulse1 = 1.0f;
        m_volPulse2 = 1.0f;
        m_volSaw = 1.0f;
    }

    void clock()
    {
        for(int ch = 0; ch < 2; ++ch) {
            if(m_pulseTimer[ch] == 0) {
                m_pulseTimer[ch] = static_cast<uint16_t>(pulsePeriod(ch) + 1);
                m_pulseStep[ch] = static_cast<uint8_t>((m_pulseStep[ch] + 1) & 0x0F);
            }
            else {
                --m_pulseTimer[ch];
            }
        }

        if(m_sawTimer == 0) {
            m_sawTimer = static_cast<uint16_t>(sawPeriod() + 1);
            if(sawEnabled()) {
                m_sawPhase = static_cast<uint8_t>((m_sawPhase + 1) % 14);
                if(m_sawPhase == 0) {
                    m_sawAccumulator = 0;
                }
                else if((m_sawPhase & 0x01) == 0) {
                    const uint8_t rate = static_cast<uint8_t>(m_sawRegs[0] & 0x3F);
                    m_sawAccumulator = static_cast<uint8_t>((m_sawAccumulator + rate) & 0xFF);
                }
            }
            else {
                m_sawPhase = 0;
                m_sawAccumulator = 0;
            }
        }
        else {
            --m_sawTimer;
        }

        updateSample();
    }

    void writeRegister(int addr, uint8_t data)
    {
        switch(addr) {
        case 0x1000:
        case 0x1001:
        case 0x1002:
            m_pulseRegs[0][addr - 0x1000] = data;
            break;
        case 0x2000:
        case 0x2001:
        case 0x2002:
            m_pulseRegs[1][addr - 0x2000] = data;
            break;
        case 0x3000:
        case 0x3001:
        case 0x3002:
            m_sawRegs[addr - 0x3000] = data;
            break;
        }
    }

    float getSample() const
    {
        return m_sample;
    }

    float getMixWeight() const
    {
        return 3.0f;
    }

    std::string getAudioChannelsJson() const
    {
        std::ostringstream ss;
        ss << "{\"channels\":["
           << "{\"id\":\"vrc6.pulse1\",\"label\":\"VRC6 Pulse 1\",\"volume\":" << m_volPulse1 << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"vrc6.pulse2\",\"label\":\"VRC6 Pulse 2\",\"volume\":" << m_volPulse2 << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"vrc6.saw\",\"label\":\"VRC6 Saw\",\"volume\":" << m_volSaw << ",\"min\":0.0,\"max\":1.0}"
           << "]}";
        return ss.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume)
    {
        const float v = std::clamp(volume, 0.0f, 1.0f);
        if(id == "vrc6.pulse1") { m_volPulse1 = v; return true; }
        if(id == "vrc6.pulse2") { m_volPulse2 = v; return true; }
        if(id == "vrc6.saw") { m_volSaw = v; return true; }
        return false;
    }

    void serialization(SerializationBase& s)
    {
        s.array(reinterpret_cast<uint8_t*>(m_pulseRegs), 1, static_cast<int>(sizeof(m_pulseRegs)));
        s.array(reinterpret_cast<uint8_t*>(m_pulseTimer), sizeof(uint16_t), 2);
        s.array(reinterpret_cast<uint8_t*>(m_pulseStep), 1, 2);
        s.array(m_sawRegs, 1, 3);
        SERIALIZEDATA(s, m_sawTimer);
        SERIALIZEDATA(s, m_sawPhase);
        SERIALIZEDATA(s, m_sawAccumulator);
        SERIALIZEDATA(s, m_sample);
        SERIALIZEDATA(s, m_volPulse1);
        SERIALIZEDATA(s, m_volPulse2);
        SERIALIZEDATA(s, m_volSaw);
    }
};
