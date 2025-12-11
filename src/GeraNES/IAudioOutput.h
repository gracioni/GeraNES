#pragma once

#include <stdint.h>

class IAudioOutput
{
    public:

    enum class Channel { Pulse_1, Pulse_2, Triangle, Noise, Sample };
    enum class PulseChannel { Pulse_1, Pulse_2 };

    virtual bool init(){return true;}
    virtual void render(uint32_t dt, bool silenceFlag){}

    virtual void setChannelVolume(Channel, float) {}
    virtual void setChannelFrequency(Channel, float) {}

    virtual void setPulseDutyCycle(PulseChannel, float){}
    virtual void setNoiseMetallic(bool /*state*/){}
    virtual void addSample(float /*sample*/){}
    virtual void addSampleDirect(float /*period*/, float /*sample*/){}

    virtual ~IAudioOutput(){}
};

class DummyAudioOutput : public IAudioOutput
{

public:

    static DummyAudioOutput& instance()
    {
        static DummyAudioOutput ret;
        return ret;
    }
};
