#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#include "GeraNES/GeraNESEmu.h"
#include "logger/logger.h"
#include "signal/signal.h"

class Test
{
private:
    class BeepAudioOutput : public IAudioOutput
    {
    private:
        static constexpr float VOLUME_ACTIVE_THRESHOLD = 0.01f;
        static constexpr float SAMPLE_ACTIVE_THRESHOLD = 0.005f;
        static constexpr float BEEP_LOUD_VOLUME_THRESHOLD = 0.20f;
        static constexpr float BEEP_LOUD_SAMPLE_THRESHOLD = 0.08f;
        static constexpr float BEEP_QUIET_VOLUME_THRESHOLD = 0.03f;
        static constexpr float BEEP_QUIET_SAMPLE_THRESHOLD = 0.01f;
        static constexpr int BEEP_MIN_QUIET_STEPS = 2;

        std::array<float, 5> m_volume = {0, 0, 0, 0, 0};
        bool m_volumeActive = false;
        bool m_sampleActivityThisStep = false;
        float m_stepMaxAbsSample = 0.0f;
        bool m_stepActive = false;
        bool m_prevStepActive = false;
        bool m_seenActivity = false;
        int m_beepCount = 0;
        uint64_t m_stepCounter = 0;
        uint64_t m_lastBeepStep = 0;
        int m_quietSteps = 0;

        GERANES_INLINE int channelIndex(Channel c) const
        {
            switch(c) {
            case Channel::Pulse_1: return 0;
            case Channel::Pulse_2: return 1;
            case Channel::Triangle: return 2;
            case Channel::Noise: return 3;
            case Channel::Sample: return 4;
            }
            return 0;
        }

        GERANES_INLINE void refreshVolumeActivity()
        {
            const float pulse = (m_volume[0] > m_volume[1]) ? m_volume[0] : m_volume[1];
            const float tri = m_volume[2];
            const float noi = m_volume[3];
            const float smp = m_volume[4];
            const float level = (pulse > tri ? pulse : tri) > (noi > smp ? noi : smp)
                ? (pulse > tri ? pulse : tri)
                : (noi > smp ? noi : smp);
            m_volumeActive = level >= VOLUME_ACTIVE_THRESHOLD;
        }

    public:
        void setChannelVolume(Channel c, float v) override
        {
            m_volume[static_cast<size_t>(channelIndex(c))] = v;
            refreshVolumeActivity();
        }

        void addSample(float sample) override
        {
            const float absSample = std::fabs(sample);
            if(absSample >= SAMPLE_ACTIVE_THRESHOLD) {
                m_sampleActivityThisStep = true;
            }
            if(absSample > m_stepMaxAbsSample) m_stepMaxAbsSample = absSample;
        }

        void addSampleDirect(float /*period*/, float sample) override
        {
            const float absSample = std::fabs(sample);
            if(absSample >= SAMPLE_ACTIVE_THRESHOLD) {
                m_sampleActivityThisStep = true;
            }
            if(absSample > m_stepMaxAbsSample) m_stepMaxAbsSample = absSample;
        }

        GERANES_INLINE bool onStep()
        {
            ++m_stepCounter;
            m_stepActive = m_volumeActive || m_sampleActivityThisStep;

            const float pulse = (m_volume[0] > m_volume[1]) ? m_volume[0] : m_volume[1];
            const float tri = m_volume[2];
            const float noi = m_volume[3];
            const float smp = m_volume[4];
            const float stepVolumeLevel = (pulse > tri ? pulse : tri) > (noi > smp ? noi : smp)
                ? (pulse > tri ? pulse : tri)
                : (noi > smp ? noi : smp);

            const bool stepIsQuiet =
                stepVolumeLevel <= BEEP_QUIET_VOLUME_THRESHOLD &&
                m_stepMaxAbsSample <= BEEP_QUIET_SAMPLE_THRESHOLD;

            if(stepIsQuiet) {
                ++m_quietSteps;
            } else {
                m_quietSteps = 0;
            }

            const bool stepIsLoud =
                stepVolumeLevel >= BEEP_LOUD_VOLUME_THRESHOLD ||
                m_stepMaxAbsSample >= BEEP_LOUD_SAMPLE_THRESHOLD;

            if(stepIsLoud && m_quietSteps >= BEEP_MIN_QUIET_STEPS) {
                ++m_beepCount;
                m_lastBeepStep = m_stepCounter;
                m_quietSteps = 0;
            }

            if(!m_prevStepActive && m_stepActive) {
                // Fallback edge detector for very short/simple pulse-based beeps.
                if(m_beepCount == 0 || (m_stepCounter - m_lastBeepStep) > 2) {
                    ++m_beepCount;
                    m_lastBeepStep = m_stepCounter;
                }
            }

            if(m_stepActive) m_seenActivity = true;
            m_prevStepActive = m_stepActive;
            m_sampleActivityThisStep = false;
            m_stepMaxAbsSample = 0.0f;
            return m_stepActive;
        }

