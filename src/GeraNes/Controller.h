#ifndef INCLUDE_CONTROLLER
#define INCLUDE_CONTROLLER

#include "defines.h"

#include "Serialization.h"

class Controller
{
private:

    uint8_t m_data[8];
    int m_index;

public:

    Controller()
    {
        m_index = 0;
        memset(m_data,0,8*sizeof(uint8_t));
    }

    uint8_t read(void)
    {
        uint8_t ret = m_data[m_index];

        if(m_index == 8) ret = 0x01;
        else m_index++;

        return ret;
    }

    void write(uint8_t data)
    {
        if(data&0x01) m_index = 0;
    }

    void setButtonsStatus(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_data[0] = bA ? 0x01 : 0x00;
        m_data[1] = bB ? 0x01 : 0x00;        
        m_data[2] = bSelect ? 0x01 : 0x00;
        m_data[3] = bStart ? 0x01 : 0x00;
        m_data[4] = bUp ? 0x01 : 0x00;
        m_data[5] = (bDown && !bUp) ? 0x01 : 0x00;
        m_data[6] = (bLeft && !bRight) ? 0x01 : 0x00;
        m_data[7] = bRight ? 0x01 : 0x00;
    }

    void serialization(SerializationBase& s)
    {
        s.array(m_data, 1, 8);
        SERIALIZEDATA(s, m_index);
    }

};

#endif
