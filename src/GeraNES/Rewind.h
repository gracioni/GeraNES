#ifndef REWIND_H
#define REWIND_H

#include <cstdint>

#include "IRewindable.h"
#include "util/CircularBuffer.h"

class Rewind
{
private:

    IRewindable& m_target;
    bool m_enabled = false;
    double m_maxTime = 0.0;
    bool m_activeFlag = false;
    double m_timer = 0.0;
    int m_FPSDivider = 1;
    int m_FPSAuxCounter = 0;
    CircularBuffer<std::vector<uint8_t>>* m_buffer = nullptr;

    void addState(const std::vector<uint8_t> data)
    {
        if(m_buffer != nullptr) m_buffer->write(data);
    }

public:

    Rewind(IRewindable& target) : m_target(target) {        
    }

    ~Rewind()
    {
        destroy();
    }

    void setup(bool enabled, double maxTime, int _FPSDivider)
    {   
        m_enabled = enabled;
        m_maxTime = maxTime;
        m_FPSDivider = _FPSDivider;

        m_FPSAuxCounter = m_FPSDivider;

        if(m_buffer != nullptr) {
            delete m_buffer;
            m_buffer = nullptr;
        }

        if(m_enabled) m_buffer = new CircularBuffer<std::vector<uint8_t>>(static_cast<size_t>((float)m_target.getFPS()/m_FPSDivider * m_maxTime),CircularBuffer<std::vector<uint8_t>>::REPLACE);

        m_activeFlag = false;
        m_timer = 0.0;
    }

    void reset()
    {
        /*
        m_activeFlag = false;
        m_timer = 0.0;
        m_FPSAuxCounter = m_FPSDivider;
        if(m_buffer != nullptr) m_buffer->clear();
        */
       setup(m_enabled, m_maxTime, m_FPSDivider);
    }

    //return true when sample a frame
    bool update()
    {
        bool ret = false;

        if( --m_FPSAuxCounter <= 0 ) {
            m_FPSAuxCounter = m_FPSDivider;
            ret = true;
        }

        return ret;
    }      

    void destroy()
    {
        m_FPSAuxCounter = m_FPSDivider;

        if(m_buffer != nullptr) {
            delete m_buffer;
            m_buffer = nullptr;
        }

        m_enabled = false;
        m_maxTime = 0.0;
        m_activeFlag = false;
        m_timer = 0.0;
    }

    void newFrame() {

        if(m_buffer != nullptr && (!m_activeFlag || m_buffer->size() == 0) && update() ) {
            Serialize s;
            m_target.serialization(s);
            addState(s.getData());
        }

        if(m_buffer != nullptr && m_activeFlag) {

            if(m_buffer->size() > 1 && update()) {
                //load state from memory
                m_target.loadStateFromMemory(m_buffer->readBack());
            }
            else {
                m_target.loadStateFromMemory(m_buffer->peakBack());
            }

        }

    }

    void setRewind(bool state)
    {
        m_activeFlag = state;
    }

    bool isRewinding()
    {
        return m_buffer != nullptr && m_activeFlag;
    }

    bool rewindLimit() {

        bool ret = false;

        if(m_buffer == nullptr) ret = true;
        else {
            if(!m_activeFlag) ret = true;
            else if(m_buffer->size() > 1) ret = true;
        }

        return ret;
    }

};


#endif