        GERANES_INLINE bool active() const { return m_stepActive; }
        GERANES_INLINE bool seenActivity() const { return m_seenActivity; }
        GERANES_INLINE int beepCount() const { return m_beepCount; }
        GERANES_INLINE uint64_t stepsSinceLastBeep() const
        {
            return (m_lastBeepStep == 0 || m_stepCounter < m_lastBeepStep) ? UINT64_MAX : (m_stepCounter - m_lastBeepStep);
        }
    };

    class ErrorLogForwarder : public SigSlot::SigSlotBase
    {
    public:
        void onLog(const std::string& msg, Logger::Type type)
        {
            if(type == Logger::Type::ERROR) {
                std::cerr << msg << std::endl;
            }
        }
    };

    static std::string sanitizeTextOutput(const std::string& input)
    {
        std::string out;
        out.reserve(input.size());

        const size_t n = input.size();
        size_t i = 0;
        while(i < n) {
            const unsigned char c = static_cast<unsigned char>(input[i]);

            // Strip ANSI control sequence introducer: ESC [ ... final-byte
            if(c == 0x1B && (i + 1) < n && input[i + 1] == '[') {
                i += 2;
                while(i < n) {
                    const unsigned char k = static_cast<unsigned char>(input[i]);
                    if(k >= 0x40 && k <= 0x7E) {
                        ++i; // consume final byte too
                        break;
                    }
                    ++i;
                }
                continue;
            }

            if(c == '\n' || c == '\r' || c == '\t' || (c >= 0x20 && c <= 0x7E)) {
                out.push_back(static_cast<char>(c));
            }

            ++i;
        }

        return out;
    }

    static std::string readOutputText(GeraNESEmu& emu)
    {
        std::string out;
        out.reserve(1024);

        for(int i = 0; i < 0x1FFC; ++i) {
            const uint8_t c = emu.read(0x6004 + i);
            if(c == 0) break;
            out.push_back(static_cast<char>(c));
        }

        return sanitizeTextOutput(out);
    }

