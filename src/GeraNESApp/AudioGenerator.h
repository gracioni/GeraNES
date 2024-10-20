#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#define _USE_MATH_DEFINES
#include <cmath>

#include "GeraNES/defines.h"
#include "GeraNES/util/CircularBuffer.h"

static GERANES_INLINE float linearInterpolation(float v0, float v1, float t)
{
    return v0 + (v1-v0)*t;
}

static GERANES_INLINE float cosineInterpolate(float v0,float v1, float t)
{
   float t2 = (1-cos(t*M_PI))/2;
   return linearInterpolation(v0, v1, t2);
}

class IWave
{
protected:

    float m_value;
    float m_period;
    float m_nextPeriod;

    float m_volume;
    float m_nextVolume;

    float m_currentPosition;

    float m_inverseSampleRate;

public:

    IWave()
    {
        init(1);
    }

    virtual ~IWave()
    {
    }

    //return true when parameter can be changed (zero crossing)
    GERANES_INLINE_HOT int update()
    {       
        m_currentPosition += m_inverseSampleRate;

        int ret = m_currentPosition/m_period;

        if(m_currentPosition >= m_period)
        {            
            m_volume = m_nextVolume;
            m_period = m_nextPeriod;

            m_currentPosition = fmod(m_currentPosition, m_period);

            if(m_volume < 0.01)
            {
                m_currentPosition = 0.0;
                m_volume = 0.0;
            }

            //return 1;
        }

        //return 0;
        return ret;
    }

    GERANES_INLINE void setFrequency(float f)
    {
        m_nextPeriod = 1.0/f;
    }

    GERANES_INLINE void setVolume(float v)
    {
        m_nextVolume = v;
    }

    virtual void init(int sampleRate)
    {
        m_inverseSampleRate = 1.0/sampleRate;

        m_value = 0.0;
        m_period = 0.001;
        m_nextPeriod = m_period;

        m_volume = 0.0;
        m_nextVolume = m_volume;

        m_currentPosition = 0.0;
    }    

    virtual float get() = 0;
};

/*
class SinWave : public IChannel
{
public:

    GERANES_INLINE_HOT float get()
    {
        m_value = sin(2 * M_PI * m_frequency *  m_currentPosition);
        m_value *= m_volume;

        update();

        return m_value;
    }
};
*/

class PulseWave : public IWave
{
private:

    float m_duty;
    float m_nextDuty;

public:

    GERANES_INLINE void setDuty(float d)
    {
        m_nextDuty = d;
    }

    void init(int sampleRate) override
    {
        IWave::init(sampleRate);
        m_duty = m_nextDuty = 0.5;
    }

    GERANES_INLINE_HOT float get() override
    {
        if( m_currentPosition <= m_duty*m_period ) m_value = m_volume;
        else m_value = -m_volume;

        if(update()) m_duty = m_nextDuty;

        return m_value;
    }

};

class TriangleWave : public IWave
{
public:

    GERANES_INLINE_HOT float get()
    {
        if( m_currentPosition <= 0.5*m_period )
            m_value = -1.0 + (m_currentPosition/(0.5*m_period));
        else
            m_value = 1.0 - ((m_currentPosition-0.5*m_period)/(0.5*m_period));

        m_value *= m_volume;

        update();

        return m_value;
    }

};

class NoiseWave : public IWave
{
private:

    bool m_metallic;
    uint16_t m_shift; //15 bits

    float m_sample;
    float m_lastSample;

public:

    void init(int sampleRate) override
    {
        IWave::init(sampleRate);
        m_metallic = false;
        m_shift = 1; //15 bits
        m_sample = 0;
        m_lastSample = 0;
    }

    GERANES_INLINE_HOT float get() override
    {
        int counter = update();

        int total = counter;

        if(counter > 0) {

            m_lastSample = m_sample;
            m_sample = 0;

            while(counter-- > 0) {
                
                //https://wiki.nesdev.com/w/index.php/APU_Noise

                bool feedback = (m_shift&1) ^ (m_metallic ? (m_shift>>6)&1 : (m_shift>>1)&1);
                m_shift >>= 1;
                if(feedback) m_shift |= 0x4000;

                m_sample += (m_shift&1 ? 1 : 0);
            }

            m_sample /= total;    
        }        

        // if(total > 1)
            m_value = m_sample;            
        // else
        //     m_value = cosineInterpolate(m_lastSample, m_sample, m_currentPosition/m_period);
            

        m_value *= m_volume;    

        return m_value;
    }

    void setMetallic(bool state)
    {
        m_metallic = state;
    }

};

class SampleWave : public IWave
{
private:

    CircularBuffer<float> m_buffer;
    float m_sample;
    float m_lastSample;

public:

    SampleWave() : m_buffer(256,CircularBuffer<float>::GROW)
    {
    }

    void init(int sampleRate) override
    {
        IWave::init(sampleRate); 
        m_sample = 0;
        m_lastSample = 0;
        clearBuffer();
    }

