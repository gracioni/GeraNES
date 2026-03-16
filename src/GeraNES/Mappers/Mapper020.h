#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "BaseMapper.h"
#include "GeraNES/NesCartridgeData/_FdsFormat.h"
#include "GeraNES/util/FileUtil.h"

class Mapper020 : public BaseMapper
{
private:
    static constexpr uint8_t FDS_ACTION_SWITCH_DISK_SIDE = 0x01;
    static constexpr uint8_t FDS_ACTION_EJECT_DISK = 0x02;
    static constexpr uint8_t FDS_ACTION_INSERT_NEXT_DISK = 0x04;
    static constexpr uint32_t NO_DISK_INSERTED = 0xFFFFFFFFu;
    static constexpr int BIOS_SIZE = 0x2000;
    static constexpr int WRAM_SIZE = 0x8000;

    _FdsFormat& m_fds;
    std::array<uint8_t, WRAM_SIZE> m_wram = {};
    std::array<uint8_t, BIOS_SIZE> m_bios = {};
    std::vector<std::vector<uint8_t>> m_diskSides;

    bool m_biosLoaded = false;

    uint16_t m_irqReloadValue = 0;
    uint16_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    bool m_irqRepeatEnabled = false;

    bool m_diskRegEnabled = true;
    bool m_soundRegEnabled = true;

    uint8_t m_writeDataReg = 0;
    bool m_motorOn = false;
    bool m_resetTransfer = false;
    bool m_readMode = false;
    bool m_crcControl = false;
    bool m_diskReady = false;
    bool m_diskIrqEnabled = false;
    uint8_t m_extConWriteReg = 0;

    bool m_badCrc = false;
    bool m_endOfHead = false;
    uint8_t m_readDataReg = 0;
    bool m_transferComplete = false;

    uint32_t m_diskNumber = NO_DISK_INSERTED;
    uint32_t m_diskPosition = 0;
    uint32_t m_delay = 0;
    uint16_t m_crcAccumulator = 0;
    bool m_previousCrcControlFlag = false;
    bool m_gapEnded = true;
    bool m_scanningDisk = false;
    bool m_verticalMirroring = false;
    int32_t m_autoDiskEjectCounter = -1;
    int32_t m_autoDiskSwitchCounter = -1;
    int32_t m_restartAutoInsertCounter = -1;
    uint32_t m_frameCounter = 0;
    uint32_t m_lastDiskCheckFrame = 0;
    uint32_t m_successiveChecks = 0;
    uint32_t m_previousDiskNumber = NO_DISK_INSERTED;

    bool m_timerIrqPending = false;
    bool m_diskIrqPending = false;

    void loadBios()
    {
        std::vector<uint8_t> biosData;
        if(readBinaryFile("disksys.rom", biosData) && biosData.size() >= BIOS_SIZE) {
            std::copy_n(biosData.begin(), BIOS_SIZE, m_bios.begin());
            m_biosLoaded = true;
        } else {
            m_bios.fill(0);
            m_biosLoaded = false;
        }
    }

    void addGaps(const std::vector<uint8_t>& source, std::vector<uint8_t>& out)
    {
        out.clear();
        out.reserve(source.size() + 4096);

        out.insert(out.end(), 28300 / 8, 0);

        for(size_t pos = 0; pos < source.size();) {
            const uint8_t blockType = source[pos];
            size_t blockLength = 1;

            switch(blockType) {
            case 0x01: blockLength = 56; break;
            case 0x02: blockLength = 2; break;
            case 0x03: blockLength = 16; break;
            case 0x04:
                if(pos >= 3) {
                    blockLength = 1 + static_cast<size_t>(source[pos - 3] | (source[pos - 2] << 8));
                } else {
                    blockLength = source.size() - pos;
                }
                break;
            default:
                out.push_back(0x80);
                out.insert(out.end(), source.begin() + static_cast<std::ptrdiff_t>(pos), source.end());
                if(out.size() < static_cast<size_t>(_FdsFormat::FDS_SIDE_SIZE)) {
                    out.resize(_FdsFormat::FDS_SIDE_SIZE, 0);
                }
                return;
            }

            if(pos + blockLength > source.size()) {
                blockLength = source.size() - pos;
            }

            out.push_back(0x80);
            out.insert(out.end(), source.begin() + static_cast<std::ptrdiff_t>(pos), source.begin() + static_cast<std::ptrdiff_t>(pos + blockLength));

            // Fake CRC bytes, as done by Mesen for standard FDS images.
            out.push_back(0x4D);
            out.push_back(0x62);

            out.insert(out.end(), 976 / 8, 0);
            pos += blockLength;
        }

        if(out.size() < static_cast<size_t>(_FdsFormat::FDS_SIDE_SIZE)) {
            out.resize(_FdsFormat::FDS_SIDE_SIZE, 0);
        }
    }

