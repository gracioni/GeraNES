#pragma once

#include "IExpansionDevice.h"
#include "NesStandardController.h"

class FamicomExpansionStandardController : public IExpansionDevice
{
private:
    NesStandardController m_controller;

public:
    uint8_t read4017(bool) override
    {
        return 0x00;
    }

    void write4016(uint8_t data) override
    {
        m_controller.write(data);
    }

    uint8_t read4016(bool outputEnabled) override
    {
        return static_cast<uint8_t>(m_controller.read(outputEnabled) << 1);
    }

    void setButtonsStatus(bool bA, bool bB, bool bSelect, bool bStart,
                          bool bUp, bool bDown, bool bLeft, bool bRight) override
    {
        m_controller.setButtonsStatus(bA, bB, bSelect, bStart, bUp, bDown, bLeft, bRight);
    }

    void serialization(SerializationBase& s) override
    {
        m_controller.serialization(s);
    }
};
