#pragma once

#include <algorithm>
#include <cstdint>

#include "../../Serialization.h"
#include "../../APU/APUCommon.h"

class Mmc5Audio
{
private:
    struct Square
    {
        uint8_t regs[4] = {0, 0, 0, 0};
        uint16_t timer = 0;
        uint8_t dutyPos = 0;

        bool enabled = false;
        bool lengthHalt = false;
        bool newLengthHalt = false;
        uint8_t lengthCounter = 0;
        uint8_t lengthPrevious = 0;
        uint8_t lengthReload = 0;

        bool constantVolume = false;
        uint8_t volume = 0;
        bool envStart = false;
        int8_t envDivider = 0;
        uint8_t envCounter = 0;

        static constexpr uint8_t DutySeq[4][8] = {
            {0, 0, 0, 0, 0, 0, 0, 1},
            {0, 0, 0, 0, 0, 0, 1, 1},
            {0, 0, 0, 0, 1, 1, 1, 1},
            {1, 1, 1, 1, 1, 1, 0, 0}
        };

        uint16_t period() const
        {
            return static_cast<uint16_t>(((regs[3] & 0x07) << 8) | regs[2]);
        }

        uint16_t timerPeriod() const
        {
            return static_cast<uint16_t>((period() << 1) | 0x01); // (period * 2) + 1
        }

        uint8_t duty() const
        {
            return static_cast<uint8_t>((regs[0] >> 6) & 0x03);
        }

        uint8_t outputVolume() const
        {
            if(lengthCounter == 0) {
                return 0;
            }
            if(constantVolume) {
                return volume;
            }
            return envCounter;
        }

        uint8_t output() const
        {
            const uint8_t vol = outputVolume();
            if(vol == 0) {
                return 0;
            }
            return static_cast<uint8_t>(DutySeq[duty()][dutyPos] ? vol : 0);
        }

        void tickLength()
        {
            if(lengthCounter > 0 && !lengthHalt) {
                --lengthCounter;
            }
        }

        void tickEnvelope()
        {
            if(!envStart) {
                --envDivider;
                if(envDivider < 0) {
                    envDivider = static_cast<int8_t>(volume);
                    if(envCounter > 0) {
                        --envCounter;
                    } else if(lengthHalt) {
                        envCounter = 15;
                    }
                }
            } else {
                envStart = false;
                envCounter = 15;
                envDivider = static_cast<int8_t>(volume);
            }
        }

        void reloadLengthCounter()
        {
            if(lengthReload != 0) {
                if(lengthCounter == lengthPrevious) {
                    lengthCounter = lengthReload;
                }
                lengthReload = 0;
            }
            lengthHalt = newLengthHalt;
        }

        void runChannel()
        {
            if(timer == 0) {
                dutyPos = static_cast<uint8_t>((dutyPos - 1) & 0x07);
                timer = timerPeriod();
            } else {
                --timer;
            }
        }

        void setEnabled(bool en)
        {
            enabled = en;
            if(!enabled) {
                lengthCounter = 0;
                lengthReload = 0;
            }
        }

        void writeReg(int reg, uint8_t value)
        {
            regs[reg & 0x03] = value;
            switch(reg & 0x03) {
            case 0:
                newLengthHalt = (value & 0x20) != 0;
                constantVolume = (value & 0x10) != 0;
                volume = static_cast<uint8_t>(value & 0x0F);
                break;
            case 1:
                // MMC5: no sweep unit
                break;
            case 2:
                break;
            case 3:
                if(enabled) {
                    lengthReload = LENGTH_TABLE[(value >> 3) & 0x1F];
                    lengthPrevious = lengthCounter;
                }
                dutyPos = 0;
                envStart = true;
                break;
            }
        }

        void reset()
        {
            regs[0] = regs[1] = regs[2] = regs[3] = 0;
            timer = 0;
            dutyPos = 0;
            enabled = false;
            lengthHalt = false;
            newLengthHalt = false;
            lengthCounter = 0;
            lengthPrevious = 0;
            lengthReload = 0;
            constantVolume = false;
            volume = 0;
            envStart = false;
            envDivider = 0;
            envCounter = 0;
        }
    };

    Square m_square1;
    Square m_square2;

    int32_t m_audioCounter = 0;
    int32_t m_audioCounterReload = 7457;

    bool m_pcmReadMode = false;
    bool m_pcmIrqEnabled = false;
    uint8_t m_pcmOutput = 0;
    bool m_pcmLatched = false;

    float m_volPulse1 = 1.0f;
    float m_volPulse2 = 1.0f;
    float m_volPcm = 1.0f;

    float m_sample = 0.0f;

    void updateSample()
    {
        const float s1 = static_cast<float>(m_square1.output()) * m_volPulse1;
        const float s2 = static_cast<float>(m_square2.output()) * m_volPulse2;
        const float pcm = m_pcmLatched ? (static_cast<float>(m_pcmOutput) / 255.0f) * m_volPcm : 0.0f;

        // Mesen sums as negative polarity; normalize to [-1, 1] for this emulator's mixer path.
        const float pulsePart = -(s1 + s2) / 30.0f;
        const float pcmPart = -pcm;
        m_sample = std::clamp(pulsePart * 0.85f + pcmPart * 0.15f, -1.0f, 1.0f);
    }

public:
    void reset(int cpuClockHz = 1789773)
    {
        m_square1.reset();
        m_square2.reset();
        m_audioCounterReload = std::max(1, cpuClockHz / 240);
        m_audioCounter = 0;
        m_pcmReadMode = false;
        m_pcmIrqEnabled = false;
        m_pcmOutput = 0;
        m_pcmLatched = false;
        m_volPulse1 = 1.0f;
        m_volPulse2 = 1.0f;
        m_volPcm = 1.0f;
        m_sample = 0.0f;
    }

