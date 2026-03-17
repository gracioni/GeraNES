#pragma once

#include "IBus.h"
#include "APU/APU.h"
#include "AccuracyTrace.h"

class CPU2A03;

class DMA {

private:

    Ibus& m_bus;
    CPU2A03& m_cpu;
    SampleChannel& m_sampleChannel;

    uint16_t m_oamAddr;
    int m_oamCounter;

    enum class State {NONE, START_OAM, START_DMC, DUMMY, READ, WRITE} m_state;

    uint16_t m_dmcAddr;
    int m_dmcCounter;

    enum class DeferredDmcStart {NONE, AFTER_FINAL_OAM_WRITE, AFTER_FINAL_OAM_READ_SLOT};
    enum class DmcRequestType {NONE, LOAD, RELOAD};

    DeferredDmcStart m_deferredDmcStart;
    DmcRequestType m_dmcRequestType;
    bool m_dmcAbortPending;
    uint16_t m_dmcHaltAddr;

    uint8_t m_data;

    bool hasPendingDmcRequest() const
    {
        return m_dmcCounter > 0;
    }

    bool isReloadDmcRequest() const
    {
        return m_dmcRequestType == DmcRequestType::RELOAD;
    }

    bool shouldStartDmcHalt() const
    {
        return !m_cpu.isOpcodeWriteCycle() && (isReloadDmcRequest() || (!isReloadDmcRequest() && m_cpu.isDmaGetCycle()));
    }

    bool shouldPreferCpuSensitiveReadStart() const
    {
        if(isReloadDmcRequest()) {
            return false;
        }

        const uint16_t addr = m_cpu.pendingBusAddr();
        return addr >= 0x2000 && addr <= 0x3FFF;
    }

    void clearPendingDmcRequest()
    {
        m_dmcCounter = 0;
        m_deferredDmcStart = DeferredDmcStart::NONE;
        m_dmcRequestType = DmcRequestType::NONE;
        m_dmcAbortPending = false;
    }

    void logDmcStart() const
    {
        AccuracyTrace::log(
            std::string("DMA START_DMC reload=") + (isReloadDmcRequest() ? "1" : "0") +
            " cpuCycle=" + std::to_string(m_cpu.cycleCounter()) +
            " get=" + std::to_string(m_cpu.isDmaGetCycle() ? 1 : 0) +
            " pendingBusAddr=" + std::to_string(m_cpu.pendingBusAddr()) +
            " lastBusAddr=" + std::to_string(m_cpu.lastBusAddr()) +
            " visibleBusAddr=" + std::to_string(m_cpu.visibleBusAddr())
        );
    }

    void beginDmcHalt()
    {
        m_dmcHaltAddr = m_cpu.dmcHaltBusAddr();
        logDmcStart();
        m_cpu.halt(1);
        m_state = State::DUMMY;
    }

