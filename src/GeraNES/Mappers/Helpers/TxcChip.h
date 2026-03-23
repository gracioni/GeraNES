#pragma once

#include "../../Serialization.h"

class TxcChip
{
private:
    uint8_t m_accumulator = 0;
    uint8_t m_inverter = 0;
    uint8_t m_staging = 0;
    uint8_t m_output = 0;
    bool m_increase = false;
    bool m_yFlag = false;
    bool m_invert = false;
    uint8_t m_mask = 0;
    bool m_isJv001 = false;

public:
    explicit TxcChip(bool isJv001) :
        m_invert(isJv001),
        m_mask(isJv001 ? 0x0F : 0x07),
        m_isJv001(isJv001)
    {
    }

    uint8_t output() const
    {
        return m_output;
    }

    bool invertFlag() const
    {
        return m_invert;
    }

    bool yFlag() const
    {
        return m_yFlag;
    }

    uint8_t read()
    {
        const uint8_t value = static_cast<uint8_t>((m_accumulator & m_mask) | ((m_inverter ^ (m_invert ? 0xFF : 0x00)) & ~m_mask));
        m_yFlag = !m_invert || ((value & 0x10) != 0);
        return value;
    }

    void write(uint16_t absolute, uint8_t value)
    {
        if(absolute < 0x8000) {
            switch(absolute & 0xE103) {
            case 0x4100:
                if(m_increase) {
                    ++m_accumulator;
                } else {
                    m_accumulator = static_cast<uint8_t>(((m_accumulator & ~m_mask) | (m_staging & m_mask)) ^ (m_invert ? 0xFF : 0x00));
                }
                break;
            case 0x4101:
                m_invert = (value & 0x01) != 0;
                break;
            case 0x4102:
                m_staging = static_cast<uint8_t>(value & m_mask);
                m_inverter = static_cast<uint8_t>(value & ~m_mask);
                break;
            case 0x4103:
                m_increase = (value & 0x01) != 0;
                break;
            default:
                break;
            }
        } else {
            if(m_isJv001) {
                m_output = static_cast<uint8_t>((m_accumulator & 0x0F) | (m_inverter & 0xF0));
            } else {
                m_output = static_cast<uint8_t>((m_accumulator & 0x0F) | ((m_inverter & 0x08) << 1));
            }
        }

        m_yFlag = !m_invert || ((value & 0x10) != 0);
    }

    void reset()
    {
        m_accumulator = 0;
        m_inverter = 0;
        m_staging = 0;
        m_output = 0;
        m_increase = false;
        m_yFlag = false;
        m_invert = m_isJv001;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_accumulator);
        SERIALIZEDATA(s, m_inverter);
        SERIALIZEDATA(s, m_staging);
        SERIALIZEDATA(s, m_output);
        SERIALIZEDATA(s, m_increase);
        SERIALIZEDATA(s, m_yFlag);
        SERIALIZEDATA(s, m_invert);
        SERIALIZEDATA(s, m_mask);
        SERIALIZEDATA(s, m_isJv001);
    }
};