    void rebuildDiskSides()
    {
        m_diskSides.clear();
        m_diskSides.reserve(static_cast<size_t>(m_fds.sideCount()));
        for(int i = 0; i < m_fds.sideCount(); ++i) {
            std::vector<uint8_t> side;
            addGaps(m_fds.sideData(i), side);
            m_diskSides.push_back(std::move(side));
        }
    }

    bool isDiskInserted() const
    {
        return m_diskNumber != NO_DISK_INSERTED;
    }

    uint8_t readFdsDisk()
    {
        return m_diskSides[static_cast<size_t>(m_diskNumber)][static_cast<size_t>(m_diskPosition)];
    }

    void updateCrc(uint8_t value)
    {
        m_crcAccumulator ^= value;
        for(uint16_t n = 0; n < 8; n++) {
            uint8_t carry = static_cast<uint8_t>(m_crcAccumulator & 1);
            m_crcAccumulator >>= 1;
            if(carry) {
                m_crcAccumulator ^= 0x8408;
            }
        }
    }

    void clockIrq()
    {
        if(m_irqEnabled) {
            if(m_irqCounter == 0) {
                m_timerIrqPending = true;
                m_irqCounter = m_irqReloadValue;
                if(!m_irqRepeatEnabled) {
                    m_irqEnabled = false;
                }
            } else {
                --m_irqCounter;
            }
        }
    }

    void processDisk()
    {
        if(!isDiskInserted() || !m_motorOn) {
            m_endOfHead = true;
            m_scanningDisk = false;
            return;
        }

        if(m_resetTransfer && !m_scanningDisk) {
            return;
        }

        if(m_endOfHead) {
            m_delay = 50000;
            m_endOfHead = false;
            m_diskPosition = 0;
            m_gapEnded = false;
            return;
        }

        if(m_delay > 0) {
            --m_delay;
            return;
        }

        m_scanningDisk = true;
        m_autoDiskEjectCounter = -1;
        m_autoDiskSwitchCounter = -1;
        uint8_t diskData = 0;
        bool needIrq = m_diskIrqEnabled;

        if(m_readMode) {
            diskData = readFdsDisk();

            if(!m_previousCrcControlFlag) {
                updateCrc(diskData);
            }

            if(!m_diskReady) {
                m_gapEnded = false;
                m_crcAccumulator = 0;
                m_badCrc = false;
            } else if(diskData != 0 && !m_gapEnded) {
                m_gapEnded = true;
                needIrq = false;
            }

            if(m_gapEnded) {
                m_transferComplete = true;
                m_readDataReg = diskData;
                if(needIrq) {
                    m_diskIrqPending = true;
                }
            }

            if(!m_previousCrcControlFlag && m_crcControl) {
                m_badCrc = m_crcAccumulator != 0;
            }
        } else {
            uint8_t diskData = 0;
            if(!m_crcControl) {
                m_transferComplete = true;
                diskData = m_writeDataReg;
                if(needIrq) {
                    m_diskIrqPending = true;
                }
            }

            if(!m_diskReady) {
                diskData = 0x00;
                m_crcAccumulator = 0;
            }

            if(!m_crcControl) {
                updateCrc(diskData);
            } else {
                diskData = static_cast<uint8_t>(m_crcAccumulator & 0xFF);
                m_crcAccumulator >>= 8;
            }

            m_diskSides[static_cast<size_t>(m_diskNumber)][static_cast<size_t>(m_diskPosition)] = diskData;
            m_gapEnded = false;
            m_badCrc = false;
        }

        m_previousCrcControlFlag = m_crcControl;

        ++m_diskPosition;
        if(m_diskPosition >= m_diskSides[static_cast<size_t>(m_diskNumber)].size()) {
            m_motorOn = false;
            if(m_diskIrqEnabled) {
                m_diskIrqPending = true;
            }
            m_autoDiskEjectCounter = 77;
        } else {
            m_delay = 149;
        }
    }