    GERANES_INLINE_HOT float get() override
    {
        int counter = update();

        int total = counter;

        if(counter > 0) {

            m_lastSample = m_sample;
            m_sample = 0;

            while(counter-- > 0)
            {
                if(!m_buffer.empty()){
                    m_sample += m_buffer.read()/total;
                } 
            }
        }

        if(total > 1)
            m_value = m_sample;
        else
            m_value = cosineInterpolate(m_lastSample, m_sample, m_currentPosition/m_period);

        
        m_value *= m_volume;

        return m_value;
    }

    GERANES_INLINE void add(float sample)
    {
        m_buffer.write(sample);
    }

    GERANES_INLINE void clearBuffer()
    {
        m_buffer.clear();
        m_currentPosition = 0;
        m_sample = 0;
        m_lastSample = 0;
    }

};

class SampleDirect
{
public:
    typedef std::pair<float,float> SampleDirectInfo; //period,value

private:

    CircularBuffer<SampleDirectInfo> m_buffer;

    float m_value;
    float m_volume;
    float m_currentPosition;
    float m_inverseSampleRate;

    float m_sample;
    float m_lastSample;

    SampleDirectInfo m_current;


    GERANES_INLINE_HOT int update()
    {
        int ret = 0;

        m_currentPosition += m_inverseSampleRate;

        while(m_currentPosition >= m_current.first)
        {
            m_currentPosition -= m_current.first;
            ret++;
        }

        return ret;
    }

public:

    SampleDirect() : m_buffer(256,CircularBuffer<SampleDirectInfo>::GROW)
    {
        init(1);
    }

    GERANES_INLINE void init(int sampleRate)
    {
        m_inverseSampleRate = 1.0/sampleRate;
  
        m_value = 0.0;
        m_volume = 0.0;
        m_currentPosition = 0.0;

        m_sample = 0;
        m_lastSample = 0;

        m_current = SampleDirectInfo(0.001, 0);

        clearBuffer();
    }

    GERANES_INLINE void add(float period, float sample)
    {
        m_buffer.write(SampleDirectInfo(period, sample));
    }

    GERANES_INLINE void clearBuffer()
    {
        m_buffer.clear();
        m_currentPosition = 0.0;
        m_sample = 0;
        m_lastSample = 0;
    }

    GERANES_INLINE void setVolume(float volume)
    {
        m_volume = volume * 2.5f; //empirical
    }

    GERANES_INLINE_HOT float get()
    {      
        int counter = update();

        int total = counter;

        if(counter > 0) {

            m_lastSample = m_sample;
            m_sample = 0;

            while(counter-- > 0)
            {
                if(!m_buffer.empty()) {
                    m_current = m_buffer.read();
                }
                
                m_sample += m_current.second/total;            
            }            
        }

        if(total > 1)
            m_value = m_sample;            
        else
            m_value = cosineInterpolate(m_lastSample,m_sample, m_currentPosition/m_current.first);
            
        m_value *= m_volume;

        return m_value;
    }
};

class SignalProcess {

public:

    virtual float apply(float value) {
        return 0;
    }

    ~SignalProcess(){}
};

class Filter : public SignalProcess {

protected:

    int m_sampleRate = 0;
    float m_cutoff = 0.0f;

public:

    Filter(int sampleRate, float cutoff) {
        init(sampleRate, cutoff);
    }

    float GetCutoff() {
        return m_cutoff;
    }

    virtual void init(int sampleRate, float cutoffFrequency) {
        m_cutoff = cutoffFrequency;
        m_sampleRate = sampleRate;
    }

    ~Filter() {
    }

};

class FirstOrderHighPassFilter : public Filter {

private:

    float m_prevInput = 0;
    float m_prevOutput = 0;
    float m_alpha = 0;

public:

    FirstOrderHighPassFilter() : Filter(0,0){}

    FirstOrderHighPassFilter(int sampleRate, float cutoff) : Filter(sampleRate,cutoff) {
    }

    virtual void init(int sampleRate, float cutoffFrequency) override{
        Filter::init(sampleRate, cutoffFrequency);
        float RC = 1.0 / (2 * M_PI * cutoffFrequency);
        m_alpha = RC / (RC + 1.0 / sampleRate);
    }

    float apply(float input) override {

        float output = m_alpha * (m_prevOutput + input - m_prevInput);

        m_prevInput = input;
        m_prevOutput = output;

        return output;
    }

    ~FirstOrderHighPassFilter(){}
};

class FirstOrderLowPassFilter : public Filter {

private:

    float m_prevOutput = 0;
    float m_alpha = 0;

public:

    FirstOrderLowPassFilter() : Filter(0,0){}

    FirstOrderLowPassFilter(int sampleRate, float cutoff) : Filter(sampleRate,cutoff) {
    }

    virtual void init(int sampleRate, float cutoffFrequency) override{
        Filter::init(sampleRate, cutoffFrequency);
        float RC = 1.0 / (2 * M_PI * cutoffFrequency);
        m_alpha = 1.0 / (1.0 + RC * sampleRate);
    }

    float apply(float input) override {

        float output = m_alpha * input + (1 - m_alpha) * m_prevOutput;

        m_prevOutput = output;

        return output;
    }

    ~FirstOrderLowPassFilter(){}
};

#endif