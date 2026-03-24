#pragma once

#include "NesStandardController.h"

class FamicomController : public NesStandardController
{
private:
    bool m_hasMic = false;
    bool m_microphoneEnabled = false;

public:
    explicit FamicomController(bool hasMic = false) : m_hasMic(hasMic)
    {
    }

    bool hasMic() const
    {
        return m_hasMic;
    }

    void setButtonsStatus(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight) override
    {
        NesStandardController::setButtonsStatus(
            bA,
            bB,
            m_hasMic ? false : bSelect,
            m_hasMic ? false : bStart,
            bUp,
            bDown,
            bLeft,
            bRight
        );
    }

    void setMicrophoneEnabled(bool enabled)
    {
        if(m_hasMic) {
            m_microphoneEnabled = enabled;
        }
    }

    bool microphoneEnabled() const
    {
        return m_microphoneEnabled;
    }

    void serialization(SerializationBase& s) override
    {
        NesStandardController::serialization(s);
        SERIALIZEDATA(s, m_hasMic);
        SERIALIZEDATA(s, m_microphoneEnabled);
    }
};