    void clearDiskTransferState()
    {
        m_transferComplete = false;
        m_diskIrqPending = false;
        m_readDataReg = 0;
        m_previousCrcControlFlag = false;
        m_gapEnded = true;
        m_scanningDisk = false;
        m_diskPosition = 0;
        m_delay = 0;
        m_endOfHead = false;
        m_badCrc = false;
        m_crcAccumulator = 0;
    }

    void applyFdsActions(uint8_t pending)
    {
        if((pending & FDS_ACTION_EJECT_DISK) != 0) {
            m_diskNumber = NO_DISK_INSERTED;
            m_scanningDisk = false;
            m_endOfHead = true;
            m_previousDiskNumber = NO_DISK_INSERTED;
        }
        if((pending & FDS_ACTION_INSERT_NEXT_DISK) != 0) {
            if(!m_diskSides.empty()) {
                if(m_diskNumber == NO_DISK_INSERTED) {
                    m_diskNumber = 0;
                } else {
                    m_diskNumber = (m_diskNumber + 1) % static_cast<uint32_t>(m_diskSides.size());
                }
            }
            m_previousDiskNumber = m_diskNumber;
            clearDiskTransferState();
        }
        if((pending & FDS_ACTION_SWITCH_DISK_SIDE) != 0) {
            if(!m_diskSides.empty()) {
                if(m_diskNumber == NO_DISK_INSERTED) {
                    m_diskNumber = 0;
                } else {
                    m_diskNumber ^= 1;
                    if(m_diskNumber >= static_cast<uint32_t>(m_diskSides.size())) {
                        m_diskNumber = 0;
                    }
                }
            }
            m_previousDiskNumber = m_diskNumber;
            clearDiskTransferState();
        }
    }

    void insertSequentialDisk()
    {
        if(m_diskSides.empty() || m_diskNumber != NO_DISK_INSERTED) {
            return;
        }

        if(m_previousDiskNumber == NO_DISK_INSERTED) {
            m_diskNumber = 0;
        } else {
            m_diskNumber = (m_previousDiskNumber + 1) % static_cast<uint32_t>(m_diskSides.size());
        }

        clearDiskTransferState();
    }

    void processAutoDiskInsert()
    {
        if(m_autoDiskEjectCounter > 0) {
            --m_autoDiskEjectCounter;
        } else if(m_autoDiskSwitchCounter > 0) {
            --m_autoDiskSwitchCounter;
            if(m_autoDiskSwitchCounter == 0) {
                insertSequentialDisk();
                m_restartAutoInsertCounter = 200;
            }
        } else if(m_restartAutoInsertCounter > 0) {
            --m_restartAutoInsertCounter;
            if(m_restartAutoInsertCounter == 0 && !m_scanningDisk) {
                m_previousDiskNumber = NO_DISK_INSERTED;
                m_autoDiskEjectCounter = 34;
                m_autoDiskSwitchCounter = -1;
            }
        }
    }

public:
    Mapper020(ICartridgeData& cd)
        : BaseMapper(cd)
        , m_fds(*dynamic_cast<_FdsFormat*>(&cd))
    {
    }

    void reset() override
    {
        m_wram.fill(0);
        loadBios();
        rebuildDiskSides();

        m_irqReloadValue = 0;
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_irqRepeatEnabled = false;
        m_diskRegEnabled = true;
        m_soundRegEnabled = true;
        m_writeDataReg = 0;
        m_motorOn = false;
        m_resetTransfer = false;
        m_readMode = false;
        m_crcControl = false;
        m_diskReady = false;
        m_diskIrqEnabled = false;
        m_extConWriteReg = 0;
        m_verticalMirroring = false;
        m_timerIrqPending = false;
        m_diskIrqPending = false;
        m_diskNumber = m_diskSides.empty() ? NO_DISK_INSERTED : 0;
        m_autoDiskEjectCounter = -1;
        m_autoDiskSwitchCounter = -1;
        m_restartAutoInsertCounter = -1;
        m_frameCounter = 0;
        m_lastDiskCheckFrame = 0;
        m_successiveChecks = 0;
        m_previousDiskNumber = m_diskNumber;
        clearDiskTransferState();
    }