    static std::string readScreenText(GeraNESEmu& emu)
    {
        auto mapTileToChar = [](uint8_t tile) -> char {
            if(tile >= 32 && tile <= 126) return static_cast<char>(tile);

            // Common test-font variant using high-bit set ASCII codes.
            const uint8_t low7 = tile & 0x7F;
            if(low7 >= 32 && low7 <= 126) return static_cast<char>(low7);

            // Fallback for simple A=1..Z=26 encodings.
            if(tile >= 1 && tile <= 26) return static_cast<char>('A' + tile - 1);

            return ' ';
        };

        std::string out;
        out.reserve(32 * 30 + 30);

        auto& ppu = emu.getConsole().ppu();
        for(int y = 0; y < 30; ++y) {
            std::string line;
            line.reserve(32);

            for(int x = 0; x < 32; ++x) {
                const uint8_t tile = ppu.getTileIndexInNameTables(x, y);
                line.push_back(mapTileToChar(tile));
            }

            while(!line.empty() && line.back() == ' ') {
                line.pop_back();
            }
            while(!line.empty() && line.front() == ' ') {
                line.erase(line.begin());
            }

            out += line;
            if(y != 29) out.push_back('\n');
        }

        // Trim full-text leading/trailing whitespace while preserving internal line breaks.
        const size_t begin = out.find_first_not_of(" \t\r\n");
        if(begin == std::string::npos) return "";
        const size_t end = out.find_last_not_of(" \t\r\n");
        return out.substr(begin, end - begin + 1);
    }

public:
    static int runHeadless(const std::string& romPath)
    {
        ErrorLogForwarder errorLogForwarder;
        Logger::instance().signalLog.bind(&ErrorLogForwarder::onLog, &errorLogForwarder);

        BeepAudioOutput beepAudio;
        GeraNESEmu emu(beepAudio);

        if(!emu.open(romPath) || !emu.valid()) {
            return 2;
        }

        // Do not use player speed-boost (3x). In test mode we run uncapped headless.
        emu.setSpeedBoost(false);
        emu.setPaused(false);

        // Use smaller headless steps to better capture short/high-pitched completion beeps.
        constexpr uint32_t STEP_MS = 20;
        constexpr uint32_t INACTIVITY_TIMEOUT_MS = 300'000;
        constexpr uint32_t BEEP_SETTLE_MS = 2'000;
        constexpr uint32_t SCREEN_SETTLE_MS = 300'000;

        bool resetArmed = false;
        int resetCountdownMs = 0;
        uint8_t lastStatus6000 = emu.read(0x6000);
        uint32_t idle6000Ms = 0;
        bool harnessDetected = false;
        uint32_t beepIdleMs = 0;
        uint32_t screenPollMs = 0;
        int passedScreenHits = 0;
        int failedScreenHits = 0;
        std::string lastScreenText;
        uint32_t stableScreenMs = 0;

        while(true) {
            emu.update(STEP_MS);
            const bool beepStepActive = beepAudio.onStep();

            const uint8_t status = emu.read(0x6000);
            if(status != lastStatus6000) {
                lastStatus6000 = status;
                idle6000Ms = 0;
            } else {
                idle6000Ms += STEP_MS;
                if(idle6000Ms >= INACTIVITY_TIMEOUT_MS) {
                    return 3;
                }
            }

            const uint8_t m1 = emu.read(0x6001);
            const uint8_t m2 = emu.read(0x6002);
            const uint8_t m3 = emu.read(0x6003);
            if(!(m1 == 0xDE && m2 == 0xB0 && m3 == 0x61)) {
                screenPollMs += STEP_MS;
                if(screenPollMs >= 200) {
                    screenPollMs = 0;

                    std::string screenText = readScreenText(emu);
                    if(!screenText.empty()) {
                        if(screenText == lastScreenText) {
                            stableScreenMs += 200;
                        }
                        else {
                            lastScreenText = screenText;
                            stableScreenMs = 0;
                        }

                        std::string lowered = screenText;
                        for(char& c : lowered) {
                            if(c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                        }

                        if(lowered.find("passed") != std::string::npos) {
                            ++passedScreenHits;
                            failedScreenHits = 0;
                            if(passedScreenHits >= 2) {
                                std::cout << screenText;
                                return 0;
                            }
                        }
                        else if(lowered.find("failed") != std::string::npos) {
                            ++failedScreenHits;
                            passedScreenHits = 0;
                            if(failedScreenHits >= 2) {
                                std::cout << screenText;
                                return 1;
                            }
                        }
                        else {
                            passedScreenHits = 0;
                            failedScreenHits = 0;

                            // No explicit Passed/Failed text: if screen output is stable for
                            // a while, treat it as end-of-test and return the captured text.
                            if(stableScreenMs >= SCREEN_SETTLE_MS) {
                                std::cout << screenText;
                                return 1;
                            }
                        }
                    }
                }

                if(!harnessDetected && beepAudio.seenActivity()) {
                    const uint64_t stepsSinceLastBeep = beepAudio.stepsSinceLastBeep();
                    if(stepsSinceLastBeep != UINT64_MAX && stepsSinceLastBeep * STEP_MS >= BEEP_SETTLE_MS) {
                        const int code = beepAudio.beepCount();
                        if(code == 1) {
                            std::string screenText = readScreenText(emu);
                            if(!screenText.empty()) std::cout << screenText;
                            else std::cout << "Passed";
                            return 0;
                        }
                        if(code > 1) {
                            std::cout << "Failed (beep code " << code << ")";
                            return 1;
                        }
                    }

                    if(beepStepActive) {
                        beepIdleMs = 0;
                    } else {
                        beepIdleMs += STEP_MS;
                        if(beepIdleMs >= BEEP_SETTLE_MS) {
                            const int code = beepAudio.beepCount();
                            if(code <= 0) return 3;
                            if(code == 1) {
                                std::string screenText = readScreenText(emu);
                                if(!screenText.empty()) std::cout << screenText;
                                else std::cout << "Passed";
                                return 0;
                            }
                            std::cout << "Failed (beep code " << code << ")";
                            return 1;
                        }
                    }
                }
                continue;
            }
            harnessDetected = true;

            if(status == 0x80) {
                continue;
            }

            if(status == 0x81) {
                if(!resetArmed) {
                    resetArmed = true;
                    resetCountdownMs = 100;
                } else {
                    resetCountdownMs -= static_cast<int>(STEP_MS);
                }

                if(resetCountdownMs <= 0) {
                    emu.reset();
                    resetArmed = false;
                }
                continue;
            }

            std::cout << readOutputText(emu);
            return static_cast<int>(status);
        }

        return 3;
    }
};