    bool executeDmcRead()
    {
        if (!hasPendingDmcRequest() || m_deferredDmcStart != DeferredDmcStart::NONE) {
            return false;
        }

        AccuracyTrace::log(
            "DMA READ sample cpuCycle=" + std::to_string(m_cpu.cycleCounter()) +
            " dmcAddr=" + std::to_string(m_dmcAddr)
        );
        m_data = m_bus.read(m_dmcAddr);
        m_sampleChannel.loadSampleBuffer(m_data);
        --m_dmcCounter;
        m_dmcRequestType = DmcRequestType::NONE;
        return true;
    }

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
        SERIALIZEDATA(s, m_deferredDmcStart);
        SERIALIZEDATA(s, m_dmcRequestType);
        SERIALIZEDATA(s, m_dmcAbortPending);
        SERIALIZEDATA(s, m_dmcHaltAddr);
    }

    void init() {

        m_oamAddr = 0;
        m_oamCounter = 0;

        m_state = State::NONE;

        m_dmcAddr = 0;
        m_dmcCounter = 0;

        m_deferredDmcStart = DeferredDmcStart::NONE;
        m_dmcRequestType = DmcRequestType::NONE;
        m_dmcAbortPending = false;
        m_dmcHaltAddr = 0;

        m_data = 0;
    }

    void cycle();

    void tryStartPendingLoadOnCpuRead()
    {
        if(m_state == State::START_DMC &&
           m_deferredDmcStart == DeferredDmcStart::NONE &&
           hasPendingDmcRequest() &&
           shouldPreferCpuSensitiveReadStart() &&
           !m_cpu.isOpcodeWriteCycle())
        {
            beginDmcHalt();
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
        AccuracyTrace::log(
            std::string("DMA REQUEST reload=") + (reload ? "1" : "0") +
            " cpuCycle=" + std::to_string(m_cpu.cycleCounter()) +
            " state=" + std::to_string(static_cast<int>(m_state)) +
            " get=" + std::to_string(m_cpu.isDmaGetCycle() ? 1 : 0) +
            " pendingBusAddr=" + std::to_string(m_cpu.pendingBusAddr()) +
            " lastBusAddr=" + std::to_string(m_cpu.lastBusAddr()) +
            " visibleBusAddr=" + std::to_string(m_cpu.visibleBusAddr())
        );

        if(m_state == State::NONE) {
            m_state = State::START_DMC;
            m_deferredDmcStart = DeferredDmcStart::NONE;
            m_dmcRequestType = reload ? DmcRequestType::RELOAD : DmcRequestType::LOAD;
        }
        else {

            if(m_oamCounter <= 2) {
                m_deferredDmcStart = DeferredDmcStart::AFTER_FINAL_OAM_WRITE;
            }
            else if(m_oamCounter <= 4) {
                m_deferredDmcStart = DeferredDmcStart::AFTER_FINAL_OAM_READ_SLOT;
            }
        }

        m_dmcAddr = addr;
        m_dmcCounter = 1;
    }

    void cancelDmcRequest() {
        AccuracyTrace::log(
            "DMA CANCEL cpuCycle=" + std::to_string(m_cpu.cycleCounter()) +
            " state=" + std::to_string(static_cast<int>(m_state))
        );

        if(!hasPendingDmcRequest()) {
            return;
        }

        if(m_state == State::START_DMC) {
            clearPendingDmcRequest();

            if(m_oamCounter == 0) {
                m_state = State::NONE;
            }
        } else if(m_deferredDmcStart != DeferredDmcStart::NONE) {
            clearPendingDmcRequest();
        } else if(m_state == State::DUMMY || m_state == State::READ) {
            m_dmcAbortPending = true;
        }
    }

};

#include "CPU2A03.h"

inline void DMA::cycle()
{
    if (m_state == State::NONE)
        return;

    switch (m_state)
    {

    case State::START_OAM:

        if (!m_cpu.isOpcodeWriteCycle())
        {
            m_cpu.halt(1);
            m_state = State::READ;
        }
        break;

    case State::START_DMC:   

        if (shouldStartDmcHalt())
        {
            beginDmcHalt();
        }
        break;

    case State::DUMMY:

        AccuracyTrace::log(
            "DMA DUMMY cpuCycle=" + std::to_string(m_cpu.cycleCounter()) +
            " pendingBusAddr=" + std::to_string(m_cpu.pendingBusAddr()) +
            " lastBusAddr=" + std::to_string(m_cpu.lastBusAddr()) +
            " visibleBusAddr=" + std::to_string(m_cpu.visibleBusAddr())
        );
        m_cpu.halt(1);

        m_bus.read(m_dmcHaltAddr);

        if(m_dmcAbortPending) {
            clearPendingDmcRequest();
            m_state = m_oamCounter > 0 ? State::READ : State::NONE;
            break;
        }

        m_state = State::READ;

        break;

    case State::READ:

        if(m_cpu.isDmaGetCycle())
        {
            if (executeDmcRead())
            {
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
            m_bus.read(m_dmcHaltAddr);
        }

        m_cpu.halt(1);

        break;

    case State::WRITE:

        m_bus.write(0x2004, m_data);

        --m_oamCounter;

        if (m_dmcCounter == 0 && m_oamCounter == 0)
        {
            assert(m_deferredDmcStart == DeferredDmcStart::NONE);

            m_state = State::NONE;
        }
        else
        {

            if (m_oamCounter == 0 && m_deferredDmcStart != DeferredDmcStart::NONE)
            {
                assert(m_dmcCounter > 0);

                if (m_deferredDmcStart == DeferredDmcStart::AFTER_FINAL_OAM_READ_SLOT)
                {
                    m_state = State::READ;
                }
                else if (m_deferredDmcStart == DeferredDmcStart::AFTER_FINAL_OAM_WRITE)
                {
                    m_state = State::START_DMC;
                }

                m_deferredDmcStart = DeferredDmcStart::NONE;
            }
            else
                m_state = State::READ;
        }

        m_cpu.halt(1);

        break;

    default:
        break;

    }

}
