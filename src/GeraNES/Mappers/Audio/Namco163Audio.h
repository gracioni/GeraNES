#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

#include "../../Serialization.h"

class Namco163Audio
{
public:
    static constexpr int SOUND_RAM_SIZE = 128;

private:
    uint8_t m_soundRamAddress = 0;
    bool m_soundAutoIncrement = false;
    uint8_t m_soundRAM[SOUND_RAM_SIZE] = {0};
    bool m_soundDisable = false;

    int m_audioClockDiv = 15;
    int m_audioClockCounter = 0;
    float m_sample = 0.0f;
    float m_prev = 0.0f;
    float m_target = 0.0f;
    float m_filtered = 0.0f;
    uint32_t m_audioPhaseRemainder[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float m_audioChannelVol[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    void incrementSoundRAMAddress()
    {
        if(m_soundAutoIncrement) m_soundRamAddress++;
        m_soundRamAddress &= SOUND_RAM_SIZE - 1;
    }

    int firstActiveChannel() const
    {
        const int c = static_cast<int>((m_soundRAM[0x7F] >> 4) & 0x07);
        return std::clamp(7 - c, 0, 7);
    }

    int activeChannelCount() const
    {
        return 8 - firstActiveChannel();
    }

    uint8_t readWaveNibble(uint8_t nibbleIndex) const
    {
        const uint8_t byteVal = m_soundRAM[(nibbleIndex >> 1) & (SOUND_RAM_SIZE - 1)];
        return (nibbleIndex & 0x01) ? static_cast<uint8_t>((byteVal >> 4) & 0x0F)
                                    : static_cast<uint8_t>(byteVal & 0x0F);
    }

    float updateChannelAndGetSampleMixed(int channel, int channels)
    {
        const int base = 0x40 + (channel << 3);

        const uint32_t freq =
            static_cast<uint32_t>(m_soundRAM[base + 0]) |
            (static_cast<uint32_t>(m_soundRAM[base + 2]) << 8) |
            (static_cast<uint32_t>(m_soundRAM[base + 4] & 0x03) << 16);

        uint32_t phase =
            static_cast<uint32_t>(m_soundRAM[base + 1]) |
            (static_cast<uint32_t>(m_soundRAM[base + 3]) << 8) |
            (static_cast<uint32_t>(m_soundRAM[base + 5]) << 16);

        const uint32_t stepNumerator = freq + m_audioPhaseRemainder[channel];
        const uint32_t step = stepNumerator / static_cast<uint32_t>(channels);
        m_audioPhaseRemainder[channel] = stepNumerator % static_cast<uint32_t>(channels);

        phase += step;
        phase &= 0x00FFFFFF;

        const uint32_t waveLen = static_cast<uint32_t>(256 - (m_soundRAM[base + 4] & 0xFC));
        const uint32_t waveSpan = waveLen << 16;
        if(waveSpan != 0) {
            phase %= waveSpan;
        }

        m_soundRAM[base + 1] = static_cast<uint8_t>(phase & 0xFF);
        m_soundRAM[base + 3] = static_cast<uint8_t>((phase >> 8) & 0xFF);
        m_soundRAM[base + 5] = static_cast<uint8_t>((phase >> 16) & 0xFF);

        const uint8_t waveAddr = m_soundRAM[base + 6];
        const uint8_t wavePos = static_cast<uint8_t>((phase >> 16) & 0xFF);
        const uint8_t sampleIndex = static_cast<uint8_t>(waveAddr + wavePos);
        const uint8_t sample4 = readWaveNibble(sampleIndex);
        const int sampleCentered = static_cast<int>(sample4) - 8;
        const int volume = static_cast<int>(m_soundRAM[base + 7] & 0x0F);

        const float out = static_cast<float>(sampleCentered * volume) / 120.0f;
        return out * m_audioChannelVol[channel];
    }

    void tick()
    {
        if(m_soundDisable) {
            m_target = 0.0f;
            return;
        }

        const int first = firstActiveChannel();
        const int channels = std::max(1, activeChannelCount());

        float mix = 0.0f;
        for(int ch = first; ch <= 7; ++ch) {
            mix += updateChannelAndGetSampleMixed(ch, channels);
        }

        const float channelNorm = std::sqrt(static_cast<float>(channels));
        const float raw = std::clamp(mix / std::max(1.0f, channelNorm), -1.0f, 1.0f);
        const float alpha = (channels >= 6) ? 0.18f : 0.30f;
        m_filtered += alpha * (raw - m_filtered);
        m_target = std::clamp(m_filtered, -1.0f, 1.0f);
    }

public:
    void reset()
    {
        m_soundRamAddress = 0;
        m_soundAutoIncrement = false;
        for(uint8_t& v : m_soundRAM) v = 0;
        m_soundDisable = false;
        m_audioClockCounter = 0;
        m_sample = 0.0f;
        m_prev = 0.0f;
        m_target = 0.0f;
        m_filtered = 0.0f;
        for(uint32_t& r : m_audioPhaseRemainder) r = 0;
        for(float& v : m_audioChannelVol) v = 1.0f;
    }

    void setDisabled(bool disabled)
    {
        m_soundDisable = disabled;
    }

    void setAddressControl(uint8_t data)
    {
        m_soundAutoIncrement = (data & 0x80) != 0;
        m_soundRamAddress = data & 0x7F;
    }

    void writeData(uint8_t data)
    {
        m_soundRAM[m_soundRamAddress] = data;
        incrementSoundRAMAddress();
    }

    uint8_t readData()
    {
        const uint8_t ret = m_soundRAM[m_soundRamAddress];
        incrementSoundRAMAddress();
        return ret;
    }

    void clock()
    {
        if(++m_audioClockCounter >= m_audioClockDiv) {
            m_audioClockCounter = 0;
            m_prev = m_target;
            tick();
        }
        const float t = static_cast<float>(m_audioClockCounter + 1) / static_cast<float>(m_audioClockDiv);
        m_sample = m_prev + (m_target - m_prev) * t;
    }

    float getSample() const
    {
        return m_sample;
    }

    float getMixWeight() const
    {
        return m_soundDisable ? 0.0f : 1;
    }

    float getOutputGain() const
    {
        return 1;
    }

    std::string getAudioChannelsJson() const
    {
        std::ostringstream ss;
        ss << "{\"channels\":[";
        for(int i = 0; i < 8; ++i) {
            if(i > 0) ss << ",";
            ss << "{\"id\":\"n163.ch" << (i + 1) << "\",\"label\":\"N163 Ch " << (i + 1)
               << "\",\"volume\":" << m_audioChannelVol[i] << ",\"min\":0.0,\"max\":1.0}";
        }
        ss << "]}";
        return ss.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume)
    {
        const float v = std::clamp(volume, 0.0f, 1.0f);
        for(int i = 0; i < 8; ++i) {
            const std::string chId = std::string("n163.ch") + std::to_string(i + 1);
            if(id == chId) {
                m_audioChannelVol[i] = v;
                return true;
            }
        }
        return false;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_soundRamAddress);
        SERIALIZEDATA(s, m_soundAutoIncrement);
        s.array(m_soundRAM, 1, SOUND_RAM_SIZE);
        SERIALIZEDATA(s, m_soundDisable);
        SERIALIZEDATA(s, m_audioClockDiv);
        SERIALIZEDATA(s, m_audioClockCounter);
        SERIALIZEDATA(s, m_sample);
        SERIALIZEDATA(s, m_prev);
        SERIALIZEDATA(s, m_target);
        SERIALIZEDATA(s, m_filtered);
        s.array(reinterpret_cast<uint8_t*>(m_audioPhaseRemainder), sizeof(uint32_t), 8);
        s.array(reinterpret_cast<uint8_t*>(m_audioChannelVol), sizeof(float), 8);
    }
};
