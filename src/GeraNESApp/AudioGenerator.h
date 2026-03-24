#pragma once

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

        m_value = m_sample;       

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

        float newSample = 0.0f;
        float lastSample = m_sample;
        int reads = 0;

        if (counter > 0) {

            while (counter-- > 0) {
                if (!m_buffer.empty()) {
                    newSample += m_buffer.read();
                    ++reads;
                } else {
                    break;
                }
            }

            if (reads > 0) {
                newSample /= static_cast<float>(reads);
            } else {
                newSample = m_sample;
            }

            m_lastSample = m_sample;
            m_sample = newSample;
        }

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

    GERANES_INLINE size_t bufferedCount()
    {
        return m_buffer.size();
    }

};

class ExpansionChannel
{
private:

    uint32_t m_rawSourceRateHz = 1789773;
    uint32_t m_outputSampleRate = 44100;
    bool m_playbackStarted = false;
    double m_consumeRate = 1.0;
    double m_consumeAcc = 0.0;
    uint64_t m_phaseAcc = 0;
    int64_t m_windowWeight = 0;
    int64_t m_windowSampleSumQ = 0;
    static constexpr int64_t Q_SCALE = 1 << 20;
    static constexpr uint32_t PREBUFFER_MS = 3;
    static constexpr uint32_t TARGET_BUFFER_MS = 6;
    CircularBuffer<float> m_buffer { 512, CircularBuffer<float>::GROW };
    float m_lastSample = 0.0f;
    float m_mixWeight = 0.0f;

    uint32_t prebufferSamples() const
    {
        const uint64_t samples =
            (static_cast<uint64_t>(std::max(1u, m_outputSampleRate)) * PREBUFFER_MS + 999ULL) / 1000ULL;
        return static_cast<uint32_t>(std::max<uint64_t>(1ULL, samples));
    }

    uint32_t targetBufferSamples() const
    {
        const uint64_t samples =
            (static_cast<uint64_t>(std::max(1u, m_outputSampleRate)) * TARGET_BUFFER_MS + 999ULL) / 1000ULL;
        return static_cast<uint32_t>(std::max<uint64_t>(1ULL, samples));
    }

public:

    void setRawSourceRateHz(uint32_t rawSourceRateHz)
    {
        m_rawSourceRateHz = std::max<uint32_t>(1u, rawSourceRateHz);
    }

    void init(int sampleRate)
    {
        m_outputSampleRate = std::max(1, sampleRate);
        clearBuffer();
    }

    GERANES_INLINE void add(float sample, float mixWeight)
    {
        m_mixWeight = std::max(0.0f, mixWeight);
        const int64_t sampleQ = static_cast<int64_t>(sample * static_cast<float>(Q_SCALE));
        const uint64_t outRate = static_cast<uint64_t>(std::max(1u, m_outputSampleRate));
        const uint64_t srcRate = static_cast<uint64_t>(std::max(1u, m_rawSourceRateHz));

        m_phaseAcc += outRate;

        if(m_phaseAcc < srcRate) {
            m_windowWeight += static_cast<int64_t>(outRate);
            m_windowSampleSumQ += sampleQ * static_cast<int64_t>(outRate);
            return;
        }

        const uint64_t excess = m_phaseAcc - srcRate;
        const uint64_t inWindowUnits = outRate - excess;

        m_windowWeight += static_cast<int64_t>(inWindowUnits);
        m_windowSampleSumQ += sampleQ * static_cast<int64_t>(inWindowUnits);

        if(m_windowWeight > 0) {
            const float averagedSample = static_cast<float>(
                static_cast<double>(m_windowSampleSumQ) /
                static_cast<double>(m_windowWeight) /
                static_cast<double>(Q_SCALE));
            m_buffer.write(averagedSample);
        }

        m_windowWeight = static_cast<int64_t>(excess);
        m_windowSampleSumQ = sampleQ * static_cast<int64_t>(excess);
        m_phaseAcc = excess;
    }

