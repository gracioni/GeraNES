#pragma once

#include "APU/APU.h"

inline void DMA::processPending(
    uint16_t readAddress,
    bool writeCycle
)
{
    CPU2A03& cpu = m_console.cpu();
    const bool getCycleNow = cpu.isDmaGetCycle();

    if(m_dmcSingleCycleAbortPending) {
        if(m_dmcSingleCycleAbortDelay > 0) {
            --m_dmcSingleCycleAbortDelay;
            return;
        }
        if(writeCycle) {
            m_dmcSingleCycleAbortPending = false;
            return;
        }

        cpu.beginCycle();
        cpu.endCycle<DmaCycle::Yes>();
        m_dmcSingleCycleAbortPending = false;

        if(!m_oamDmaTransfer) {
            return;
        }
    }

    if(m_dmcDmaRunning && m_dmaNeedHalt && m_dmcInitialLoadPhasePending) {
        if(!getCycleNow) {
            return;
        }
        m_dmcInitialLoadPhasePending = false;
    }

    if(!m_dmaNeedHalt || writeCycle) {
        return;
    }

    bool enableInternalRegReads = (readAddress & 0xFFE0) == 0x4000;
    const bool controllerReadAddress = readAddress == 0x4016 || readAddress == 0x4017;
    const bool skipDummyReads = controllerReadAddress;
    const bool dmcControllerGetConflict =
        enableInternalRegReads &&
        m_dmcDmaRunning &&
        controllerReadAddress &&
        ((m_dmcDmaAddr & 0x1F) == (readAddress & 0x1F));
    m_dmaPrevReadAddr = readAddress;

    startDmaCycle(cpu);
    if(!(m_dmcAbortPending && skipDummyReads)) {
        dmaBusRead(readAddress, !controllerReadAddress || !dmcControllerGetConflict);
    }
    cpu.endCycle<DmaCycle::Yes>();

    if(m_dmcAbortPending) {
        m_dmcDmaRunning = false;
        m_dmcAbortPending = false;
        if(!m_oamDmaTransfer) {
            m_dmaNeedDummyRead = false;
            m_dmaNeedHalt = false;
            return;
        }
    }

    while(m_dmcDmaRunning || m_oamDmaTransfer) {
        const bool getCycle = cpu.isDmaGetCycle();
        if(getCycle) {
            if(m_dmcDmaRunning && !m_dmaNeedHalt && !m_dmaNeedDummyRead) {
                startDmaCycle(cpu);
                if(m_dmcAbortPending) {
                    if(!skipDummyReads) {
                        dmaBusRead(readAddress, true);
                    }
                    cpu.endCycle<DmaCycle::Yes>();
                    m_dmcDmaRunning = false;
                    m_dmcAbortPending = false;
                    continue;
                }
                const uint8_t value = processDmaRead(m_dmcDmaAddr, enableInternalRegReads);
                cpu.endCycle<DmaCycle::Yes>();
                m_dmcDmaRunning = false;
                m_dmcAbortPending = false;
                m_dmcInitialLoadPhasePending = false;
                m_console.apu().getSampleChannel().reloadShiftRegister(value);
            } else if(m_oamDmaTransfer) {
                startDmaCycle(cpu);
                const uint16_t sourceAddr = static_cast<uint16_t>((m_oamDmaPage << 8) | m_oamDmaReadAddr);
                m_oamDmaData = processDmaRead(sourceAddr, enableInternalRegReads);
                cpu.endCycle<DmaCycle::Yes>();
                m_oamDmaReadAddr++;
                m_oamDmaCounter++;
            } else {
                startDmaCycle(cpu);
                if(!skipDummyReads) {
                    dmaBusRead(readAddress, true);
                }
                cpu.endCycle<DmaCycle::Yes>();
            }
        } else {
            if(m_oamDmaTransfer && (m_oamDmaCounter & 0x01)) {
                startDmaCycle(cpu);
                m_bus.write(0x2004, m_oamDmaData);
                cpu.endCycle<DmaCycle::Yes>();
                m_oamDmaCounter++;
                if(m_oamDmaCounter == 0x200) {
                    m_oamDmaTransfer = false;
                }
            } else {
                startDmaCycle(cpu);
                if(!skipDummyReads) {
                    dmaBusRead(readAddress, true);
                }
                cpu.endCycle<DmaCycle::Yes>();
            }
        }
    }
}
