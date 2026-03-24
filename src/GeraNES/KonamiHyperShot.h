#pragma once

#include <cstdint>

#include "Serialization.h"
#include "IExpansionDevice.h"

// Konami Hyper Shot (Famicom expansion port)
// - $4016 write: bit1 enables Player 1 buttons when cleared, bit2 enables Player 2 buttons when cleared
// - $4017 read: bit1=P1 Run, bit2=P1 Jump, bit3=P2 Run, bit4=P2 Jump
class KonamiHyperShot : public IExpansionDevice
{
private:
    bool m_player1Run = false;
    bool m_player1Jump = false;
    bool m_player2Run = false;
    bool m_player2Jump = false;
    bool m_player1Enabled = false;
    bool m_player2Enabled = false;

public:
    void setButtonsStatus(bool bA, bool bB, bool, bool, bool, bool, bool, bool) override
    {
        m_player2Run = bA;
        m_player2Jump = bB;
    }

    void setPlayersButtons(bool p1Run, bool p1Jump, bool p2Run, bool p2Jump)
    {
        m_player1Run = p1Run;
        m_player1Jump = p1Jump;
        m_player2Run = p2Run;
        m_player2Jump = p2Jump;
    }

    void write4016(uint8_t data) override
    {
        m_player1Enabled = (data & 0x02) == 0;
        m_player2Enabled = (data & 0x04) == 0;
    }

    uint8_t read4017(bool) override
    {
        uint8_t ret = 0x00;
        if(m_player1Enabled && m_player1Run) ret |= (1 << 1);
        if(m_player1Enabled && m_player1Jump) ret |= (1 << 2);
        if(m_player2Enabled && m_player2Run) ret |= (1 << 3);
        if(m_player2Enabled && m_player2Jump) ret |= (1 << 4);
        return ret;
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_player1Run);
        SERIALIZEDATA(s, m_player1Jump);
        SERIALIZEDATA(s, m_player2Run);
        SERIALIZEDATA(s, m_player2Jump);
        SERIALIZEDATA(s, m_player1Enabled);
        SERIALIZEDATA(s, m_player2Enabled);
    }
};
