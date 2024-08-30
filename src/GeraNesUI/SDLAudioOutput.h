#ifndef SDL_AUDIO_OUTPUT_H
#define SDL_AUDIO_OUTPUT_H

#include <SDL.h>

#include <set>
#include <string>
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>

#define BUFFER_TIME (0.1) //seconds

#include "GeraNes/IAudioOutput.h"

#include "GeraNes/Logger.h"

#include "AudioGenerator.h"

class SDLAudioOutput : public IAudioOutput
{

private:

    SDL_AudioDeviceID m_device = 0;
    std::string m_currentDeviceName = "";

    std::vector<char> m_buffer;

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

    uint32_t sampleAcc = 0;

    float m_volume = 1.0f;

    void clearBuffers()
    {
        m_buffer.clear();
        m_sampleDirect.clearBuffer();
        m_sample.clearBuffer();
        sampleAcc = 0;
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

    SDLAudioOutput()
    {
        SDL_Init(SDL_INIT_AUDIO);

        auto audioDevices = getAudioList();

        for(auto d : audioDevices) {
            Logger::instance().log(std::string("Audio device detected: ") + d, Logger::INFO);
        }        
    }

    ~SDLAudioOutput() override
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

    bool config(const std::string& deviceName)
    {
        turnOff();

        Logger::instance().log("Initializing audio...", Logger::INFO);

        std::string _deviceName = deviceName;

        if(deviceName == "") {

            char* name;
            SDL_AudioSpec dummySpec;
            if(SDL_GetDefaultAudioInfo(&name, &dummySpec, 0) != 0) {
                Logger::instance().log("SDL_GetDefaultAudioInfo error", Logger::ERROR);
                return false;
            }
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

        if( SDL_GetAudioDeviceSpec(deviceIndex, 0, &preferedSpec) != 0) {
            Logger::instance().log("SDL_GetAudioDeviceSpec error", Logger::ERROR);
            return false;
        }

        m_currentDeviceName = _deviceName;

        int sampleRate = preferedSpec.freq;
        int sampleSize = 8;

        switch(preferedSpec.format) {
            case AUDIO_U8: sampleSize=8; break;
            case AUDIO_S16: sampleSize=16; break;
            case AUDIO_S32: sampleSize=32; break;
        }
    
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
            Logger::instance().log("Audio init error", Logger::ERROR);
            return false; 
        }

        Logger::instance().log(std::string("Audio device: ") + _deviceName, Logger::INFO);
        Logger::instance().log(std::string("Sample rate: ") + std::to_string(sampleRate), Logger::INFO); 
        Logger::instance().log(std::string("Sample size: ") + std::to_string(sampleSize), Logger::INFO); 

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

        return true;
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
    
    void render(uint32_t dt, bool silenceFlag) override
    {    

        if(m_device == 0){
            turnOff();
            return;
        }       

        sampleAcc += dt * sampleRate();

        float vol = std::pow(m_volume,2);

        while(sampleAcc >= 1000)
        {
            float value = silenceFlag ? 0 : mix() * vol;

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

            sampleAcc -= 1000;
        }   

        bool playFlag = SDL_GetQueuedAudioSize(m_device) != 0;

        //if(!playFlag) std::cout << "Audio buffer underflow" << std::endl;
        
        if(playFlag || m_buffer.size() >= sampleRate()*(sampleSize()/8)*BUFFER_TIME)
        {   
            SDL_QueueAudio(m_device, (void*)(&m_buffer[0]), m_buffer.size());
            m_buffer.clear();
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

    void setVolume(float volume) {
        m_volume = volume;
    }

    float getVolume() {
        return m_volume;
    }


};

#endif
