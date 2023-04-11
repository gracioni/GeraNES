#ifndef IAUDIOOUTPUT_H
#define IAUDIOOUTPUT_H

#include <stdint.h>

class IAudioOutput
{
    public:

    virtual bool init(void) {return true;}
    virtual void render(float dt, float volume){}
    virtual void setSquare1Frequency(float /*f*/){}
    virtual void setSquare1DutyCycle(float /*d*/){}
    virtual void setSquare1Volume(float /*v*/){}
    virtual void setSquare2Frequency(float /*f*/){}
    virtual void setSquare2DutyCycle(float /*d*/){}
    virtual void setSquare2Volume(float /*v*/){}
    virtual void setTriangleFrequency(float /*f*/){}
    virtual void setTriangleVolume(float /*v*/){}
    virtual void setNoiseFrequency(float /*f*/){}
    virtual void setNoiseMetallic(bool /*state*/){}
    virtual void setNoiseVolume(float /*v*/){}
    virtual void setSampleVolume(float /*v*/){}
    virtual void setSampleFrequency(float /*f*/){}
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

#endif // IAUDIOOUTPUT_H
