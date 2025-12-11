#pragma once

#include <stdint.h>

class IAudioOutput
{
    public:

    enum class Channel { Square_1, Square_2, Triangle, Noise, Sample };
    enum class SquareChannel { Square_1, Square_2 };

    virtual bool init(){return true;}
    virtual void render(uint32_t dt, bool silenceFlag){}

    virtual void setChannelVolume(Channel, float) {}
    virtual void setChannelFrequency(Channel, float) {}

    virtual void setSquareDutyCycle(SquareChannel, float){}
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
