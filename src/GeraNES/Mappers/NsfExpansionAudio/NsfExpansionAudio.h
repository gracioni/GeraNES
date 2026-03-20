#pragma once

#include <string>

#include "../../Serialization.h"

class NsfExpansionAudio
{
public:
    virtual ~NsfExpansionAudio() = default;

    virtual void reset(int cpuClockHz) = 0;
    virtual void clock() = 0;
    virtual bool handlesRegister(int addr) const = 0;
    virtual uint8_t readRegister(int addr, uint8_t openBusData) = 0;
    virtual void writeRegister(int addr, uint8_t data) = 0;
    virtual float getSample() const = 0;
    virtual std::string getAudioChannelsJson() const = 0;
    virtual bool setAudioChannelVolumeById(const std::string& id, float volume) = 0;
    virtual void serialization(SerializationBase& s) = 0;
};
