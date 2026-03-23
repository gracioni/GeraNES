#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <string>

#include "../../Serialization.h"
#include "../../ThirdParty/emu2413.h"

class Vrc7Audio
{
private:
    static constexpr int OPLL_SAMPLE_RATE = 49716;
    static constexpr int OPLL_CLOCK_RATE = OPLL_SAMPLE_RATE * 72;

    OPLL* m_opll = nullptr;
    std::array<uint8_t, 0x40> m_regs = {};
    uint8_t m_currentReg = 0;
    float m_sample = 0.0f;
    double m_clockTimer = 0.0;
    bool m_muted = false;
    float m_volume = 1.0f;

    void applyRegisters()
    {
        if(m_opll == nullptr) return;

        OPLL_reset(m_opll);
        for(size_t i = 0; i < m_regs.size(); ++i) {
            OPLL_writeReg(m_opll, static_cast<uint32_t>(i), m_regs[i]);
        }
        m_sample = 0.0f;
    }

public:
    Vrc7Audio()
    {
        m_opll = OPLL_new(OPLL_CLOCK_RATE, OPLL_SAMPLE_RATE);
        OPLL_setChipType(m_opll, 1);
        OPLL_resetPatch(m_opll, 1);
        reset();
    }

    ~Vrc7Audio()
    {
        if(m_opll != nullptr) {
            OPLL_delete(m_opll);
            m_opll = nullptr;
        }
    }

    void reset()
    {
        m_regs.fill(0);
        m_currentReg = 0;
        m_sample = 0.0f;
        m_clockTimer = 0.0;
        m_muted = false;
        m_volume = 1.0f;
        applyRegisters();
    }

    void setMuted(bool muted)
    {
        m_muted = muted;
        if(m_muted) {
            m_sample = 0.0f;
        }
    }

    void writeRegisterSelect(uint8_t value)
    {
        if(m_muted) return;
        m_currentReg = value & 0x3F;
    }

    void writeRegisterData(uint8_t value)
    {
        if(m_muted) return;
        m_regs[m_currentReg] = value;
        OPLL_writeReg(m_opll, m_currentReg, value);
    }

    void clock(int cpuClockHz)
    {
        if(m_muted) {
            m_sample = 0.0f;
            return;
        }

        if(m_clockTimer <= 0.0) {
            m_clockTimer += static_cast<double>(cpuClockHz) / static_cast<double>(OPLL_SAMPLE_RATE);
        }

        m_clockTimer -= 1.0;
        while(m_clockTimer <= 0.0) {
            const int16_t raw = OPLL_calc(m_opll);
            m_sample = std::clamp((static_cast<float>(raw) / 4096.0f) * m_volume, -1.0f, 1.0f);
            m_clockTimer += static_cast<double>(cpuClockHz) / static_cast<double>(OPLL_SAMPLE_RATE);
        }
    }

    float getSample() const
    {
        return m_muted ? 0.0f : m_sample;
    }

    float getMixWeight() const
    {
        return 1.0f;
    }

    std::string getAudioChannelsJson() const
    {
        std::ostringstream ss;
        ss << "{\"channels\":["
           << "{\"id\":\"vrc7.fm\",\"label\":\"VRC7 FM\",\"volume\":" << m_volume << ",\"min\":0.0,\"max\":1.0}"
           << "]}";
        return ss.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume)
    {
        if(id != "vrc7.fm") return false;
        m_volume = std::clamp(volume, 0.0f, 1.0f);
        return true;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_currentReg);
        s.array(m_regs.data(), 1, m_regs.size());
        SERIALIZEDATA(s, m_sample);
        SERIALIZEDATA(s, m_clockTimer);
        SERIALIZEDATA(s, m_muted);
        SERIALIZEDATA(s, m_volume);

        if(dynamic_cast<Deserialize*>(&s) != nullptr) {
            applyRegisters();
        }
    }
};
