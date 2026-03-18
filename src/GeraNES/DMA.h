#pragma once

#include <cstdint>

#include "defines.h"
#include "IBus.h"
#include "Console.h"
#include "APU/APU.h"
#include "Serialization.h"

class DMA
{
private:
    Ibus& m_bus;
    Console& m_console;

    bool m_dmaNeedHalt = false;
    bool m_dmaNeedDummyRead = false;

    bool m_oamDmaTransfer = false;
    uint8_t m_oamDmaPage = 0;
    uint16_t m_oamDmaCounter = 0;
    uint8_t m_oamDmaReadAddr = 0;
    uint8_t m_oamDmaData = 0;

    bool m_dmcDmaRunning = false;
    bool m_dmcAbortPending = false;
    bool m_dmcSingleCycleAbortPending = false;
    bool m_dmcLastRequestWasReload = false;
    uint16_t m_dmcDmaAddr = 0;
    uint16_t m_dmaPrevReadAddr = 0;

    bool m_dmaReadInProgress = false;
    uint8_t m_dmaReadInputClockMask = 0;

    GERANES_INLINE uint8_t inputClockMaskForAddr(uint16_t addr) const
    {
        switch(addr) {
            case 0x4016: return 0x01;
            case 0x4017: return 0x02;
            default: return 0x00;
        }
    }

    GERANES_INLINE uint8_t controllerOpenBusMask(uint16_t /*addr*/) const
    {
        return 0xE0;
    }

    uint8_t dmaBusRead(uint16_t addr, bool enableInputClock)
    {
        m_dmaReadInProgress = true;
        m_dmaReadInputClockMask = enableInputClock ? inputClockMaskForAddr(addr) : 0x00;
        const uint8_t value = m_bus.read(addr);
        m_dmaReadInProgress = false;
        m_dmaReadInputClockMask = 0x00;
        return value;
    }

    uint8_t processDmaRead(uint16_t dmaAddr, bool enableInternalRegReads, bool skipInputRead)
    {
        if(!enableInternalRegReads) {
            m_dmaPrevReadAddr = dmaAddr;
            if(dmaAddr >= 0x4000 && dmaAddr <= 0x401F) {
                return m_bus.getOpenBus();
            }
            return dmaBusRead(dmaAddr, false);
        }

        uint16_t internalAddr = static_cast<uint16_t>(0x4000 | (dmaAddr & 0x1F));
        bool isSameAddress = internalAddr == dmaAddr;
        uint8_t value = 0;

        switch(internalAddr) {
            case 0x4015:
                value = dmaBusRead(internalAddr, false);
                if(!isSameAddress) {
                    dmaBusRead(dmaAddr, false);
                }
                break;

            case 0x4016:
            case 0x4017:
                if(skipInputRead || m_dmaPrevReadAddr == internalAddr) {
                    value = dmaBusRead(internalAddr, false);
                } else {
                    value = dmaBusRead(internalAddr, true);
                }

                if(!isSameAddress) {
                    const uint8_t externalValue = dmaBusRead(dmaAddr, false);
                    const uint8_t obMask = controllerOpenBusMask(internalAddr);
                    value = static_cast<uint8_t>(
                        (externalValue & obMask) |
                        (value & ~obMask)
                    );
                    m_bus.setOpenBus(value);
                }
                break;

            default:
                value = dmaBusRead(dmaAddr, false);
                break;
        }

        m_dmaPrevReadAddr = internalAddr;
        return value;
    }

    template<typename CpuType>
    void startDmaCycle(CpuType& cpu, bool& suppressInstructionCycleAccounting)
    {
        if(m_dmaNeedHalt) {
            m_dmaNeedHalt = false;
        } else if(m_dmaNeedDummyRead) {
            m_dmaNeedDummyRead = false;
        }

        suppressInstructionCycleAccounting = true;
        cpu.beginCycle();
    }

public:
    DMA(Ibus& bus, Console& console) : m_bus(bus), m_console(console)
    {
    }

    void init()
    {
        m_dmaNeedHalt = false;
        m_dmaNeedDummyRead = false;
        m_oamDmaTransfer = false;
        m_oamDmaPage = 0;
        m_oamDmaCounter = 0;
        m_oamDmaReadAddr = 0;
        m_oamDmaData = 0;
        m_dmcDmaRunning = false;
        m_dmcAbortPending = false;
        m_dmcSingleCycleAbortPending = false;
        m_dmcLastRequestWasReload = false;
        m_dmcDmaAddr = 0;
        m_dmaPrevReadAddr = 0;
        m_dmaReadInProgress = false;
        m_dmaReadInputClockMask = 0;
    }

    void startOamDma(uint16_t addr)
    {
        m_oamDmaTransfer = true;
        m_oamDmaPage = static_cast<uint8_t>(addr >> 8);
        m_oamDmaCounter = 0;
        m_oamDmaReadAddr = 0;
        m_dmaNeedHalt = true;
    }

    void startDmcDma(uint16_t addr, bool reload)
    {
        m_dmcDmaAddr = addr;
        m_dmcDmaRunning = true;
        m_dmcLastRequestWasReload = reload;
        m_dmaNeedDummyRead = true;
        m_dmaNeedHalt = true;
    }

    void cancelDmcDma()
    {
        if(!m_dmcDmaRunning) {
            return;
        }

        if(m_dmaNeedHalt) {
            if(m_dmcLastRequestWasReload) {
                m_dmcSingleCycleAbortPending = true;
            }
            m_dmcDmaRunning = false;
            m_dmaNeedDummyRead = false;
            m_dmaNeedHalt = false;
        } else {
            m_dmcAbortPending = true;
        }
    }

    void scheduleImplicitDmcSingleCycleAbort()
    {
        m_dmcSingleCycleAbortPending = true;
        m_dmcDmaRunning = false;
        m_dmcAbortPending = false;
        m_dmaNeedDummyRead = false;
        m_dmaNeedHalt = false;
    }

    bool isDmaInputClockEnabled(uint16_t addr) const
    {
        return m_dmaReadInProgress && (m_dmaReadInputClockMask & inputClockMaskForAddr(addr)) != 0;
    }

    bool isDmaReadInProgress() const
    {
        return m_dmaReadInProgress;
    }

    void processPending(
        uint16_t readAddress,
        bool writeCycle,
        bool& suppressInstructionCycleAccounting
    );

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_dmaNeedHalt);
        SERIALIZEDATA(s, m_dmaNeedDummyRead);
        SERIALIZEDATA(s, m_oamDmaTransfer);
        SERIALIZEDATA(s, m_oamDmaPage);
        SERIALIZEDATA(s, m_oamDmaCounter);
        SERIALIZEDATA(s, m_oamDmaReadAddr);
        SERIALIZEDATA(s, m_oamDmaData);
        SERIALIZEDATA(s, m_dmcDmaRunning);
        SERIALIZEDATA(s, m_dmcAbortPending);
        SERIALIZEDATA(s, m_dmcSingleCycleAbortPending);
        SERIALIZEDATA(s, m_dmcLastRequestWasReload);
        SERIALIZEDATA(s, m_dmcDmaAddr);
        SERIALIZEDATA(s, m_dmaPrevReadAddr);
    }
};
