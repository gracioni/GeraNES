#pragma once

#include <string>
#include <cstdint>

#include "../../Serialization.h"

class NsfExpansionAudioBase
{
public:
    virtual ~NsfExpansionAudioBase() = default;

    virtual void reset(int cpuClockHz) = 0;
    virtual void clock() = 0;
    virtual bool handlesRegister(uint16_t cpuAddr) const = 0;
    virtual uint8_t readRegister(uint16_t cpuAddr, uint8_t openBusData) = 0;
    virtual void writeRegister(uint16_t cpuAddr, uint8_t data) = 0;
    virtual float getSample() const = 0;
    virtual std::string getAudioChannelsJson() const = 0;
    virtual bool setAudioChannelVolumeById(const std::string& id, float volume) = 0;
    virtual void serialization(SerializationBase& s) = 0;
};