    GERANES_INLINE void clearBuffer()
    {
        m_buffer.clear();
        m_playbackStarted = false;
        m_consumeRate = 1.0;
        m_consumeAcc = 0.0;
        m_phaseAcc = 0;
        m_windowWeight = 0;
        m_windowSampleSumQ = 0;
        m_lastSample = 0.0f;
        m_mixWeight = 0.0f;
    }

    GERANES_INLINE_HOT float get(float& mixWeight)
    {
        if(!m_playbackStarted && m_buffer.size() >= prebufferSamples()) {
            m_playbackStarted = true;
        }

        if(!m_playbackStarted) {
            mixWeight = 0.0f;
            return 0.0f;
        }

        const double bufferError = static_cast<double>(
            static_cast<int64_t>(m_buffer.size()) -
            static_cast<int64_t>(targetBufferSamples()));
        const double targetConsumeRate = std::clamp(1.0 + bufferError * 0.00005, 0.995, 1.005);
        m_consumeRate += (targetConsumeRate - m_consumeRate) * 0.05;
        m_consumeAcc += m_consumeRate;

        while(m_consumeAcc >= 1.0) {
            if(!m_buffer.empty()) {
                m_lastSample = m_buffer.read();
            }
            m_consumeAcc -= 1.0;
        }

        mixWeight = m_mixWeight;
        return m_lastSample;
    }
};

class SampleDirect
{
public:
    struct SampleDirectInfo
    {
        double period;
        float value;
        uint32_t serial;

        SampleDirectInfo(double p = 0.001, float v = 0.0f, uint32_t s = 0)
            : period(p), value(v), serial(s)
        {
        }
    };

private:

    CircularBuffer<SampleDirectInfo> m_buffer;

    float m_value;
    float m_volume;
    double m_currentPosition;
    double m_inverseSampleRate;

    float m_sample;
    float m_lastSample;

    SampleDirectInfo m_current;
    uint32_t m_serialCounter = 0;
    uint32_t m_lastAddedSerial = 0;


    GERANES_INLINE_HOT int update()
    {
        m_currentPosition += m_inverseSampleRate;
        if(m_current.period <= 0.0) {
            return 0;
        }

        int ret = static_cast<int>(m_currentPosition / m_current.period);
        if(ret > 0) {
            m_currentPosition -= static_cast<double>(ret) * m_current.period;
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
        m_inverseSampleRate = 1.0/static_cast<double>(sampleRate);
  
        m_value = 0.0;
        m_volume = 0.0;
        m_currentPosition = 0.0;

        m_sample = 0;
        m_lastSample = 0;

        m_current = SampleDirectInfo(0.001f, 0.0f, 0);
        m_serialCounter = 0;
        m_lastAddedSerial = 0;

        clearBuffer();
    }

    GERANES_INLINE void add(float period, float sample)
    {
        const uint32_t serial = ++m_serialCounter;
        m_lastAddedSerial = serial;
        m_buffer.write(SampleDirectInfo(period, sample, serial));
    }

    GERANES_INLINE void setLastPeriod(float period)
    {
        if(period <= 0.0f) {
            return;
        }

        // Update the most recently added logical sample, regardless of whether
        // it is still queued or already being played from m_current.
        if(m_current.serial == m_lastAddedSerial) {
            // Keep absolute elapsed time when extending period.
            // Rescaling by normalized phase causes artificial time stretch
            // when this method is called repeatedly for long runs.
            m_current.period = static_cast<double>(period);
            return;
        }

        if(!m_buffer.empty()) {
            SampleDirectInfo& back = m_buffer.peakBack();
            if(back.serial == m_lastAddedSerial) {
                back.period = static_cast<double>(period);
            }
        }
    }

    GERANES_INLINE void clearBuffer()
    {
        m_buffer.clear();
        m_currentPosition = 0.0;
        m_sample = 0;
        m_lastSample = 0;
        m_current = SampleDirectInfo(0.001f, 0.0f, 0);
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

                m_sample += m_current.value/total;
            }
        }

        if(total > 1)
            m_value = m_sample;            
        else
            m_value = cosineInterpolate(m_lastSample,m_sample, static_cast<float>(m_currentPosition/m_current.period));
            
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
