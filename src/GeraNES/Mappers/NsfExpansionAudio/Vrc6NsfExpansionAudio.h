#pragma once

#include "../Audio/Vrc6Audio.h"
#include "NsfExpansionAudioBase.h"

class Vrc6NsfExpansionAudio : public NsfExpansionAudioBase
{
private:
    Vrc6Audio m_audio;

public:
    void reset(int /*cpuClockHz*/) override
    {
        m_audio.reset();
    }

    void clock() override
    {
        m_audio.clock();
    }

    bool handlesRegister(uint16_t cpuAddr) const override
    {
        switch(cpuAddr) {
        case 0x9000:
        case 0x9001:
        case 0x9002:
        case 0xA000:
        case 0xA001:
        case 0xA002:
        case 0xB000:
        case 0xB001:
        case 0xB002:
            return true;
        default:
            return false;
        }
    }

    uint8_t readRegister(uint16_t /*cpuAddr*/, uint8_t openBusData) override
    {
        return openBusData;
    }

    void writeRegister(uint16_t cpuAddr, uint8_t data) override
    {
        const int relativeAddr = static_cast<int>(cpuAddr & 0xF003);
        m_audio.writeRegister(relativeAddr, data);
    }

    float getSample() const override
    {
        return m_audio.getSample();
    }

    std::string getAudioChannelsJson() const override
    {
        return m_audio.getAudioChannelsJson();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        return m_audio.setAudioChannelVolumeById(id, volume);
    }

    void serialization(SerializationBase& s) override
    {
        m_audio.serialization(s);
    }
};
