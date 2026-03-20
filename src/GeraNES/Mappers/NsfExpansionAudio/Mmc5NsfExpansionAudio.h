#pragma once

#include <array>
#include <algorithm>
#include <sstream>

#include "../Audio/Mmc5Audio.h"
#include "NsfExpansionAudioBase.h"

class Mmc5NsfExpansionAudio : public NsfExpansionAudioBase
{
private:
    static constexpr uint16_t AUDIO_REG_START = 0x5000;
    static constexpr uint16_t AUDIO_REG_END = 0x5015;

    Mmc5Audio m_audio;
    std::array<uint8_t, 0x3F6> m_ram = {};
    uint8_t m_mulA = 0xFF;
    uint8_t m_mulB = 0xFF;

public:
    void reset(int cpuClockHz) override
    {
        m_audio.reset(cpuClockHz);
        m_ram.fill(0);
        m_mulA = 0xFF;
        m_mulB = 0xFF;
    }

    void clock() override
    {
        m_audio.clock();
    }

    bool handlesRegister(uint16_t cpuAddr) const override
    {
        return (cpuAddr >= AUDIO_REG_START && cpuAddr <= AUDIO_REG_END) ||
               cpuAddr == 0x5205 ||
               cpuAddr == 0x5206 ||
               (cpuAddr >= 0x5C00 && cpuAddr <= 0x5FF5);
    }

    uint8_t readRegister(uint16_t cpuAddr, uint8_t openBusData) override
    {
        switch(cpuAddr) {
        case 0x5015:
            return m_audio.readStatus();
        case 0x5205:
            return static_cast<uint8_t>((m_mulA * m_mulB) & 0xFF);
        case 0x5206:
            return static_cast<uint8_t>(((m_mulA * m_mulB) >> 8) & 0xFF);
        default:
            if(cpuAddr >= 0x5C00 && cpuAddr <= 0x5FF5) {
                return m_ram[static_cast<size_t>(cpuAddr - 0x5C00)];
            }
            return openBusData;
        }
    }

    void writeRegister(uint16_t cpuAddr, uint8_t data) override
    {
        if(cpuAddr >= AUDIO_REG_START && cpuAddr <= AUDIO_REG_END) {
            m_audio.writeRegister(cpuAddr, data);
            return;
        }

        switch(cpuAddr) {
        case 0x5205:
            m_mulA = data;
            return;
        case 0x5206:
            m_mulB = data;
            return;
        default:
            if(cpuAddr >= 0x5C00 && cpuAddr <= 0x5FF5) {
                m_ram[static_cast<size_t>(cpuAddr - 0x5C00)] = data;
            }
            return;
        }
    }

    float getSample() const override
    {
        return m_audio.getSample();
    }

    float getMixWeight() const override
    {
        return m_audio.getMixWeight();
    }

    float getOutputGain() const override
    {
        return 1.0f;
    }

    std::string getAudioChannelsJson() const override
    {
        std::ostringstream ss;
        ss << "{\"channels\":["
           << "{\"id\":\"mmc5.pulse1\",\"label\":\"MMC5 Pulse 1\",\"volume\":" << m_audio.pulse1Volume() << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"mmc5.pulse2\",\"label\":\"MMC5 Pulse 2\",\"volume\":" << m_audio.pulse2Volume() << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"mmc5.pcm\",\"label\":\"MMC5 PCM\",\"volume\":" << m_audio.pcmVolume() << ",\"min\":0.0,\"max\":1.0}"
           << "]}";
        return ss.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        const float v = std::clamp(volume, 0.0f, 1.0f);
        if(id == "mmc5.pulse1") { m_audio.setPulse1Volume(v); return true; }
        if(id == "mmc5.pulse2") { m_audio.setPulse2Volume(v); return true; }
        if(id == "mmc5.pcm") { m_audio.setPcmVolume(v); return true; }
        return false;
    }

    void serialization(SerializationBase& s) override
    {
        s.array(m_ram.data(), 1, static_cast<int>(m_ram.size()));
        SERIALIZEDATA(s, m_mulA);
        SERIALIZEDATA(s, m_mulB);
        m_audio.serialization(s);
    }
};