    MirroringType mirroringType() override
    {
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    uint8_t readSaveRam(int addr) override
    {
        return m_wram[static_cast<size_t>(addr & 0x1FFF)];
    }

    void writeSaveRam(int addr, uint8_t data) override
    {
        m_wram[static_cast<size_t>(addr & 0x1FFF)] = data;
    }

    uint8_t readPrg(int addr) override
    {
        if(addr < 0x6000) {
            return m_wram[static_cast<size_t>(0x2000 + addr)];
        }
        return m_bios[static_cast<size_t>(addr - 0x6000)];
    }

    void writePrg(int addr, uint8_t data) override
    {
        if(addr < 0x6000) {
            m_wram[static_cast<size_t>(0x2000 + addr)] = data;
        }
    }

    bool getInterruptFlag() override
    {
        return m_timerIrqPending || m_diskIrqPending;
    }

    void cycle() override
    {
        clockIrq();
        processDisk();
    }

    void onScanlineStart(bool /*renderingEnabled*/, int scanline) override
    {
        if(scanline == 0) {
            ++m_frameCounter;
            processAutoDiskInsert();
        }
    }

    uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if(!m_diskRegEnabled || addr > 0x033) {
            return openBusData;
        }

        switch(addr) {
        case 0x030:
        {
            uint8_t value = openBusData & 0x24;
            value |= m_timerIrqPending ? 0x01 : 0x00;
            value |= m_transferComplete ? 0x02 : 0x00;
            value |= m_verticalMirroring ? 0x00 : 0x08;
            value |= 0x00;
            m_transferComplete = false;
            m_timerIrqPending = false;
            m_diskIrqPending = false;
            return value;
        }
        case 0x031:
            m_transferComplete = false;
            m_diskIrqPending = false;
            return m_readDataReg;
        case 0x032:
        {
            uint8_t value = openBusData & 0xF8;
            value |= !isDiskInserted() ? 0x01 : 0x00;
            value |= (!isDiskInserted() || !m_scanningDisk) ? 0x02 : 0x00;
            value |= !isDiskInserted() ? 0x04 : 0x00;

            if(m_frameCounter - m_lastDiskCheckFrame < 100) {
                ++m_successiveChecks;
            } else {
                m_successiveChecks = 0;
            }
            m_lastDiskCheckFrame = m_frameCounter;

            if(m_successiveChecks > 20 && m_autoDiskEjectCounter == 0 && m_autoDiskSwitchCounter == -1 && isDiskInserted()) {
                m_lastDiskCheckFrame = 0;
                m_successiveChecks = 0;
                m_autoDiskSwitchCounter = 77;
                m_previousDiskNumber = m_diskNumber;
                m_diskNumber = NO_DISK_INSERTED;
                clearDiskTransferState();
            }
            return value;
        }
        case 0x033:
            return m_extConWriteReg;
        default:
            return openBusData;
        }
    }

    void writeMapperRegister(int addr, uint8_t value) override
    {
        if((!m_diskRegEnabled && addr >= 0x024 && addr <= 0x026) || (!m_soundRegEnabled && addr >= 0x040)) {
            return;
        }

        switch(addr) {
        case 0x020:
            m_irqReloadValue = static_cast<uint16_t>((m_irqReloadValue & 0xFF00) | value);
            break;
        case 0x021:
            m_irqReloadValue = static_cast<uint16_t>((m_irqReloadValue & 0x00FF) | (static_cast<uint16_t>(value) << 8));
            break;
        case 0x022:
            m_irqRepeatEnabled = (value & 0x01) == 0x01;
            m_irqEnabled = (value & 0x02) == 0x02 && m_diskRegEnabled;
            if(m_irqEnabled) {
                m_irqCounter = m_irqReloadValue;
            } else {
                m_timerIrqPending = false;
            }
            break;
        case 0x023:
            m_diskRegEnabled = (value & 0x01) == 0x01;
            m_soundRegEnabled = (value & 0x02) == 0x02;
            if(!m_diskRegEnabled) {
                m_irqEnabled = false;
                m_timerIrqPending = false;
                m_diskIrqPending = false;
            }
            break;
        case 0x024:
            m_writeDataReg = value;
            m_transferComplete = false;
            m_diskIrqPending = false;
            break;
        case 0x025:
            m_motorOn = (value & 0x01) == 0x01;
            m_resetTransfer = (value & 0x02) == 0x02;
            m_readMode = (value & 0x04) == 0x04;
            m_verticalMirroring = (value & 0x08) == 0x00;
            m_crcControl = (value & 0x10) == 0x10;
            m_diskReady = (value & 0x40) == 0x40;
            m_diskIrqEnabled = (value & 0x80) == 0x80;
            m_diskIrqPending = false;
            break;
        case 0x026:
            m_extConWriteReg = value;
            break;
        default:
            break;
        }
    }

    void applyExternalActions(uint8_t pending) override
    {
        applyFdsActions(pending);
    }
};
