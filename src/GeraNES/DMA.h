#ifndef DMA_H
#define DMA_H

#include "CPU2A03.h"
#include "IBus.h"
#include "APU.h"

//#include <cassert>

class DMA {

private:

    Ibus& m_bus;
    CPU2A03& m_cpu;
    SampleChannel& m_sampleChannel;

    uint16_t m_oamAddr;
    int m_oamCounter;

    enum State {NONE, START_OAM, START_DMC, DUMMY, READ, WRITE} m_state;

    uint16_t m_dmcAddr;
    int m_dmcCounter;

    int m_dmcSkip;

    uint8_t m_data;    

    bool dmcReload;

public:

    DMA(Ibus& bus, SampleChannel& sampleChannel, CPU2A03& cpu) : m_bus(bus), m_sampleChannel(sampleChannel), m_cpu(cpu) {
        init();
    }

    void serialization(SerializationBase& s) {
        SERIALIZEDATA(s, m_oamAddr);
        SERIALIZEDATA(s, m_oamCounter);
        SERIALIZEDATA(s, m_state);
        SERIALIZEDATA(s, m_dmcAddr);
        SERIALIZEDATA(s, m_dmcCounter);
        SERIALIZEDATA(s, m_data);
        SERIALIZEDATA(s, m_dmcSkip);
        SERIALIZEDATA(s, dmcReload);
    }

    void init() {

        m_oamAddr = 0;
        m_oamCounter = 0;

        m_state = State::NONE;

        m_dmcAddr = 0;
        m_dmcCounter = 0;

        m_dmcSkip = false;

        m_data = 0;

        dmcReload = false;
    }

    void cycle()
    {

        if (m_state == State::NONE)
            return;

        switch (m_state)
        {

        case State::START_OAM:

            if (!m_cpu.isOpcodeWriteCycle())
            {
                m_cpu.sleep(1);
                m_state = State::READ;      
            }
            break;

        case State::START_DMC:   

            if (!m_cpu.isOpcodeWriteCycle() && dmcReload == !m_cpu.isOddCycle())
            {
                m_cpu.sleep(1);
                m_state = State::DUMMY;
            }
            break;

        case State::DUMMY:

            m_state = State::READ;
            m_cpu.sleep(1);  

            m_bus.read(m_cpu.bus_addr());

            break;

        case State::READ:

            if(m_cpu.isOddCycle()) // align test
            {
                // assert (m_cpu.isOddCycle() == true );

                if (m_dmcCounter > 0 && m_dmcSkip == 0) // dmc dma has priority over oam dma
                {
                    m_data = m_bus.read(m_dmcAddr);

                    m_sampleChannel.loadSampleBuffer(m_data);

                    --m_dmcCounter;

                    if (m_dmcCounter == 0 && m_oamCounter == 0) {
                        m_state = State::NONE;
                    }
                    else
                        m_state = State::READ;
                }
                else if (m_oamCounter > 0)
                {

                    m_data = m_bus.read(m_oamAddr++);
                    --m_oamCounter;

                    m_state = State::WRITE;
                }
                else
                    m_state = State::NONE;
            }
            else {
                m_bus.read(m_cpu.bus_addr());
            }

            m_cpu.sleep(1);

            break;

        case WRITE:

            // assert (m_cpu.isOddCycle() == false );

            m_bus.write(0x2004, m_data);
            
            --m_oamCounter;

            if (m_dmcCounter == 0 && m_oamCounter == 0)
            {
                assert(m_dmcSkip == 0);

                m_state = State::NONE;
            }
            else
            {

                if (m_oamCounter == 0 && m_dmcSkip > 0)
                {

                    assert(m_dmcCounter > 0);

                    if (m_dmcSkip == 3 || m_dmcSkip == 4)
                    {
                        m_state = State::READ;
                    }
                    else if (m_dmcSkip == 1 || m_dmcSkip == 2)
                    {
                        m_state = State::START_DMC;
                        dmcReload = false;              
                    }

                    m_dmcSkip = 0;
                }
                else
                    m_state = State::READ;
            }

            m_cpu.sleep(1);

            break;
        
        default:
            break;

        }
        
    }

    void OAMRequest(uint16_t addr) {

        if(m_state == State::NONE || m_state == State::START_DMC) {            
            m_state = State::START_OAM;
        }
        m_oamAddr = addr;
        m_oamCounter = 512;

        
    }

    void dmcRequest(uint16_t addr, bool reload) { 

        if(m_state == State::NONE) {
            m_state = State::START_DMC;            
            dmcReload = reload;
            m_dmcSkip = 0;   
        }
        else {  

            //std::cout << m_oamCounter << std::endl;  

            if(m_oamCounter <= 4) {
                m_dmcSkip = m_oamCounter;            
            }         

        }       

        m_dmcAddr = addr;
        m_dmcCounter = 1;

    }

    bool isRunning() {
        return m_state != State::NONE;
    }

};

#endif // DMA_H
