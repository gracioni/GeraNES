#pragma once

#include "../Audio/Sunsoft5BAudio.h"
#include "NsfExpansionAudioBase.h"

class Sunsoft5BNsfExpansionAudio : public NsfExpansionAudioBase
{
private:
    Sunsoft5BAudio m_audio;

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
        return cpuAddr == 0xC000 || cpuAddr == 0xE000;
    }

    uint8_t readRegister(uint16_t /*cpuAddr*/, uint8_t openBusData) override
    {
        return openBusData;
    }

    void writeRegister(uint16_t cpuAddr, uint8_t data) override
    {
        if(cpuAddr == 0xC000) {
            m_audio.writeAddress(data);
        }
        else if(cpuAddr == 0xE000) {
            m_audio.writeData(data);
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
