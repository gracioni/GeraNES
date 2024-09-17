#ifndef MAPPER004_H
#define MAPPER004_H

#include "IMapper.h"

//MMC3
//TxROM
//(MMC6)
//(HxROM)
class Mapper004 : public IMapper
{

protected:

    uint8_t m_addrReg = 0;
    bool m_CHRMode = false;

    uint8_t m_CHRReg[6] = {0};


    uint8_t m_CHRMask = 0; //1k banks maks

private:

    bool m_PRGMode = false;

    uint8_t m_PRGReg0 = 0;
    uint8_t m_PRGReg1 = 0;

    uint8_t m_PRGMask = 0; //8k banks mask


    bool m_mirroring = false; //0=Vert 1=Horz Ignored when 4-screen

    bool m_enableWRAM = false;
    bool m_writeProtectWRAM = false;

    uint8_t m_reloadValue = 0;
    uint8_t m_scanlinesCounter = 0;
    bool m_enableInterrupt = false;

    bool m_reloadScanlineCounterFlag = false;

    bool m_interruptFlag = false;

    bool m_coolFlag = false; //hack flag to enable the interrupt only when a new reload value is set
                           //without this, the menu in the megaman3 last boss is glitched
                           //because improper interrupts happen
                           //however, some mmc3 tests fail :(

public:

    Mapper004(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGMask = calculateMask(m_cd.numberOfPRGBanks<W8K>());
        m_CHRMask = calculateMask(m_cd.numberOfCHRBanks<W1K>());
    }

    virtual ~Mapper004()
    {

    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(!m_PRGMode)
        {
            switch(addr >> 13) { // addr/8k
            case 0: return m_cd.readPrg<W8K>(m_PRGReg0,addr);
            case 1: return m_cd.readPrg<W8K>(m_PRGReg1,addr);
            case 2: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-2,addr);
            case 3: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-1,addr);
            }
        }
        else
        {
            switch(addr >> 13) {
            case 0: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-2,addr);
            case 1: return m_cd.readPrg<W8K>(m_PRGReg1,addr);
            case 2: return m_cd.readPrg<W8K>(m_PRGReg0,addr);
            case 3: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-1,addr);
            }
        }

        return 0;
    }

    GERANES_HOT virtual void writePrg(int addr, uint8_t data) override
    {
        addr &= 0xF001;

        switch(addr)
        {
        case 0x0000:
        case 0x1000:
            m_CHRMode = data & 0x80;
            m_PRGMode = data & 0x40;
            m_addrReg = data & 0x07;            

            break;
        case 0x0001:
        case 0x1001:

            switch(m_addrReg)
            {
            case 0: m_CHRReg[0] = data; break;
            case 1: m_CHRReg[1] = data; break;
            case 2: m_CHRReg[2] = data; break;
            case 3: m_CHRReg[3] = data; break;
            case 4: m_CHRReg[4] = data; break;
            case 5: m_CHRReg[5] = data; break;
            case 6: m_PRGReg0 = data&m_PRGMask; break;
            case 7: m_PRGReg1 = data&m_PRGMask; break;
            }
            break;
        case 0x2000:
        case 0x3000:
            m_mirroring = data & 0x01;
            break;
        case 0x2001:
        case 0x3001:
            m_enableWRAM = data & 0x80;
            m_writeProtectWRAM = data & 0x40;
            break;
        case 0x4000:
        case 0x5000:
            m_reloadValue = data;
            m_coolFlag = true;
            break;
        case 0x4001:
        case 0x5001:
            m_reloadScanlineCounterFlag = true;
            break;
        case 0x6000:
        case 0x7000:
            m_interruptFlag = false;
            m_enableInterrupt = false;
            break;
        case 0x6001:
        case 0x7001:
            m_enableInterrupt = true;
            break;
        }
    }


   GERANES_HOT virtual uint8_t readChr(int addr) override
    {
        if(hasVRAM()) return IMapper::readChr(addr);

        if(!m_CHRMode)
        {
            switch(addr >> 10) { // addr/1k
            case 0:
            case 1: return m_cd.readChr<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr);
            case 2:
            case 3: return m_cd.readChr<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr);
            case 4: return m_cd.readChr<W1K>(m_CHRReg[2]&m_CHRMask,addr);
            case 5: return m_cd.readChr<W1K>(m_CHRReg[3]&m_CHRMask,addr);
            case 6: return m_cd.readChr<W1K>(m_CHRReg[4]&m_CHRMask,addr);
            case 7: return m_cd.readChr<W1K>(m_CHRReg[5]&m_CHRMask,addr);
            }
        }
        else
        {
            switch(addr>>10) {
            case 0: return m_cd.readChr<W1K>(m_CHRReg[2]&m_CHRMask,addr);
            case 1: return m_cd.readChr<W1K>(m_CHRReg[3]&m_CHRMask,addr);
            case 2: return m_cd.readChr<W1K>(m_CHRReg[4]&m_CHRMask,addr);
            case 3: return m_cd.readChr<W1K>(m_CHRReg[5]&m_CHRMask,addr);
            case 4:
            case 5: return m_cd.readChr<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr);
            case 6:
            case 7: return m_cd.readChr<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr);
            }

        }

        return 0;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_cd.useFourScreenMirroring() ) return MirroringType::FOUR_SCREEN;
        if(m_mirroring) return MirroringType::HORIZONTAL;
        return MirroringType::VERTICAL;
    }

    bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    void tick() override
    {
        //should reload and set irq every clock when reloadValue == 0 (MMC3-C)
        if(m_reloadValue == 0 && m_scanlinesCounter == 0){
            m_scanlinesCounter = m_reloadValue;
            if(m_enableInterrupt) m_interruptFlag = true;
        }

        if(m_reloadScanlineCounterFlag || m_scanlinesCounter == 0) {
            m_scanlinesCounter = m_reloadValue;
            m_reloadScanlineCounterFlag = false;            
        }
        else if(m_scanlinesCounter > 0) m_scanlinesCounter--;

        if(m_scanlinesCounter == 0 && m_enableInterrupt) {

            if(m_coolFlag){
                m_interruptFlag = true;
                m_coolFlag = false;
            }

        }
    }

    virtual void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

        SERIALIZEDATA(s, m_CHRMode);
        SERIALIZEDATA(s, m_PRGMode);
        SERIALIZEDATA(s, m_addrReg);
        SERIALIZEDATA(s, m_CHRReg[0]);
        SERIALIZEDATA(s, m_CHRReg[1]);
        SERIALIZEDATA(s, m_CHRReg[2]);
        SERIALIZEDATA(s, m_CHRReg[3]);
        SERIALIZEDATA(s, m_CHRReg[4]);
        SERIALIZEDATA(s, m_CHRReg[5]);
        SERIALIZEDATA(s, m_PRGReg0);
        SERIALIZEDATA(s, m_PRGReg1);

        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_CHRMask);

        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_enableWRAM);
        SERIALIZEDATA(s, m_writeProtectWRAM);
        SERIALIZEDATA(s, m_reloadValue);
        SERIALIZEDATA(s, m_scanlinesCounter);
        SERIALIZEDATA(s, m_enableInterrupt);
        SERIALIZEDATA(s, m_reloadScanlineCounterFlag);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_coolFlag);
    }

};

#endif
