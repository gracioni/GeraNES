#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "Cartridge.h"
#include "APU/APU.h"
#include "IAudioOutput.h"
#include "logger/logger.h"

class NsfPlayer
{
public:
    enum class PlaybackState { Stopped, Playing, Paused, Ended };

private:
    static constexpr uint32_t END_SILENT_FRAMES = 180;

    Cartridge& m_cartridge;
    APU& m_apu;
    IAudioOutput& m_audioOutput;
    std::function<void()> m_resetCallback;

    PlaybackState m_state = PlaybackState::Stopped;
    bool m_heardAudio = false;
    bool m_forceMute = false;
    int m_startupMuteFrames = 0;
    uint32_t m_silentFrames = 0;

    std::string songPositionLabel() const
    {
        return std::to_string(currentSong()) + "/" + std::to_string(totalSongs());
    }

    void primeSilentStartup()
    {
        for(int a = 0x4000; a <= 0x4013; ++a) m_apu.write(a, 0x00);
        m_apu.write(0x4015, 0x00);
        m_audioOutput.clearAudioBuffers();
        m_forceMute = true;
        m_startupMuteFrames = 0;
    }

    void switchToCurrentSong()
    {
        primeSilentStartup();
        m_resetCallback();
        m_cartridge.nsfSetPlaying(true);
        m_state = PlaybackState::Playing;
        m_heardAudio = false;
        m_silentFrames = 0;
        m_forceMute = true;
        m_startupMuteFrames = 1;
    }

    void resetToSongStart(bool startPlaying)
    {
        primeSilentStartup();
        m_resetCallback();
        m_cartridge.nsfSetPlaying(startPlaying);
        m_state = startPlaying ? PlaybackState::Playing : PlaybackState::Stopped;
        m_heardAudio = false;
        m_silentFrames = 0;
        m_forceMute = true;
        m_startupMuteFrames = startPlaying ? 1 : 0;
    }

public:
    NsfPlayer(Cartridge& cartridge, APU& apu, IAudioOutput& audioOutput, std::function<void()> resetCallback)
        : m_cartridge(cartridge)
        , m_apu(apu)
        , m_audioOutput(audioOutput)
        , m_resetCallback(std::move(resetCallback))
    {
    }

    void init()
    {
        m_state = PlaybackState::Stopped;
        m_heardAudio = false;
        m_forceMute = false;
        m_startupMuteFrames = 0;
        m_silentFrames = 0;
    }

    void onOpen()
    {
        if(m_cartridge.isNsf()) {
            m_state = PlaybackState::Playing;
            m_heardAudio = false;
            m_silentFrames = 0;
            m_forceMute = false;
            m_startupMuteFrames = 0;
        } else {
            init();
        }
    }

    void onEmulatorReset()
    {
        m_state = m_cartridge.isNsf() ? PlaybackState::Playing : PlaybackState::Stopped;
        m_heardAudio = false;
        m_forceMute = false;
        m_startupMuteFrames = 0;
        m_silentFrames = 0;
    }

    void onFrameStart()
    {
        if(m_startupMuteFrames > 0) {
            --m_startupMuteFrames;
            if(m_startupMuteFrames == 0) {
                m_forceMute = false;
            }
        }

        if(m_cartridge.isNsf() && m_state == PlaybackState::Playing) {
            const bool hasActiveAudio = m_apu.getActiveChannelMask() != 0;
            if(hasActiveAudio) {
                m_heardAudio = true;
                m_silentFrames = 0;
            }
            else if(m_heardAudio) {
                ++m_silentFrames;
                if(m_silentFrames >= END_SILENT_FRAMES) {
                    m_cartridge.nsfSetPlaying(false);
                    m_state = PlaybackState::Ended;
                    m_forceMute = true;
                    m_startupMuteFrames = 0;
                }
            }
        }
    }

    bool isLoaded() const
    {
        return m_cartridge.isNsf();
    }

    int totalSongs() const
    {
        return m_cartridge.nsfTotalSongs();
    }

    int currentSong() const
    {
        return m_cartridge.nsfCurrentSong();
    }

    bool isPlaying() const
    {
        return m_state == PlaybackState::Playing;
    }

    bool isPaused() const
    {
        return m_state == PlaybackState::Paused;
    }

    bool hasEnded() const
    {
        return m_state == PlaybackState::Ended;
    }

    bool forceMute() const
    {
        return m_forceMute;
    }

    void play()
    {
        if(m_cartridge.nsfSetPlaying(true)) {
            if(m_state == PlaybackState::Stopped || m_state == PlaybackState::Ended) {
                switchToCurrentSong();
            } else {
                m_state = PlaybackState::Playing;
                m_forceMute = false;
                m_startupMuteFrames = 0;
            }
            Logger::instance().log("NSF: play", Logger::Type::USER);
        }
    }

    void stop()
    {
        if(m_cartridge.nsfSetPlaying(false)) {
            resetToSongStart(false);
            Logger::instance().log("NSF: stop", Logger::Type::USER);
        }
    }

    void pause()
    {
        if(m_cartridge.nsfSetPlaying(false)) {
            m_state = PlaybackState::Paused;
            m_forceMute = true;
            m_startupMuteFrames = 0;
            Logger::instance().log("NSF: pause", Logger::Type::USER);
        }
    }

    void nextSong()
    {
        if(m_cartridge.nsfNextSong()) {
            switchToCurrentSong();
            Logger::instance().log("NSF: next song (" + songPositionLabel() + ")", Logger::Type::USER);
        }
    }

    void prevSong()
    {
        if(m_cartridge.nsfPrevSong()) {
            switchToCurrentSong();
            Logger::instance().log("NSF: previous song (" + songPositionLabel() + ")", Logger::Type::USER);
        }
    }

    void setSong(int song1Based)
    {
        if(m_cartridge.nsfSetSong(song1Based)) {
            switchToCurrentSong();
            Logger::instance().log("NSF: song selected (" + songPositionLabel() + ")", Logger::Type::USER);
        }
    }
};
