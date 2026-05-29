#pragma once

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

class IAudioOutput
{
    public:

    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    class ExternalAudioMixer
    {
    public:
        virtual ~ExternalAudioMixer() = default;
        virtual void resetRuntime() = 0;
        virtual void onOutputSampleRateChanged(int sampleRate) = 0;
        virtual float mixMonoSample(int sampleRate) = 0;
        virtual StereoSample mixStereoFrame(int sampleRate)
        {
            const float sample = mixMonoSample(sampleRate);
            return { sample, sample };
        }
        virtual int preferredOutputChannels() const { return 1; }
    };

    enum class Channel { Pulse_1, Pulse_2, Triangle, Noise, Sample, Expansion };
    enum class PulseChannel { Pulse_1, Pulse_2 };

    struct AudioFormatOptions {
        std::vector<int> sampleRates;
        std::vector<int> sampleSizes;
    };

    virtual bool init(){return true;}
    virtual void render(uint32_t /*dt*/, bool /*silenceFlag*/){}

    virtual void setChannelVolume(Channel, float) {}
    virtual void setChannelFrequency(Channel, float) {}

    virtual void setPulseDutyCycle(PulseChannel, float){}
    virtual void setNoiseMetallic(bool /*state*/){}
    virtual void addSample(float /*sample*/){}
    virtual void addSampleDirect(float /*period*/, float /*sample*/){}
    virtual void setExpansionSourceRateHz(int /*rateHz*/) {}
    virtual void setExpansionAudioVolume(float /*volume*/) {}
    virtual void setRewinding(bool /*rewinding*/) {}
    virtual void processExpansionAudioSample(float /*currentSample*/, float /*mixWeight*/) {}
    virtual void clearAudioBuffers() {}
    virtual void discardQueuedAudio() {}
    virtual std::vector<float> getRecentMixedSamples(size_t /*maxSamples*/ = 0) const { return {}; }
    virtual int outputSampleRate() const { return 44100; }
    virtual void setPlaybackSpeed(double /*speed*/) {}
    virtual std::vector<std::string> getAudioList() const { return {}; }
    virtual AudioFormatOptions getAudioFormatOptions(const std::string& /*deviceName*/) const { return {}; }
    virtual const std::string& currentDeviceName() const { static const std::string empty; return empty; }
    virtual bool config(const std::string& /*deviceName*/) { return true; }
    virtual bool config(const std::string& deviceName, int /*sampleRate*/, int /*sampleSize*/) { return config(deviceName); }
    virtual int currentSampleRate() const { return outputSampleRate(); }
    virtual int currentSampleSize() const { return 16; }
    virtual void restart() {}
    virtual void setVolume(float /*volume*/) {}
    virtual float getVolume() const { return 1.0f; }
    virtual void setExternalAudioMixer(std::shared_ptr<ExternalAudioMixer> /*mixer*/) {}
    virtual std::shared_ptr<ExternalAudioMixer> getExternalAudioMixer() const { return {}; }

    virtual std::string getAudioChannelsJson() const { return "{\"channels\":[]}"; }
    virtual bool setAudioChannelVolumeById(const std::string& /*id*/, float /*volume*/) { return false; }

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
