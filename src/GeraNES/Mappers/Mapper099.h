#pragma once

#include "BaseMapper.h"

// Nintendo Vs. System (iNES Mapper 99)
class Mapper099 : public BaseMapper
{
private:
    bool m_bankSelect = false; // OUT2 ($4016 bit 2)
    static constexpr uint8_t INSERT_COIN_FRAMES = 4;
    static constexpr uint8_t SERVICE_BUTTON_FRAMES = 4;
    uint8_t m_insertCoinFrames[4] = {0, 0, 0, 0}; // slots 1..4
    uint8_t m_serviceFrames[2] = {0, 0}; // service buttons 1..2

public:
    Mapper099(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        // $8000-$9FFF: switchable between bank 0 and bank 4 using OUT2
        // $A000-$FFFF: fixed to banks 1,2,3.
        switch((addr >> 13) & 0x03) {
        case 0:
        {
            const uint8_t bank = static_cast<uint8_t>(m_bankSelect ? 4 : 0);
            const uint8_t mask = calculateMask(cd().numberOfPRGBanks<BankSize::B8K>());
            return cd().readPrg<BankSize::B8K>(bank & mask, addr);
        }
        case 1: return cd().readPrg<BankSize::B8K>(1, addr);
        case 2: return cd().readPrg<BankSize::B8K>(2, addr);
        default: return cd().readPrg<BankSize::B8K>(3, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t bank = static_cast<uint8_t>(m_bankSelect ? 1 : 0);
        if(hasChrRam()) return readChrRam<BankSize::B8K>(bank, addr);
        return cd().readChr<BankSize::B8K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t bank = static_cast<uint8_t>(m_bankSelect ? 1 : 0);
        writeChrRam<BankSize::B8K>(bank, addr, data);
    }

    GERANES_HOT void onCpuWrite(uint16_t addr, uint8_t data) override
    {
        if(addr == 0x4016) {
            m_bankSelect = (data & 0x04) != 0;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData = 0) override
    {
        // This method is also called for $4016/$4017 reads in GeraNESEmu.
        uint8_t data = openBusData;
        if(addr == 0x016) {
            if(m_serviceFrames[0] > 0) data |= 0x04;
            if(m_insertCoinFrames[0] > 0) data |= 0x20;
            if(m_insertCoinFrames[1] > 0) data |= 0x40;
        }
        else if(addr == 0x017) {
            if(m_serviceFrames[1] > 0) data |= 0x04;
            if(m_insertCoinFrames[2] > 0) data |= 0x20;
            if(m_insertCoinFrames[3] > 0) data |= 0x40;
        }
        return data;
    }

    GERANES_HOT void onScanlineStart(bool /*renderingEnabled*/, int scanline) override
    {
        if(scanline != 0) return;

        for(uint8_t& v : m_insertCoinFrames) {
            if(v > 0) --v;
        }
        for(uint8_t& v : m_serviceFrames) {
            if(v > 0) --v;
        }
    }

    bool onHardwareAction(HardwareActionType type, int parameter = 0) override
    {
        switch(type) {
        case HardwareActionType::VS_INSERT_COIN:
            if(parameter >= 1 && parameter <= 4) {
                m_insertCoinFrames[parameter - 1] = INSERT_COIN_FRAMES;
                return true;
            }
            return false;
        case HardwareActionType::VS_SERVICE_BUTTON:
            if(parameter >= 1 && parameter <= 2) {
                m_serviceFrames[parameter - 1] = SERVICE_BUTTON_FRAMES;
                return true;
            }
            return false;
        default:
            return false;
        }
    }

    void reset() override
    {
        m_bankSelect = false;
        m_insertCoinFrames[0] = m_insertCoinFrames[1] = m_insertCoinFrames[2] = m_insertCoinFrames[3] = 0;
        m_serviceFrames[0] = m_serviceFrames[1] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bankSelect);
        SERIALIZEDATA(s, m_insertCoinFrames[0]);
        SERIALIZEDATA(s, m_insertCoinFrames[1]);
        SERIALIZEDATA(s, m_insertCoinFrames[2]);
        SERIALIZEDATA(s, m_insertCoinFrames[3]);
        SERIALIZEDATA(s, m_serviceFrames[0]);
        SERIALIZEDATA(s, m_serviceFrames[1]);
    }
};
