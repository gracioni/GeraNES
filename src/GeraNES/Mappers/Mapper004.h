#pragma once

#include "BaseMapper.h"

//MMC3
//TxROM
//(MMC6)
//(HxROM)
class Mapper004 : public BaseMapper
{

protected:

    uint8_t m_addrReg = 0;
    bool m_chrMode = false;

    uint8_t m_chrReg[6] = {0};


    uint8_t m_chrMask = 0;

    bool m_prgMode = false;

    uint8_t m_prgReg0 = 0;
    uint8_t m_prgReg1 = 0;

    uint8_t m_prgMask = 0; //8k banks mask


    bool m_mirroring = false; //0=Vert 1=Horz Ignored when 4-screen

    bool m_enableWRAM = false;
    bool m_writeProtectWRAM = false;

    uint8_t m_reloadValue = 0;
    uint8_t m_irqCounter = 0;
    bool m_enableInterrupt = false;

    bool m_irqClearFlag = false;

    bool m_interruptFlag = false;

    bool m_a12LastState = true;

    uint8_t m_cycleCounter = 0;

    bool m_mmc3RevAIrqs = false;

    template<int WindowSize>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr) {
        if(hasChrRam()) return readChrRam<WindowSize>(bank, addr);
        return m_cd.readChr<WindowSize>(bank, addr);
    }

    template<int WindowSize>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data) {
        writeChrRam<WindowSize>(bank, addr, data);
    }

