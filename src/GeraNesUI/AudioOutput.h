#ifndef INCLUDE_AUDIOGENERATOR
#define INCLUDE_AUDIOGENERATOR

#include <SDL.h>

#include <set>
#include <string>
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>

#define _USE_MATH_DEFINES
#include <cmath>

#include <fstream>

#define BUFFER_TIME (0.05) //seconds

#include "GeraNes/IAudioOutput.h"
#include "GeraNes/defines.h"
#include "GeraNes/util/CircularBuffer.h"

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
    GERANES_INLINE_HOT bool update()
    {
        bool ret = false;

        m_currentPosition += m_inverseSampleRate;

        if(m_currentPosition >= m_period)
        {
            m_currentPosition = fmod(m_currentPosition, m_period);
            m_volume = m_nextVolume;
            m_period = m_nextPeriod;

            if(m_volume < 0.01)
            {
                m_currentPosition = 0.0;
                m_volume = 0.0;
            }

            ret = true;
        }

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

    virtual float get(void) = 0;
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

    bool m_flag;
    bool m_metallic;
    uint16_t m_shift; //15 bits
    float m_rand;
    float m_lastRand;

public:

    void init(int sampleRate) override
    {
        IWave::init(sampleRate);

        m_flag = true;
        m_metallic = false;
        m_shift = 1; //15 bits
        m_rand = 0.0;
        m_lastRand = 0.0;
    }

    GERANES_INLINE_HOT float get() override
    {

        if(m_flag)
        {
            //https://wiki.nesdev.com/w/index.php/APU_Noise

            bool bit = m_metallic ? (m_shift&0x40) : (m_shift&0x02);
            bool feedback = (m_shift&1) ^ bit;
            m_shift >>= 1;
            if(feedback) m_shift |= 0x4000;

            m_rand = (static_cast<float>(m_shift)-0x3FFF)/0x7FFF * 2;

            m_flag = false;
        }

        //m_value = m_rand;
        m_value = cosineInterpolate(m_lastRand, m_rand, m_currentPosition/m_period);

        m_value *= m_volume;

        if(update()) {
            m_flag = true;
            if(m_metallic) m_period *= 2;
            m_lastRand = m_rand;
        }


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

    bool m_flag;
    float m_sample;
    float m_lastSample;

public:

    SampleWave() : m_buffer(256,CircularBuffer<float>::GROW)
    {
    }

    void init(int sampleRate) override
    {
        IWave::init(sampleRate);

        m_flag = true;
        m_sample = 0.0;
        m_lastSample = 0.0;

        clearBuffer();
    }

    GERANES_INLINE_HOT float get() override
    {
        if(m_flag)
        {
            if(!m_buffer.empty()){
                while(!m_buffer.empty()){
                    m_sample = m_buffer.read();
                    if(m_currentPosition >= m_period) m_currentPosition -= m_period;
                    else break;
                }
            }
            //else m_sample = 0; //dmc letterbox

            m_flag = false;
        }

        m_value = cosineInterpolate(m_lastSample, m_sample, m_currentPosition/m_period);
        m_value *= m_volume;

        if(update())
        {
            m_flag = true;
            m_lastSample = m_sample;
        }


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
        m_flag = true;
    }

};

class SampleDirect
{
public:
    typedef std::pair<float,float> SampleDirectInfo; //period,value

private:

    CircularBuffer<SampleDirectInfo> m_buffer;

    bool m_flag;
    float m_value;

    float m_sample;
    float m_lastSample;
    float m_volume = 4.0;
    float m_currentPosition;

    float m_inverseSampleRate;

    SampleDirectInfo m_current;


    GERANES_INLINE_HOT bool update()
    {
        m_currentPosition += m_inverseSampleRate;

        if(m_currentPosition >= m_current.first)
        {
            m_currentPosition -= m_current.first;
            return true;
        }

        return false;
    }

public:

    SampleDirect() : m_buffer(256,CircularBuffer<SampleDirectInfo>::GROW)
    {
        init(1);
    }

    GERANES_INLINE void init(int sampleRate)
    {
        m_inverseSampleRate = 1.0/sampleRate;

        m_flag = true;
        m_value = 0.0;

        m_sample = 0.0;
        m_lastSample = 0.0;
        m_volume = 4.0;
        m_currentPosition = 0.0;

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
        m_flag = true;
    }

    GERANES_INLINE_HOT float get()
    {
        //if(m_buffer.empty()) return m_value;

        if(m_flag){

            while(!m_buffer.empty()) {
                m_current = m_buffer.read();
                if(m_currentPosition >= m_current.first) m_currentPosition -= m_current.first;
                else break;
            }
            m_sample = m_current.second;

            m_flag = false;
        }

        m_value = cosineInterpolate(m_lastSample,m_sample, m_currentPosition/m_current.first);
        m_value *= m_volume;

        if(update())
        {
            m_lastSample = m_sample;
            m_flag = true;
        }

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
        float RC = 1.0 / (2 * M_PI * cutoffFrequency); // Calcula o valor do tempo de constante do filtro
        m_alpha = RC / (RC + 1.0 / sampleRate); // Calcula o valor do fator de suavização alpha
    }

    float apply(float input) override {

        float output = m_alpha * (m_prevOutput + input - m_prevInput); // Calcula o valor da saída filtrad

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
        float RC = 1.0 / (2 * M_PI * cutoffFrequency); // Calcula o valor do tempo de constante do filtro
        m_alpha = 1.0 / (1.0 + RC * sampleRate); // Calcula o valor do fator de suavização alpha
    }

    float apply(float input) override {

        float output = m_alpha * input + (1 - m_alpha) * m_prevOutput; // Calcula o valor da saída filtrad

        m_prevOutput = output;

        return output;
    }

    ~FirstOrderLowPassFilter(){}
};


class AudioOutput : public IAudioOutput
{

private:

    SDL_AudioDeviceID m_device = 0;
    std::string m_currentDeviceName = "";

    std::vector<char> m_buffer;
    bool m_playFlag;

    SDL_AudioSpec spec; 

    PulseWave m_pulseWave1;
    PulseWave m_pulseWave2;
    TriangleWave m_triangleWave;
    NoiseWave m_noise;

    SampleWave m_sample;
    SampleDirect m_sampleDirect;

    FirstOrderHighPassFilter m_hpFilter1;
    FirstOrderHighPassFilter m_hpFilter2;
    FirstOrderLowPassFilter m_lpFilter;

    void clearBuffers()
    {
        m_buffer.clear();
        m_sampleDirect.clearBuffer();
        m_sample.clearBuffer();
    }

    void turnOff()
    {
        if(m_device != 0) {
            SDL_PauseAudioDevice(m_device, 1); 
            SDL_CloseAudioDevice(m_device);
            m_device = 0;
            m_currentDeviceName = "";
        }
        clearBuffers();
    }

public:

    AudioOutput()
    {
        SDL_Init(SDL_INIT_AUDIO);

        //m_device = nullptr;
        //m_audioOutput = nullptr;
    }

    ~AudioOutput() override
    {
        //if(m_audioOutput != nullptr) delete m_audioOutput;
        turnOff();
    }

    int sampleRate()
    {
        return spec.freq;
    }

    int sampleSize()
    {
        //the formats hold the number of bits in the first byte
        return spec.format & 0xFF;       
    }

    const std::string& currentDeviceName() {
        return m_currentDeviceName;
    }

    std::vector<std::string> getAudioList() {

        std::vector<std::string> ret;

        int num = SDL_GetNumAudioDevices(0); // 1 input, 0 output
        for (int i = 0; i < num; i++) {
            const char* name = SDL_GetAudioDeviceName(i, 0); // 1 input, 0 output
            ret.push_back(name);
        }

        return ret;
    }

    const std::string config(const std::string& deviceName)
    {
        turnOff();

        std::string _deviceName = deviceName;

        if(deviceName == "") {

            char* name;
            SDL_AudioSpec dummySpec;
            if(SDL_GetDefaultAudioInfo(&name, &dummySpec, 0) != 0) return "SDL_GetDefaultAudioInfo error";
            _deviceName = name;
            SDL_free(name); 
        }

        int deviceIndex = 0;

        auto deviceList = getAudioList();

        for(int i = 0; i < deviceList.size(); i++) {
            if(_deviceName == deviceList[i]) {
                deviceIndex = i;
                break;
            }
        }

        SDL_AudioSpec preferedSpec;

        if( SDL_GetAudioDeviceSpec(deviceIndex, 0, &preferedSpec) != 0) return "SDL_GetAudioDeviceSpec error";

        m_currentDeviceName = _deviceName;

        int sampleRate = preferedSpec.freq;
        int sampleSize = 8;

        switch(preferedSpec.format) {
            case AUDIO_U8: sampleSize=8; break;
            case AUDIO_S16: sampleSize=16; break;
            case AUDIO_S32: sampleSize=32; break;
        }

        std::cout << "device name: " << _deviceName << std::endl;
        std::cout << "sample rate: " << sampleRate << std::endl;
        std::cout << "sample size: " << sampleSize << std::endl;
    
        spec.freq = sampleRate;
        spec.channels = 1;
        spec.samples = sampleRate*BUFFER_TIME;
        spec.callback = nullptr;
        spec.userdata = this;

        switch(sampleSize) {
            case 8:
                spec.format = AUDIO_U8;
                break;
            case 16:
                spec.format = AUDIO_S16; 
                break;
            case 32:
                spec.format = AUDIO_S32;
                break;            
        }     

        SDL_AudioSpec obtained;     

        m_device = SDL_OpenAudioDevice(_deviceName != "" ? _deviceName.c_str() : nullptr, 0, &spec, &obtained, 0);

        if(m_device == 0 || spec.format != obtained.format) {
            turnOff();
            std::cout << "audio erro" << std::endl;
            return "ret audio erro";
        }     

        SDL_PauseAudioDevice(m_device, 0);     

        m_pulseWave1.init(spec.freq);
        m_pulseWave2.init(spec.freq);
        m_triangleWave.init(spec.freq);
        m_noise.init(spec.freq);
        m_sample.init(spec.freq);
        m_sampleDirect.init(spec.freq);

        //from https://www.nesdev.org/wiki/APU_Mixer
        m_hpFilter1.init(spec.freq, 90);
        m_hpFilter2.init(spec.freq, 440);
        m_lpFilter.init(spec.freq, 14000);

        m_playFlag = false;

        return "";
    }

    bool init() override
    {
        if(m_device == 0) config("");
        return true;
    }

    GERANES_INLINE_HOT float mix(void)
    {
        float ret = 0;

        const float sum = 0.5+0.5+0.5+1.0+1.5+1.5;

        //empirical values 
        ret += 0.5/sum*m_pulseWave1.get();
        ret += 0.5/sum*m_pulseWave2.get();
        ret += 0.5/sum*m_triangleWave.get();
        ret += 1.0/sum*m_noise.get();
        ret += 1.5/sum*m_sample.get();
        ret += 1.5/sum*m_sampleDirect.get();

        ret = m_hpFilter1.apply(ret);
        ret = m_hpFilter2.apply(ret);
        ret = m_lpFilter.apply(ret);

        if(ret > 0.999) ret = 0.9999;
        else if(ret < -0.999) ret = -0.9999;

        return ret;
    }

    void render(float dt, float volume) override
    {    

        if(m_device == 0){
            turnOff();
            return;
        }       
        
        int size = 0;             

        size = (dt * sampleRate()) + 0.5; //round

        while(size > 0)
        {
            float value = mix() * volume;

            if(sampleSize() == 8) {
                int temp = (value/2+0.5)*255;
                if(temp < 0) temp = 0;
                else if(temp > 255) temp = 255;
                m_buffer.push_back(temp);
            }           
            else {
                uint64_t temp = value/2 * exp2(sampleSize());

                for(int i = 0; i < sampleSize()/8; i++ ){
                    m_buffer.push_back(temp&0xFF);
                    temp >>= 8;
                }
            }

            size--;
        }
        
        if(m_playFlag || m_buffer.size() >= sampleRate()*(sampleSize()/8)*BUFFER_TIME)
        {
            m_playFlag = true;
            SDL_QueueAudio(m_device, (void*)(&m_buffer[0]), m_buffer.size());
        }
        
        if(m_buffer.size() >= sampleRate()*(sampleSize()/8)*BUFFER_TIME)
        {
            m_buffer.clear();
            m_playFlag = false;
        }  

    }

    void setSquare1Frequency(float f) override
    {
        m_pulseWave1.setFrequency(f);
    }

    void setSquare1DutyCycle(float d) override
    {
        m_pulseWave1.setDuty(d);
    }

    void setSquare1Volume(float v) override
    {
        m_pulseWave1.setVolume(v);
    }

    void setSquare2Frequency(float f) override
    {
        m_pulseWave2.setFrequency(f);
    }

    void setSquare2DutyCycle(float d) override
    {
        m_pulseWave2.setDuty(d);
    }

    void setSquare2Volume(float v) override
    {
        m_pulseWave2.setVolume(v);
    }

    void setTriangleFrequency(float f) override
    {
        m_triangleWave.setFrequency(f);
    }

    void setTriangleVolume(float v) override
    {
        m_triangleWave.setVolume(v);
    }

    void setNoiseFrequency(float f) override
    {
        m_noise.setFrequency(f);
    }

    void setNoiseMetallic(bool state) override
    {
        m_noise.setMetallic(state);
    }

    void setNoiseVolume(float v) override
    {
        m_noise.setVolume(v);
    }

    void setSampleVolume(float v) override
    {
        m_sample.setVolume(v);
    }

    void setSampleFrequency(float f) override
    {
        m_sample.setFrequency(f);
    }

    void addSample(float sample) override
    {
        m_sample.add(sample);
    }

    void addSampleDirect(float period, float sample) override
    {
        m_sampleDirect.add(period,sample);
    }


};

#endif
