#pragma once

#include "BaseMapper.h"

class Mapper018 : public BaseMapper
{

private:

    uint8_t m_PRGREGMask = 0;
    uint8_t m_CHRREGMask = 0;

    uint8_t m_CHRReg[8] = {0};
    uint8_t m_PRGReg[3] = {0};

    union {
        uint16_t value;
        struct {
            uint8_t lowByte;
            uint8_t highByte;
        };
    } m_IRQReload = { .value = 0 };

    uint16_t m_IRQCounter = 0;

    uint16_t m_IRQCounterMask = 0xFFFF;

    bool m_IRQEnable = false;

    bool m_interruptFlag = false;

    uint8_t m_mirroring = 0;

    static void setLowNible(uint8_t& reg, uint8_t data, uint8_t mask = 0xFF) {
        reg &= 0xF0;
        reg |= data & 0x0F;
        reg &= mask;
    }

    static void setHighNible(uint8_t& reg, uint8_t data, uint8_t mask = 0xFF) {
        reg &= 0x0F;
        reg |= data << 4;
        reg &= mask;
    }

    void updateIRQCounterMask(uint8_t IRQSize)
    {
        if      (IRQSize & 0x4) m_IRQCounterMask = 0x000F;
        else if (IRQSize & 0x2) m_IRQCounterMask = 0x00FF;
        else if (IRQSize & 0x1) m_IRQCounterMask = 0x0FFF;
        else m_IRQCounterMask = 0xFFFF;
    }

public:

    Mapper018(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGREGMask = calculateMask(m_cd.numberOfPRGBanks<W8K>());
        m_CHRREGMask = calculateMask(m_cd.numberOfCHRBanks<W1K>());

    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {

        switch(addr) {

        case 0x0000: setLowNible(m_PRGReg[0],data,m_PRGREGMask); break;
        case 0x0001: setHighNible(m_PRGReg[0],data,m_PRGREGMask); break;
        case 0x0002: setLowNible(m_PRGReg[1],data,m_PRGREGMask); break;
        case 0x0003: setHighNible(m_PRGReg[1],data,m_PRGREGMask); break;
        case 0x1000: setLowNible(m_PRGReg[2],data,m_PRGREGMask); break;
        case 0x1001: setHighNible(m_PRGReg[2],data,m_PRGREGMask); break;

        case 0x2000: setLowNible(m_CHRReg[0],data,m_CHRREGMask); break;
        case 0x2001: setHighNible(m_CHRReg[0],data,m_CHRREGMask); break;

        case 0x2002: setLowNible(m_CHRReg[1],data,m_CHRREGMask); break;
        case 0x2003: setHighNible(m_CHRReg[1],data,m_CHRREGMask); break;

        case 0x3000: setLowNible(m_CHRReg[2],data,m_CHRREGMask); break;
        case 0x3001: setHighNible(m_CHRReg[2],data,m_CHRREGMask); break;

        case 0x3002: setLowNible(m_CHRReg[3],data,m_CHRREGMask); break;
        case 0x3003: setHighNible(m_CHRReg[3],data,m_CHRREGMask); break;

        case 0x4000: setLowNible(m_CHRReg[4],data,m_CHRREGMask); break;
        case 0x4001: setHighNible(m_CHRReg[4],data,m_CHRREGMask); break;

        case 0x4002: setLowNible(m_CHRReg[5],data,m_CHRREGMask); break;
        case 0x4003: setHighNible(m_CHRReg[5],data,m_CHRREGMask); break;

        case 0x5000: setLowNible(m_CHRReg[6],data,m_CHRREGMask); break;
        case 0x5001: setHighNible(m_CHRReg[6],data,m_CHRREGMask); break;

        case 0x5002: setLowNible(m_CHRReg[7],data,m_CHRREGMask); break;
        case 0x5003: setHighNible(m_CHRReg[7],data,m_CHRREGMask); break;

        case 0x6000: setLowNible(m_IRQReload.lowByte,data); break;
        case 0x6001: setHighNible(m_IRQReload.lowByte,data); break;
        case 0x6002: setLowNible(m_IRQReload.highByte,data); break;
        case 0x6003: setHighNible(m_IRQReload.highByte,data); break;

        case 0x7000: m_IRQCounter = m_IRQReload.value; m_interruptFlag = false; break;

        case 0x7001:
            m_IRQEnable = data & 0x01;
            updateIRQCounterMask((data >> 1) & 0x07);
            m_interruptFlag = false;
            break;

        case 0x7002: m_mirroring = data & 0x03; break;

        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        int index = addr >> 10; // addr/0x400
        return m_cd.readChr<W1K>(m_CHRReg[index],addr);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch(addr>>13) { // addr/8192
        case 0: return m_cd.readPrg<W8K>(m_PRGReg[0],addr);
        case 1: return m_cd.readPrg<W8K>(m_PRGReg[1],addr);
        case 2: return m_cd.readPrg<W8K>(m_PRGReg[2],addr);
        case 3: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-1,addr);
        }

        return 0;
    }


    GERANES_HOT void cycle() override
    {
        if(m_IRQEnable) {

            uint16_t aux = m_IRQCounter & m_IRQCounterMask;
            if(aux-- == 0) m_interruptFlag = true;

            m_IRQCounter &= ~m_IRQCounterMask;
            m_IRQCounter |= aux & m_IRQCounterMask;
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }


    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroring){
        case 0: return MirroringType::HORIZONTAL;
        case 1: return MirroringType::VERTICAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::FOUR_SCREEN;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGREGMask);
        SERIALIZEDATA(s, m_CHRREGMask);

        s.array(m_CHRReg, 1, 8);
        s.array(m_PRGReg, 1, 3);

        SERIALIZEDATA(s, m_IRQReload.value);

        SERIALIZEDATA(s, m_IRQCounter);

        SERIALIZEDATA(s, m_IRQCounterMask);

        SERIALIZEDATA(s, m_IRQEnable);

        SERIALIZEDATA(s, m_interruptFlag);

        SERIALIZEDATA(s, m_mirroring);
    }

};
