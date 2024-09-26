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
    uint8_t m_irqCounter = 0;
    bool m_enableInterrupt = false;

    bool m_irqClearFlag = false;

    bool m_interruptFlag = false;

    bool m_a12LastState = false;

    uint64_t m_fallingEdgeCycle = 0;

    bool m_mmc3RevAIrqs = false;

public:

    Mapper004(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGMask = calculateMask(m_cd.numberOfPRGBanks<W8K>());
        m_CHRMask = calculateMask(m_cd.numberOfCHRBanks<W1K>());

        m_mmc3RevAIrqs = cd.chip().substr(0, 5).compare("MMC3A") == 0;        
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
            m_CHRMode = data & 0x80;
            m_PRGMode = data & 0x40;
            m_addrReg = data & 0x07;            

            break;
        case 0x0001:
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
            m_mirroring = data & 0x01;
            break;
        case 0x2001:
            m_enableWRAM = data & 0x80;
            m_writeProtectWRAM = data & 0x40;
            break;
        case 0x4000: // 0xC000
            m_reloadValue = data;
            break;           
        case 0x4001: // 0xC001
            m_irqClearFlag = true;
            m_irqCounter = 0;
            break;
        case 0x6000: // 0xE000
            m_interruptFlag = false;
            m_enableInterrupt = false;
            break;
        case 0x6001:
            m_enableInterrupt = true;
            break;
        }
    }


   GERANES_HOT virtual uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return IMapper::readChr(addr);

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

    void setA12State(bool state, uint64_t ppuCycle) override
    {
        if(!m_a12LastState && state) {

            uint64_t diff = ppuCycle - m_fallingEdgeCycle;       

            if(diff > 12) {
                //std::cout << ppuCycle << std::endl;
                count();
            }
        }
        else if(m_a12LastState && !state) {
            m_fallingEdgeCycle = ppuCycle;
        }   

        m_a12LastState = state;      
    } 

    void count() {

        uint8_t count = m_irqCounter;

        if(m_irqCounter == 0 || m_irqClearFlag) {
            m_irqCounter = m_reloadValue;
        } else {
            m_irqCounter--;
        }

        if(m_mmc3RevAIrqs) {

            //MMC3 Revision A behavior
            if((count > 0 || m_irqClearFlag) && m_irqCounter == 0 && m_enableInterrupt) {
                m_interruptFlag = true;
            }
        } else {
            if(m_irqCounter == 0 && m_enableInterrupt) {
                m_interruptFlag = true;
            }
        }
        m_irqClearFlag = false;
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
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_enableInterrupt);
        SERIALIZEDATA(s, m_irqClearFlag);
        SERIALIZEDATA(s, m_interruptFlag);
        
        SERIALIZEDATA(s, m_a12LastState);    
        SERIALIZEDATA(s, m_fallingEdgeCycle);
    }

};

#endif
