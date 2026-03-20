#pragma once

#include "BaseMapper.h"
#include <cstring>
#include "Audio/Vrc6Audio.h"

//VRC6a
class Mapper024 : public BaseMapper
{

private:

    static const int16_t PRESCALER_RELOAD = 341; //114 114 113
    static const int16_t PRESCALER_DEC = 3;


    uint8_t m_mirroring = 0;
    uint8_t m_ppuBankMode = 0;

    bool m_interruptFlag = false;
    uint8_t m_IRQCounter = 0;
    uint8_t m_IRQReload = 0;

    bool m_IRQMode = false; //(0=scanline mode, 1=CPU cycle mode)
    bool m_IRQEnable = false;
    bool m_IRQEnableOnAck = false;

    int16_t m_prescaler = 0;

    Vrc6Audio m_audio;

    GERANES_INLINE uint8_t chrRegIndexFor1kSlot(uint8_t slot) const
    {
        switch(m_ppuBankMode & 0x03) {
        case 0:
            // Mode 0: 8 x 1KB banks (R0..R7)
            return slot & 0x07;
        case 1:
            // Mode 1: 4 x 2KB banks from R0..R3
            return static_cast<uint8_t>((slot >> 1) & 0x03);
        default:
            // Mode 2/3: R0..R3 as 1KB at $0000-$0FFF, R4..R5 as 2KB at $1000-$1FFF
            if(slot < 4) return slot;
            return static_cast<uint8_t>(4 + ((slot - 4) >> 1));
        }
    }


protected:

    uint8_t m_CHRREGMask = 0;
    uint8_t m_CHRReg[8] = {0};
    uint8_t m_PRG16REGMask = 0;
    uint8_t m_PRGREGMask = 0;

    uint8_t m_PRGReg[2] = {0};

    //address expected $x000, $x001, $x002, $x003
    void writeVRC6x(int addr, uint8_t data, bool hasPRGMode)
    {
        switch(addr)
        {
        case 0x0000:
        case 0x0001:
        case 0x0002:
        case 0x0003:
            m_PRGReg[0] = data & m_PRG16REGMask;
            break;

        case 0x1000:
        case 0x1001:
        case 0x1002:
        case 0x2000:
        case 0x2001:
        case 0x2002:
        case 0x3000:
        case 0x3001:
        case 0x3002:
            m_audio.writeRegister(addr, data);
            break;

        case 0x3003:
            m_mirroring = (data>>2) & 0x03;
            m_ppuBankMode = data & 0x03;
            break;

        case 0x4000:
        case 0x4001:
        case 0x4002:
        case 0x4003:
            m_PRGReg[1] = data & m_PRGREGMask;
            break;

        case 0x5000: m_CHRReg[0] = data&m_CHRREGMask; break;
        case 0x5001: m_CHRReg[1] = data&m_CHRREGMask; break;
        case 0x5002: m_CHRReg[2] = data&m_CHRREGMask; break;
        case 0x5003: m_CHRReg[3] = data&m_CHRREGMask; break;
        case 0x6000: m_CHRReg[4] = data&m_CHRREGMask; break;
        case 0x6001: m_CHRReg[5] = data&m_CHRREGMask; break;
        case 0x6002: m_CHRReg[6] = data&m_CHRREGMask; break;
        case 0x6003: m_CHRReg[7] = data&m_CHRREGMask; break;


        case 0x7000: m_IRQReload = data; break;

        case 0x7001:
            m_IRQMode = data & 0x04;
            m_IRQEnable = data & 0x02;
            m_IRQEnableOnAck = data & 0x01;

            if(m_IRQEnable) {
                m_IRQCounter = m_IRQReload;
                m_prescaler = PRESCALER_RELOAD;
                //m_prescaler = 0;
            }

            m_interruptFlag = false;

            break;

        case 0x7002:
            m_interruptFlag = false;
            m_IRQEnable = m_IRQEnableOnAck;
            break;

        }
    }


public:

    Mapper024(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRG16REGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_PRGREGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_CHRREGMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
    }

    GERANES_HOT virtual void writePrg(int addr, uint8_t data) override
    {
        // VRC6a:    A0, A1    $x000, $x001, $x002, $x003 -> 0xF003

        addr &= 0xF003;
        writeVRC6x(addr,data,true);

    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch(addr>>13) { // addr/8192
        case 0: return cd().readPrg<BankSize::B8K>(m_PRGReg[0]<<1,addr);
        case 1: return cd().readPrg<BankSize::B8K>((m_PRGReg[0]<<1)+1,addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_PRGReg[1],addr);
        case 3: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>()-1,addr);
        }

        return 0;
    }

    GERANES_HOT virtual uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        const uint8_t slot = static_cast<uint8_t>(addr >> 10);
        const uint8_t regIndex = chrRegIndexFor1kSlot(slot);
        uint8_t bank = m_CHRReg[regIndex];

        return cd().readChr<BankSize::B1K>(bank,addr); // addr/1024
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroring) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::FOUR_SCREEN;
    }

    GERANES_HOT virtual bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT float getExpansionAudioSample() override
    {
        return m_audio.getSample();
    }

    float getMixWeight() const override
    {
        return m_audio.getMixWeight();
    }

    std::string getAudioChannelsJson() const override
    {
        return m_audio.getAudioChannelsJson();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        return m_audio.setAudioChannelVolumeById(id, volume);
    }

    void reset() override
    {
        m_mirroring = 0;
        m_ppuBankMode = 0;
        m_interruptFlag = false;
        m_IRQCounter = 0;
        m_IRQReload = 0;
        m_IRQMode = false;
        m_IRQEnable = false;
        m_IRQEnableOnAck = false;
        m_prescaler = 0;
        m_PRGReg[0] = 0;
        m_PRGReg[1] = 0;
        memset(m_CHRReg, 0, sizeof(m_CHRReg));

        m_audio.reset();
    }

    GERANES_HOT void cycle() override
    {
        m_audio.clock();

        if(!m_IRQEnable) return;

        if(!m_IRQMode) //divider ~113.666667 CPU cycles 114 114 113
        {
            m_prescaler -= PRESCALER_DEC;
            if(m_prescaler > 0) return;
            m_prescaler += PRESCALER_RELOAD;
        }

        if (m_IRQCounter != 0xFF) {
            ++m_IRQCounter;
            return;
        }

        m_IRQCounter = m_IRQReload;

        m_interruptFlag = true;
    }



    virtual void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGREGMask);
        SERIALIZEDATA(s, m_PRG16REGMask);
        SERIALIZEDATA(s, m_CHRREGMask);

        s.array(m_PRGReg,1,2);
        s.array(m_CHRReg,1,8);

        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_ppuBankMode);

        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_IRQCounter);
        SERIALIZEDATA(s, m_IRQReload);

        SERIALIZEDATA(s, m_IRQMode);
        SERIALIZEDATA(s, m_IRQEnable);
        SERIALIZEDATA(s, m_IRQEnableOnAck);

        SERIALIZEDATA(s, m_prescaler);

        m_audio.serialization(s);
    }

};
