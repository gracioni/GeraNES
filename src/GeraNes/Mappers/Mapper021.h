#ifndef MAPPER021_H
#define MAPPER021_H

#include "IMapper.h"


//VRC4a VRC4c
class Mapper021 : public IMapper
{

private:

    static const int16_t PRESCALER_RELOAD = 341; //114 114 113
    static const int16_t PRESCALER_DEC = 3;


    uint8_t m_mirroring = 0;

    bool m_interruptFlag = false;
    uint8_t m_IRQCounter = 0;
    uint8_t m_IRQReload = 0;

    bool m_IRQMode = false; //(0=scanline mode, 1=CPU cycle mode)
    bool m_IRQEnable = false;
    bool m_IRQEnableOnAck = false;

    int16_t m_prescaler = 0;

protected:

    uint8_t m_CHRREGMask = 0;
    uint8_t m_CHRReg[8] = {0};
    uint8_t m_PRGREGMask = 0;

    bool m_PRGMode = false;
    uint8_t m_PRGReg[2] = {0};

    //address expected $x000, $x001, $x002, $x003
    void writeVRCxx(int addr, uint8_t data, bool hasPRGMode)
    {
        switch(addr)
        {
        case 0x0000:
        case 0x0001:
        case 0x0002:
        case 0x0003:
            m_PRGReg[0] = data & 0x1F & m_PRGREGMask;
            break;

        case 0x1000:
        case 0x1001:
            m_mirroring = data & 0x03;
            break;

        case 0x1002:
        case 0x1003:
            if(hasPRGMode) m_PRGMode = data & 0x02;
            else m_mirroring = data & 0x03;
            break;

        case 0x2000:
        case 0x2001:
        case 0x2002:
        case 0x2003:
            m_PRGReg[1] = data & 0x1F & m_PRGREGMask;
            break;
        case 0x3000: m_CHRReg[0] = (m_CHRReg[0]&0xF0) | (data&0x0F); break;
        case 0x3001: m_CHRReg[0] = (m_CHRReg[0]&0x0F) | (data<<4); break;
        case 0x3002: m_CHRReg[1] = (m_CHRReg[1]&0xF0) | (data&0x0F); break;
        case 0x3003: m_CHRReg[1] = (m_CHRReg[1]&0x0F) | (data<<4); break;
        case 0x4000: m_CHRReg[2] = (m_CHRReg[2]&0xF0) | (data&0x0F); break;
        case 0x4001: m_CHRReg[2] = (m_CHRReg[2]&0x0F) | (data<<4); break;
        case 0x4002: m_CHRReg[3] = (m_CHRReg[3]&0xF0) | (data&0x0F); break;
        case 0x4003: m_CHRReg[3] = (m_CHRReg[3]&0x0F) | (data<<4); break;
        case 0x5000: m_CHRReg[4] = (m_CHRReg[4]&0xF0) | (data&0x0F); break;
        case 0x5001: m_CHRReg[4] = (m_CHRReg[4]&0x0F) | (data<<4); break;
        case 0x5002: m_CHRReg[5] = (m_CHRReg[5]&0xF0) | (data&0x0F); break;
        case 0x5003: m_CHRReg[5] = (m_CHRReg[5]&0x0F) | (data<<4); break;
        case 0x6000: m_CHRReg[6] = (m_CHRReg[6]&0xF0) | (data&0x0F); break;
        case 0x6001: m_CHRReg[6] = (m_CHRReg[6]&0x0F) | (data<<4); break;
        case 0x6002: m_CHRReg[7] = (m_CHRReg[7]&0xF0) | (data&0x0F); break;
        case 0x6003: m_CHRReg[7] = (m_CHRReg[7]&0x0F) | (data<<4); break;

        case 0x7000: m_IRQReload = (m_IRQReload&0xF0) | (data&0x0F); break;
        case 0x7001: m_IRQReload = (m_IRQReload&0x0F) | (data<<4); break;

        case 0x7002:
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

        case 0x7003:
            m_interruptFlag = false;
            m_IRQEnable = m_IRQEnableOnAck;
            break;

        }
    }


public:

    Mapper021(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGREGMask = calculateMask(m_cartridgeData.numberOfPRGBanks<W8K>());
        m_CHRREGMask = calculateMask(m_cartridgeData.numberOfCHRBanks<W1K>());
    }

    virtual void writePRG32k(int addr, uint8_t data) override
    {

        // VRC4c -> 0xF00C
        // VRC4a -> 0xF006

        addr = (addr & 0xF000)| ((addr & 0x00C0) >> 6)|  ((addr & 0x0006)>>1);
        writeVRCxx(addr,data,true);

    }

    uint8_t readPRG32k(int addr) override
    {
        if(!m_PRGMode) {

            switch(addr>>13) { // addr/8192
            case 0: return m_cartridgeData.readPRG<W8K>(m_PRGReg[0],addr);
            case 1: return m_cartridgeData.readPRG<W8K>(m_PRGReg[1],addr);
            case 2: return m_cartridgeData.readPRG<W8K>(m_cartridgeData.numberOfPRGBanks<W8K>()-2,addr);
            case 3: return m_cartridgeData.readPRG<W8K>(m_cartridgeData.numberOfPRGBanks<W8K>()-1,addr);
            }
        }
        else {

            switch(addr>>13) { // addr/8192
            case 0: return m_cartridgeData.readPRG<W8K>(m_cartridgeData.numberOfPRGBanks<W8K>()-2,addr);
            case 1: return m_cartridgeData.readPRG<W8K>(m_PRGReg[1],addr);
            case 2: return m_cartridgeData.readPRG<W8K>(m_PRGReg[0],addr);
            case 3: return m_cartridgeData.readPRG<W8K>(m_cartridgeData.numberOfPRGBanks<W8K>()-1,addr);
            }

        }

        return 0;
    }

    virtual uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM()) return IMapper::readCHR8k(addr);

        size_t index = addr >> 10;
        uint8_t bank = m_CHRReg[index];

        return m_cartridgeData.readCHR<W1K>(bank&m_CHRREGMask,addr); // addr/1024
    }

    MirroringType mirroringType(void) override
    {
        switch(m_mirroring) {
        case 0: return IMapper::VERTICAL;
        case 1: return IMapper::HORIZONTAL;
        case 2: return IMapper::SINGLE_SCREEN_A;
        case 3: return IMapper::SINGLE_SCREEN_B;
        }

        return IMapper::FOUR_SCREEN;
    }

    virtual bool getInterruptFlag(void) override
    {
        return m_interruptFlag;
    }

    void cycle(void) override
    {
        if(!m_IRQEnable) return;

        if(!m_IRQMode) //divider ~113.666667 CPU cycles 114 114 113
        {
            if (m_prescaler > 0)
            {
                m_prescaler -= PRESCALER_DEC;
                return;
            }

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
        IMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGREGMask);
        SERIALIZEDATA(s, m_CHRREGMask);

        SERIALIZEDATA(s, m_PRGMode);

        s.array(m_PRGReg,1,2);
        s.array(m_CHRReg,1,8);

        SERIALIZEDATA(s, m_mirroring);

        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_IRQCounter);
        SERIALIZEDATA(s, m_IRQReload);

        SERIALIZEDATA(s, m_IRQMode);
        SERIALIZEDATA(s, m_IRQEnable);
        SERIALIZEDATA(s, m_IRQEnableOnAck);

        SERIALIZEDATA(s, m_prescaler);
    }

};

#endif
