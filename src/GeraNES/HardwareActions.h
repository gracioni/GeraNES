#pragma once

#include <cstdint>
#include <string>

#include "Serialization.h"
#include "logger/logger.h"

class HardwareActions
{
private:
    static constexpr uint8_t VS_INSERT_COIN_FRAMES = 4;
    static constexpr uint8_t VS_SERVICE_BUTTON_FRAMES = 4;
    static constexpr uint8_t FDS_ACTION_SWITCH_DISK_SIDE = 0x01;
    static constexpr uint8_t FDS_ACTION_EJECT_DISK = 0x02;
    static constexpr uint8_t FDS_ACTION_INSERT_NEXT_DISK = 0x04;

    uint8_t m_vsInsertCoinFrames[4] = {0, 0, 0, 0}; // slots 1..4
    uint8_t m_vsServiceFrames[2] = {0, 0}; // service buttons 1..2
    uint8_t m_fdsPendingActions = 0;

public:
    void reset()
    {
        m_vsInsertCoinFrames[0] = m_vsInsertCoinFrames[1] = m_vsInsertCoinFrames[2] = m_vsInsertCoinFrames[3] = 0;
        m_vsServiceFrames[0] = m_vsServiceFrames[1] = 0;
        m_fdsPendingActions = 0;
    }

    void onFrameStart()
    {
        for(uint8_t& v : m_vsInsertCoinFrames) {
            if(v > 0) --v;
        }
        for(uint8_t& v : m_vsServiceFrames) {
            if(v > 0) --v;
        }
    }

    uint8_t applyVsSystemRead4016(uint8_t data, bool isVsSystem) const
    {
        if(!isVsSystem) return data;

        if(m_vsServiceFrames[0] > 0) data |= 0x04;
        if(m_vsInsertCoinFrames[0] > 0) data |= 0x20;
        if(m_vsInsertCoinFrames[1] > 0) data |= 0x40;
        return data;
    }

    uint8_t applyVsSystemRead4017(uint8_t data, bool isVsSystem) const
    {
        if(!isVsSystem) return data;

        if(m_vsServiceFrames[1] > 0) data |= 0x04;
        if(m_vsInsertCoinFrames[2] > 0) data |= 0x20;
        if(m_vsInsertCoinFrames[3] > 0) data |= 0x40;
        return data;
    }

    void fdsSwitchDiskSide()
    {
        m_fdsPendingActions |= FDS_ACTION_SWITCH_DISK_SIDE;
        Logger::instance().log("FDS Switch Disk Side", Logger::Type::USER);
    }

    void fdsEjectDisk()
    {
        m_fdsPendingActions |= FDS_ACTION_EJECT_DISK;
        Logger::instance().log("FDS Eject Disk", Logger::Type::USER);
    }

    void fdsInsertNextDisk()
    {
        m_fdsPendingActions |= FDS_ACTION_INSERT_NEXT_DISK;
        Logger::instance().log("FDS Insert Next Disk", Logger::Type::USER);
    }

    uint8_t consumeFdsPendingActions(bool isFdsSystem)
    {
        if(!isFdsSystem) return 0;
        const uint8_t pending = m_fdsPendingActions;
        m_fdsPendingActions = 0;
        return pending;
    }

    void vsInsertCoin(int slot, bool isVsSystem)
    {
        if(slot < 1 || slot > 4) return;
        if(!isVsSystem) return;

        m_vsInsertCoinFrames[slot - 1] = VS_INSERT_COIN_FRAMES;
        Logger::instance().log("VS Insert Coin " + std::to_string(slot), Logger::Type::USER);
    }

    void vsServiceButton(int button, bool isVsSystem)
    {
        if(button < 1 || button > 2) return;
        if(!isVsSystem) return;

        m_vsServiceFrames[button - 1] = VS_SERVICE_BUTTON_FRAMES;
        Logger::instance().log("VS Service Button " + std::to_string(button), Logger::Type::USER);
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_vsInsertCoinFrames[0]);
        SERIALIZEDATA(s, m_vsInsertCoinFrames[1]);
        SERIALIZEDATA(s, m_vsInsertCoinFrames[2]);
        SERIALIZEDATA(s, m_vsInsertCoinFrames[3]);
        SERIALIZEDATA(s, m_vsServiceFrames[0]);
        SERIALIZEDATA(s, m_vsServiceFrames[1]);
        SERIALIZEDATA(s, m_fdsPendingActions);
    }
};
