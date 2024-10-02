#ifndef MAPPER065_H
#define MAPPER065_H

#include "BaseMapper.h"

class Mapper065 : public BaseMapper
{
private:

    uint8_t m_PRGReg0 = 0x00;
    uint8_t m_PRGReg1 = 0x01;
    uint8_t m_PRGReg2 = 0xFE;

    uint8_t m_CHRReg[8];

    uint8_t m_PRGMask = 0;
    uint8_t m_CHRMask = 0;

    bool m_mirroring = false;

    bool m_IRQEnable = false;
    uint16_t m_IRQCounter = 0;
    uint16_t m_IRQReload = 0;

    bool m_interruptFlag = false;


public:

    Mapper065(ICartridgeData& cd) : BaseMapper(cd)
    {
        memset(m_CHRReg, 0x00, 8);

        m_PRGMask = calculateMask(m_cd.numberOfPRGBanks<W8K>());
        m_CHRMask = calculateMask(m_cd.numberOfCHRBanks<W1K>());

        m_PRGReg0 &= m_PRGMask;
        m_PRGReg1 &= m_PRGMask;
        m_PRGReg2 &= m_PRGMask;
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch(addr>>12)
        {
        case 0x00: m_PRGReg0 = data&m_PRGMask; break;
        case 0x02: m_PRGReg1 = data&m_PRGMask; break;
        case 0x04: m_PRGReg2 = data&m_PRGMask; break;

        case 0x03: m_CHRReg[addr&0x07] = data&m_CHRMask; break;

        case 0x01:
            switch(addr&0x07) {
            case 1: m_mirroring = data & 0x80; break;
            case 3: m_IRQEnable = data & 0x80; m_interruptFlag = false; break;
            case 4: m_IRQCounter = m_IRQReload; m_interruptFlag = false; break;
            case 5: m_IRQReload = (m_IRQReload&0x00FF) | (((uint16_t)data)<<8); break;
            case 6: m_IRQReload =  (m_IRQReload&0xFF00) | data; break;
            }

            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x2000) return m_cd.readPrg<W8K>(m_PRGReg0,addr);
        else if(addr < 0x4000) return m_cd.readPrg<W8K>(m_PRGReg1,addr);
        else if(addr < 0x6000) return m_cd.readPrg<W8K>(m_PRGReg2,addr);

        return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-1,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        return m_cd.readChr<W1K>(m_CHRReg[(addr >> 10)&0x07],addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_mirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    GERANES_HOT void cycle() override
    {
        if(m_IRQEnable) {

            if(m_IRQCounter > 0) m_IRQCounter--;

            if(m_IRQCounter == 0){
                m_interruptFlag = true;
                m_IRQEnable = false;
            }
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGReg0);
        SERIALIZEDATA(s, m_PRGReg1);
        SERIALIZEDATA(s, m_PRGReg2);

        s.array(m_CHRReg,1,8);

        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_CHRMask);

        SERIALIZEDATA(s, m_mirroring);

        SERIALIZEDATA(s, m_IRQEnable);
        SERIALIZEDATA(s, m_IRQCounter);
        SERIALIZEDATA(s, m_IRQReload);

        SERIALIZEDATA(s, m_interruptFlag);

    }

};

#endif // MAPPER065_H
