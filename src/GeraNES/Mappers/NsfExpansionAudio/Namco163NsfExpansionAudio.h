#pragma once

#include "../Audio/Namco163Audio.h"
#include "NsfExpansionAudioBase.h"

class Namco163NsfExpansionAudio : public NsfExpansionAudioBase
{
private:
    Namco163Audio m_audio;

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
        return cpuAddr == 0x4800 || cpuAddr == 0xE000 || cpuAddr == 0xF800;
    }

    uint8_t readRegister(uint16_t cpuAddr, uint8_t openBusData) override
    {
        if(cpuAddr == 0x4800) {
            return m_audio.readData();
        }
        return openBusData;
    }

    void writeRegister(uint16_t cpuAddr, uint8_t data) override
    {
        switch(cpuAddr) {
        case 0x4800:
            m_audio.writeData(data);
            break;
        case 0xE000:
            m_audio.setDisabled((data & 0x40) != 0);
            break;
        case 0xF800:
            m_audio.setAddressControl(data);
            break;
        }
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
