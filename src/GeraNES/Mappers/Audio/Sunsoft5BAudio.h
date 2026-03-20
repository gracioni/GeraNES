#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>

#include "../../Serialization.h"

class Sunsoft5BAudio
{
private:
    uint8_t m_selectedReg = 0;
    uint8_t m_regs[16] = {0};
    int m_audioClockDiv = 8;
    int m_audioClockCounter = 0;

    uint16_t m_toneCounter[3] = {1, 1, 1};
    bool m_toneOutput[3] = {true, true, true};

    uint16_t m_noiseCounter = 1;
    uint32_t m_noiseLfsr = 1;
    bool m_noiseOutput = true;

    uint16_t m_envCounter = 1;
    uint8_t m_envVolume = 0;
    bool m_envAttack = false;
    bool m_envContinue = false;
    bool m_envAlternate = false;
    bool m_envHold = false;
    bool m_envHolding = false;
    int m_envDirection = 1;

    float m_sample = 0.0f;
    float m_volA = 1.0f;
    float m_volB = 1.0f;
    float m_volC = 1.0f;

    static constexpr float VOLUME_TABLE[16] = {
        0.0f, 0.0045f, 0.0071f, 0.0106f,
        0.0159f, 0.0240f, 0.0345f, 0.0500f,
        0.0709f, 0.1000f, 0.1414f, 0.1995f,
        0.2818f, 0.3981f, 0.5623f, 1.0f
    };

    uint16_t tonePeriod(int ch) const
    {
        const uint16_t p = static_cast<uint16_t>(m_regs[ch * 2] | ((m_regs[ch * 2 + 1] & 0x0F) << 8));
        return (p == 0) ? 1 : p;
    }

    uint16_t noisePeriod() const
    {
        const uint16_t p = static_cast<uint16_t>(m_regs[6] & 0x1F);
        return (p == 0) ? 1 : p;
    }

    uint16_t envPeriod() const
    {
        const uint16_t p = static_cast<uint16_t>(m_regs[11] | (m_regs[12] << 8));
        return (p == 0) ? 1 : p;
    }

    void restartEnvelope()
    {
        const uint8_t shape = m_regs[13] & 0x0F;
        m_envContinue = (shape & 0x08) != 0;
        m_envAttack = (shape & 0x04) != 0;
        m_envAlternate = (shape & 0x02) != 0;
        m_envHold = (shape & 0x01) != 0;
        m_envHolding = false;
        m_envCounter = envPeriod();
        m_envVolume = m_envAttack ? 0 : 15;
        m_envDirection = m_envAttack ? 1 : -1;
    }

    void tickEnvelope()
    {
        if(m_envHolding) return;

        if(--m_envCounter > 0) return;
        m_envCounter = envPeriod();

        int next = static_cast<int>(m_envVolume) + m_envDirection;
        if(next >= 0 && next <= 15) {
            m_envVolume = static_cast<uint8_t>(next);
            return;
        }

        if(!m_envContinue) {
            m_envVolume = 0;
            m_envHolding = true;
            return;
        }

        if(m_envHold) {
            m_envVolume = m_envAttack ? 15 : 0;
            m_envHolding = true;
            return;
        }

        if(m_envAlternate) {
            m_envDirection = -m_envDirection;
            m_envAttack = !m_envAttack;
        }

        m_envVolume = m_envAttack ? 0 : 15;
    }

