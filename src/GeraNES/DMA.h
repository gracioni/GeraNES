#ifndef DMA_H
#define DMA_H

#include "CPU2A03.h"
#include "IBus.h"
#include "APU.h"

//#include <cassert>

#ifdef DEBUG_DMA
    #include <QDebug>
#endif


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

#ifdef DEBUG_DMA
    size_t debugDmaStart= 0;
#endif

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
       
                #ifdef DEBUG_DMA
                qDebug() << m_cpu.cycleNumber() << ": " << "OAM start";
                debugDmaStart = m_cpu.cycleNumber();
                #endif
            }
            break;

        case State::START_DMC:   

            if (!m_cpu.isOpcodeWriteCycle() && dmcReload == !m_cpu.isOddCycle())
            {
                m_cpu.sleep(1);
                m_state = State::DUMMY;                
                 
                #ifdef DEBUG_DMA
                qDebug() << m_cpu.cycleNumber() << ": " << "DMC start";
                #endif
            }
            break;

        case State::DUMMY:

            m_state = State::READ;
            m_cpu.sleep(1);  

            m_bus.read(m_cpu.addr());   

            #ifdef DEBUG_DMA
            qDebug() << m_cpu.cycleNumber() << ": " << "DMC dummy";
            #endif

            break;

        case State::READ:

            if(m_cpu.isOddCycle()) // align test
            {
                // assert (m_cpu.isOddCycle() == true );

                if (m_dmcCounter > 0 && m_dmcSkip == 0)
                { // dmc priority

                    m_data = m_bus.read(m_dmcAddr);

                    m_sampleChannel.loadSampleBuffer(m_data);

                    --m_dmcCounter;

                    #ifdef DEBUG_DMA
                    qDebug() << m_cpu.cycleNumber() << ": " << "DMC read";
                    #endif

                    if (m_dmcCounter == 0 && m_oamCounter == 0)
                    { // we can finish
                        m_state = State::NONE;

                        #ifdef DEBUG_DMA
                        qDebug() << m_cpu.cycleNumber() + 1 << ": " << "DMC end" << endl;
                        #endif
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
                m_bus.read(m_cpu.addr());
            }

            m_cpu.sleep(1);

            break;

        case WRITE:

            // assert (m_cpu.isOddCycle() == false );

            m_bus.write(0x2004, m_data);
            --m_oamCounter;

            if (m_dmcCounter == 0 && m_oamCounter == 0)
            {

                m_state = State::NONE;

                #ifdef DEBUG_DMA
                qDebug() << m_cpu.cycleNumber() + 1 << ": " << "OAM end (" << (m_cpu.cycleNumber() + 1 - debugDmaStart) << ")" << endl;
                #endif
            }
            else
            {

                if (m_oamCounter == 0 && m_dmcSkip > 0)
                {

                    assert(m_dmcCounter > 0);

                    if (m_dmcSkip == 3 || m_dmcSkip == 5)
                    {
                        m_state = State::READ;

                        #ifdef DEBUG_DMA
                        qDebug() << m_cpu.cycleNumber() + 1 << ": " << "second-to-last put";
                        #endif
                    }
                    else if (m_dmcSkip == 1)
                    {

                        m_state = State::START_DMC;
                        dmcReload = false;

                        #ifdef DEBUG_DMA
                        qDebug() << m_cpu.cycleNumber() + 1 << ": " << "last put";
                        #endif
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

        #ifdef DEBUG_DMA
        qDebug() << m_cpu.cycleNumber() << ": " << "OAM Request";
        #endif

        if(m_state == State::NONE || m_state == State::START_DMC) m_state = State::START_OAM;
        m_oamAddr = addr;
        m_oamCounter = 512;
    }

    void dmcRequest(uint16_t addr, bool reload) {

        //assert(m_dmcCounter == 0);

        if(m_state == State::NONE) {
            m_state = State::START_DMC;
            
            dmcReload = reload;
        }
        else {

            #ifdef DEBUG_DMA
            qDebug() << m_cpu.cycleNumber() << ": AQUI : " << m_oamCounter;
            #endif

               //last put           //second-to-last put
            if(m_oamCounter == 1 || m_oamCounter == 3) {
                m_dmcSkip = m_oamCounter;
             }
            else if(m_oamCounter == 0 && m_cpu.isHalted()) {
                m_state = State::START_DMC;
                dmcReload = true;
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
