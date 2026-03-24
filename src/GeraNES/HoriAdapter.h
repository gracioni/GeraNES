#pragma once

#include <array>
#include <cstdint>

#include "Controller.h"

class HoriAdapter
{
private:
    std::array<Controller, 4> m_controllers;
    std::array<uint8_t, 2> m_signature = {0x04, 0x08};
    std::array<uint8_t, 2> m_signatureCounter = {16, 16};
    bool m_strobe = false;

    void resetSignature()
    {
        m_signature = {0x04, 0x08};
        m_signatureCounter = {16, 16};
    }

    uint8_t readPort(uint8_t port, bool outputEnabled)
    {
        if(m_strobe) {
            resetSignature();
        }

        uint8_t controllerIndex = port;
        uint8_t output = 0;

        if(m_signatureCounter[port] > 0) {
            --m_signatureCounter[port];
            if(m_signatureCounter[port] < 8) {
                controllerIndex = static_cast<uint8_t>(controllerIndex + 2);
            }
            output = m_controllers[controllerIndex].read(outputEnabled) & 0x01;
        }
        else {
            output = m_signature[port] & 0x01;
            m_signature[port] = static_cast<uint8_t>((m_signature[port] >> 1) | 0x80);
        }

        return output;
    }

public:
    void write4016(uint8_t data)
    {
        data &= 0x01;
        const bool prevStrobe = m_strobe;
        m_strobe = data != 0;
        for(auto& controller : m_controllers) {
            controller.write(data);
        }
        if(prevStrobe && !m_strobe) {
            resetSignature();
        }
    }

    uint8_t read4016(bool outputEnabled)
    {
        return static_cast<uint8_t>(readPort(0, outputEnabled) << 1);
    }

    uint8_t read4017(bool outputEnabled)
    {
        return static_cast<uint8_t>(readPort(1, outputEnabled) << 1);
    }

    void onCpuGetToPutTransition()
    {
        for(auto& controller : m_controllers) {
            controller.onCpuGetToPutTransition();
        }
    }

    void setControllerButtons(int index, bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        if(index < 0 || index >= static_cast<int>(m_controllers.size())) return;
        m_controllers[static_cast<size_t>(index)].setButtonsStatus(bA, bB, bSelect, bStart, bUp, bDown, bLeft, bRight);
    }

    void serialization(SerializationBase& s)
    {
        for(auto& controller : m_controllers) {
            controller.serialization(s);
        }
        SERIALIZEDATA(s, m_signature[0]);
        SERIALIZEDATA(s, m_signature[1]);
        SERIALIZEDATA(s, m_signatureCounter[0]);
        SERIALIZEDATA(s, m_signatureCounter[1]);
        SERIALIZEDATA(s, m_strobe);
    }
};