public:

    Mapper004(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(m_cd.numberOfPRGBanks<W8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize()/0x400);
        }
        else {
            m_chrMask = calculateMask(m_cd.numberOfCHRBanks<W1K>());
        }

        m_mmc3RevAIrqs = cd.chip().substr(0, 5).compare("MMC3A") == 0;        
    }

    virtual ~Mapper004()
    {

    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(!m_prgMode)
        {
            switch(addr >> 13) { // addr/8k
            case 0: return m_cd.readPrg<W8K>(m_prgReg0,addr);
            case 1: return m_cd.readPrg<W8K>(m_prgReg1,addr);
            case 2: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-2,addr);
            case 3: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-1,addr);
            }
        }
        else
        {
            switch(addr >> 13) {
            case 0: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-2,addr);
            case 1: return m_cd.readPrg<W8K>(m_prgReg1,addr);
            case 2: return m_cd.readPrg<W8K>(m_prgReg0,addr);
            case 3: return m_cd.readPrg<W8K>(m_cd.numberOfPRGBanks<W8K>()-1,addr);
            }
        }

        return 0;
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        addr &= 0xF001;

        switch(addr)
        {
        case 0x0000:
            m_chrMode = data & 0x80;
            m_prgMode = data & 0x40;
            m_addrReg = data & 0x07;            

            break;
        case 0x0001:
            switch(m_addrReg)
            {
            case 0: m_chrReg[0] = data; break;
            case 1: m_chrReg[1] = data; break;
            case 2: m_chrReg[2] = data; break;
            case 3: m_chrReg[3] = data; break;
            case 4: m_chrReg[4] = data; break;
            case 5: m_chrReg[5] = data; break;
            case 6: m_prgReg0 = data&m_prgMask; break;
            case 7: m_prgReg1 = data&m_prgMask; break;
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
        if(!m_chrMode)
        {
            switch(addr >> 10) { // addr/1k
                case 0:
                case 1: return readChrBank<W2K>((m_chrReg[0]&m_chrMask)>>1, addr);
                case 2:
                case 3: return readChrBank<W2K>((m_chrReg[1]&m_chrMask)>>1, addr);
                case 4: return readChrBank<W1K>(m_chrReg[2]&m_chrMask, addr);
                case 5: return readChrBank<W1K>(m_chrReg[3]&m_chrMask, addr);
                case 6: return readChrBank<W1K>(m_chrReg[4]&m_chrMask, addr);
                case 7: return readChrBank<W1K>(m_chrReg[5]&m_chrMask, addr);
            }
        }
        else
        {
            switch(addr>>10) {
                case 0: return readChrBank<W1K>(m_chrReg[2]&m_chrMask, addr);
                case 1: return readChrBank<W1K>(m_chrReg[3]&m_chrMask, addr);
                case 2: return readChrBank<W1K>(m_chrReg[4]&m_chrMask, addr);
                case 3: return readChrBank<W1K>(m_chrReg[5]&m_chrMask, addr);
                case 4:
                case 5: return readChrBank<W2K>((m_chrReg[0]&m_chrMask)>>1, addr);
                case 6:
                case 7: return readChrBank<W2K>((m_chrReg[1]&m_chrMask)>>1, addr);
            }

        }

        return 0;
    }

    GERANES_HOT virtual void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        // writeChrRam<W8K>(0, addr, data);
        // return;

        if(!m_chrMode)
        {
            switch(addr >> 10) { // addr/1k
                case 0:
                case 1: writeChrBank<W2K>((m_chrReg[0]&m_chrMask)>>1, addr, data); break;
                case 2:
                case 3: writeChrBank<W2K>((m_chrReg[1]&m_chrMask)>>1, addr, data); break;
                case 4: writeChrBank<W1K>(m_chrReg[2]&m_chrMask, addr, data); break;
                case 5: writeChrBank<W1K>(m_chrReg[3]&m_chrMask, addr, data); break;
                case 6: writeChrBank<W1K>(m_chrReg[4]&m_chrMask, addr, data); break;
                case 7: writeChrBank<W1K>(m_chrReg[5]&m_chrMask, addr, data); break;
            }
        }
        else
        {
            switch(addr>>10) {
                case 0: writeChrBank<W1K>(m_chrReg[2]&m_chrMask, addr, data); break;
                case 1: writeChrBank<W1K>(m_chrReg[3]&m_chrMask, addr, data); break;
                case 2: writeChrBank<W1K>(m_chrReg[4]&m_chrMask, addr, data); break;
                case 3: writeChrBank<W1K>(m_chrReg[5]&m_chrMask, addr, data); break;
                case 4:
                case 5: writeChrBank<W2K>((m_chrReg[0]&m_chrMask)>>1, addr, data); break;
                case 6:
                case 7: writeChrBank<W2K>((m_chrReg[1]&m_chrMask)>>1, addr, data); break;
            }

        }

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

    void setA12State(bool state) override {
  
        if(!m_a12LastState && state) {    

            if(m_cycleCounter > 3) {
                count();
            }
        }
        else if(m_a12LastState && !state) {
            m_cycleCounter = 0;
        }   

        m_a12LastState = state;      
    }

    void cycle() override {

        if((uint8_t)(m_cycleCounter+1) != 0)
            m_cycleCounter++;
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
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_chrMode);
        SERIALIZEDATA(s, m_prgMode);
        SERIALIZEDATA(s, m_addrReg);
        SERIALIZEDATA(s, m_chrReg[0]);
        SERIALIZEDATA(s, m_chrReg[1]);
        SERIALIZEDATA(s, m_chrReg[2]);
        SERIALIZEDATA(s, m_chrReg[3]);
        SERIALIZEDATA(s, m_chrReg[4]);
        SERIALIZEDATA(s, m_chrReg[5]);
        SERIALIZEDATA(s, m_prgReg0);
        SERIALIZEDATA(s, m_prgReg1);

        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);

        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_enableWRAM);
        SERIALIZEDATA(s, m_writeProtectWRAM);
        SERIALIZEDATA(s, m_reloadValue);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_enableInterrupt);
        SERIALIZEDATA(s, m_irqClearFlag);
        SERIALIZEDATA(s, m_interruptFlag);
        
        SERIALIZEDATA(s, m_a12LastState);    
        SERIALIZEDATA(s, m_cycleCounter);
    }

};