    void tick()
    {
        for(int ch = 0; ch < 3; ++ch) {
            if(--m_toneCounter[ch] == 0) {
                m_toneCounter[ch] = tonePeriod(ch);
                m_toneOutput[ch] = !m_toneOutput[ch];
            }
        }

        if(--m_noiseCounter == 0) {
            m_noiseCounter = noisePeriod();
            const uint32_t feedback = (m_noiseLfsr ^ (m_noiseLfsr >> 3)) & 0x01;
            m_noiseLfsr = (m_noiseLfsr >> 1) | (feedback << 16);
            m_noiseOutput = (m_noiseLfsr & 0x01) != 0;
        }

        tickEnvelope();

        const uint8_t mixer = m_regs[7];

        float mix = 0.0f;
        const float gains[3] = {m_volA, m_volB, m_volC};
        for(int ch = 0; ch < 3; ++ch) {
            const bool toneDisabled = ((mixer >> ch) & 0x01) != 0;
            const bool noiseDisabled = ((mixer >> (ch + 3)) & 0x01) != 0;
            const bool toneOk = toneDisabled || m_toneOutput[ch];
            const bool noiseOk = noiseDisabled || m_noiseOutput;
            if(!(toneOk && noiseOk)) continue;

            const uint8_t volReg = m_regs[8 + ch];
            const uint8_t volIndex = ((volReg & 0x10) != 0) ? m_envVolume : (volReg & 0x0F);
            mix += VOLUME_TABLE[volIndex] * gains[ch];
        }

        m_sample = (mix / 3.0f) * 2.0f - 1.0f;
        m_sample = std::clamp(m_sample, -1.0f, 1.0f);
    }

public:
    void reset()
    {
        m_selectedReg = 0;
        memset(m_regs, 0, sizeof(m_regs));
        m_audioClockCounter = 0;
        m_toneCounter[0] = m_toneCounter[1] = m_toneCounter[2] = 1;
        m_toneOutput[0] = m_toneOutput[1] = m_toneOutput[2] = true;
        m_noiseCounter = 1;
        m_noiseLfsr = 1;
        m_noiseOutput = true;
        restartEnvelope();
        m_sample = 0.0f;
        m_volA = 1.0f;
        m_volB = 1.0f;
        m_volC = 1.0f;
    }

    void writeAddress(uint8_t data)
    {
        m_selectedReg = data & 0x0F;
    }

    void writeData(uint8_t data)
    {
        const uint8_t reg = m_selectedReg & 0x0F;
        m_regs[reg] = data;
        if(reg == 13) {
            restartEnvelope();
        }
    }

    void clock()
    {
        if(++m_audioClockCounter >= m_audioClockDiv) {
            m_audioClockCounter = 0;
            tick();
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
        std::ostringstream os;
        os << "{\"channels\":["
           << "{\"id\":\"sunsoft5b.a\",\"label\":\"Sunsoft 5B A\",\"volume\":" << m_volA << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"sunsoft5b.b\",\"label\":\"Sunsoft 5B B\",\"volume\":" << m_volB << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"sunsoft5b.c\",\"label\":\"Sunsoft 5B C\",\"volume\":" << m_volC << ",\"min\":0.0,\"max\":1.0}"
           << "]}";
        return os.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume)
    {
        const float v = std::clamp(volume, 0.0f, 1.0f);
        if(id == "sunsoft5b.a") { m_volA = v; return true; }
        if(id == "sunsoft5b.b") { m_volB = v; return true; }
        if(id == "sunsoft5b.c") { m_volC = v; return true; }
        return false;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_selectedReg);
        s.array(m_regs, 1, 16);
        SERIALIZEDATA(s, m_audioClockDiv);
        SERIALIZEDATA(s, m_audioClockCounter);
        s.array(reinterpret_cast<uint8_t*>(m_toneCounter), sizeof(uint16_t), 3);
        s.array(reinterpret_cast<uint8_t*>(m_toneOutput), 1, 3);
        SERIALIZEDATA(s, m_noiseCounter);
        SERIALIZEDATA(s, m_noiseLfsr);
        SERIALIZEDATA(s, m_noiseOutput);
        SERIALIZEDATA(s, m_envCounter);
        SERIALIZEDATA(s, m_envVolume);
        SERIALIZEDATA(s, m_envAttack);
        SERIALIZEDATA(s, m_envContinue);
        SERIALIZEDATA(s, m_envAlternate);
        SERIALIZEDATA(s, m_envHold);
        SERIALIZEDATA(s, m_envHolding);
        SERIALIZEDATA(s, m_envDirection);
        SERIALIZEDATA(s, m_sample);
        SERIALIZEDATA(s, m_volA);
        SERIALIZEDATA(s, m_volB);
        SERIALIZEDATA(s, m_volC);
    }
};