    void clock()
    {
        --m_audioCounter;
        m_square1.runChannel();
        m_square2.runChannel();

        if(m_audioCounter <= 0) {
            m_audioCounter = m_audioCounterReload;
            m_square1.tickLength();
            m_square1.tickEnvelope();
            m_square2.tickLength();
            m_square2.tickEnvelope();
        }

        m_square1.reloadLengthCounter();
        m_square2.reloadLengthCounter();

        updateSample();
    }

    void writeRegister(uint16_t addr, uint8_t value)
    {
        switch(addr) {
        case 0x5000: case 0x5001: case 0x5002: case 0x5003:
            m_square1.writeReg(addr - 0x5000, value);
            break;
        case 0x5004: case 0x5005: case 0x5006: case 0x5007:
            m_square2.writeReg(addr - 0x5004, value);
            break;
        case 0x5010:
            m_pcmReadMode = (value & 0x01) == 0x01;
            m_pcmIrqEnabled = (value & 0x80) == 0x80;
            break;
        case 0x5011:
            if(!m_pcmReadMode && value != 0) {
                m_pcmOutput = value;
                m_pcmLatched = true;
            }
            break;
        case 0x5015:
            m_square1.setEnabled((value & 0x01) != 0);
            m_square2.setEnabled((value & 0x02) != 0);
            break;
        }
    }

    uint8_t readStatus() const
    {
        uint8_t status = 0;
        status |= (m_square1.lengthCounter > 0) ? 0x01 : 0x00;
        status |= (m_square2.lengthCounter > 0) ? 0x02 : 0x00;
        return status;
    }

    bool isPcmLatched() const { return m_pcmLatched; }
    uint8_t pulseLengthCounter(int index) const { return index == 0 ? m_square1.lengthCounter : m_square2.lengthCounter; }

    float getSample() const
    {
        return m_sample;
    }

    float getMixWeight() const
    {
        return 3.0f;
    }

    void setPulse1Volume(float v) { m_volPulse1 = std::clamp(v, 0.0f, 1.0f); }
    void setPulse2Volume(float v) { m_volPulse2 = std::clamp(v, 0.0f, 1.0f); }
    void setPcmVolume(float v) { m_volPcm = std::clamp(v, 0.0f, 1.0f); }
    float pulse1Volume() const { return m_volPulse1; }
    float pulse2Volume() const { return m_volPulse2; }
    float pcmVolume() const { return m_volPcm; }

    void serialization(SerializationBase& s)
    {
        s.array(m_square1.regs, 1, 4);
        SERIALIZEDATA(s, m_square1.timer);
        SERIALIZEDATA(s, m_square1.dutyPos);
        SERIALIZEDATA(s, m_square1.enabled);
        SERIALIZEDATA(s, m_square1.lengthHalt);
        SERIALIZEDATA(s, m_square1.newLengthHalt);
        SERIALIZEDATA(s, m_square1.lengthCounter);
        SERIALIZEDATA(s, m_square1.lengthPrevious);
        SERIALIZEDATA(s, m_square1.lengthReload);
        SERIALIZEDATA(s, m_square1.constantVolume);
        SERIALIZEDATA(s, m_square1.volume);
        SERIALIZEDATA(s, m_square1.envStart);
        SERIALIZEDATA(s, m_square1.envDivider);
        SERIALIZEDATA(s, m_square1.envCounter);

        s.array(m_square2.regs, 1, 4);
        SERIALIZEDATA(s, m_square2.timer);
        SERIALIZEDATA(s, m_square2.dutyPos);
        SERIALIZEDATA(s, m_square2.enabled);
        SERIALIZEDATA(s, m_square2.lengthHalt);
        SERIALIZEDATA(s, m_square2.newLengthHalt);
        SERIALIZEDATA(s, m_square2.lengthCounter);
        SERIALIZEDATA(s, m_square2.lengthPrevious);
        SERIALIZEDATA(s, m_square2.lengthReload);
        SERIALIZEDATA(s, m_square2.constantVolume);
        SERIALIZEDATA(s, m_square2.volume);
        SERIALIZEDATA(s, m_square2.envStart);
        SERIALIZEDATA(s, m_square2.envDivider);
        SERIALIZEDATA(s, m_square2.envCounter);

        SERIALIZEDATA(s, m_audioCounter);
        SERIALIZEDATA(s, m_audioCounterReload);
        SERIALIZEDATA(s, m_pcmReadMode);
        SERIALIZEDATA(s, m_pcmIrqEnabled);
        SERIALIZEDATA(s, m_pcmOutput);
        SERIALIZEDATA(s, m_pcmLatched);
        SERIALIZEDATA(s, m_volPulse1);
        SERIALIZEDATA(s, m_volPulse2);
        SERIALIZEDATA(s, m_volPcm);
        SERIALIZEDATA(s, m_sample);
    }
};
