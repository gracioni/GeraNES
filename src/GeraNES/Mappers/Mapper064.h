#pragma once

#include "BaseMapper.h"

//Tengen RAMBO-1
//games: Klax, Skull and Crossbones, Shinobi, Hard Drivin, Rolling Thunder

class Mapper064 : public BaseMapper
{
private:

    bool m_CHRMode = false;
    bool m_PRGMode = false;
    bool m_CHR1KMode = false;
    uint8_t m_addrReg = 0;

    uint8_t m_CHRReg0 = 0;
    uint8_t m_CHRReg1 = 0;
    uint8_t m_CHRReg2 = 0;
    uint8_t m_CHRReg3 = 0;
    uint8_t m_CHRReg4 = 0;
    uint8_t m_CHRReg5 = 0;
    uint8_t m_CHRReg6 = 0;
    uint8_t m_CHRReg7 = 0;

    uint8_t m_PRGReg0 = 0;
    uint8_t m_PRGReg1 = 0;
    uint8_t m_PRGReg2 = 0;

    uint8_t m_PRGMask = 0; //8k banks mask
    uint8_t m_CHRMask = 0; //1k banks maks

    bool m_mirroring = false; //0=Vert 1=Horz Ignored when 4-screen

    uint8_t m_reloadValue = 0;
    uint8_t m_irqCounter = 0;
    bool m_enableInterrupt = false;

    bool m_irqMode = false; //false = scanline A12 / true = cycle mode
    bool m_reloadFlag = false;

    bool m_interruptFlag = false;

    int m_cycleDivider = 0;

    int m_delayToInterrupt = 0;

    bool m_a12LastState = false;

    uint8_t m_cycleCounter = 0;

public:

    Mapper064(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_CHRMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(!m_PRGMode)
        {
            if(addr >= 0x0000 && addr < 0x2000)
                return cd().readPrg<BankSize::B8K>(m_PRGReg0,addr);
            else if(addr >= 0x2000 && addr < 0x4000)
                return cd().readPrg<BankSize::B8K>(m_PRGReg1,addr);
            else if(addr >= 0x4000 && addr < 0x6000)
                return cd().readPrg<BankSize::B8K>(m_PRGReg2,addr);
            else if(addr >= 0x6000 && addr < 0x8000)
                return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>()-1,addr);
        }
        else
        {
            if(addr >= 0x0000 && addr < 0x2000)
                return cd().readPrg<BankSize::B8K>(m_PRGReg2,addr);
            else if(addr >= 0x2000 && addr < 0x4000)
                return cd().readPrg<BankSize::B8K>(m_PRGReg0,addr);
            else if(addr >= 0x4000 && addr < 0x6000)
                return cd().readPrg<BankSize::B8K>(m_PRGReg1,addr);
            else if(addr >= 0x6000 && addr < 0x8000)
                return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>()-1,addr);
        }

        return 0;
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        addr &= 0xE001;

        switch(addr)
        {

        case 0x0000:
            m_CHRMode = data & 0x80;
            m_PRGMode = data & 0x40;
            m_CHR1KMode = data & 0x20;
            m_addrReg = data & 0x0F;
            break;

        case 0x0001:

            switch(m_addrReg)
            {
            case 0: m_CHRReg0 = data&m_CHRMask; break;
            case 1: m_CHRReg1 = data&m_CHRMask; break;
            case 2: m_CHRReg2 = data&m_CHRMask; break;
            case 3: m_CHRReg3 = data&m_CHRMask; break;
            case 4: m_CHRReg4 = data&m_CHRMask; break;
            case 5: m_CHRReg5 = data&m_CHRMask; break;
            case 6: m_PRGReg0 = data&m_PRGMask; break;
            case 7: m_PRGReg1 = data&m_PRGMask; break;
            case 8: m_CHRReg6 = data&m_CHRMask; break;
            case 9: m_CHRReg7 = data&m_CHRMask; break;
            case 0xF: m_PRGReg2 = data&m_PRGMask; break;
            }
            break;

        case 0x2000:
            m_mirroring = data & 0x01;
            break;

        case 0x4000:
            m_reloadValue = data;
            break;

        case 0x4001:

            m_reloadFlag = true;
            m_irqMode = data & 0x01;
 
            if(m_irqMode) m_cycleDivider = 0; 

            break;

        case 0x6000:
            m_enableInterrupt = false;
            m_interruptFlag = false;
            m_delayToInterrupt = 0;            
            break;

        case 0x6001:
            m_enableInterrupt = true;
            m_interruptFlag = false;
            m_delayToInterrupt = 0; 
            break;
        }
    }


    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam())
        {
            return BaseMapper::readChr(addr);
        }
        else
        {
            if(!m_CHRMode && !m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0800)
                    return cd().readChr<BankSize::B2K>(m_CHRReg0>>1,addr);
                else if(addr >= 0x0800 && addr < 0x1000)
                    return cd().readChr<BankSize::B2K>(m_CHRReg1>>1,addr);
                else if(addr >= 0x1000 && addr < 0x1400)
                    return cd().readChr<BankSize::B1K>(m_CHRReg2,addr);
                else if(addr >= 0x1400 && addr < 0x1800)
                    return cd().readChr<BankSize::B1K>(m_CHRReg3,addr);
                else if(addr >= 0x1800 && addr < 0x1C00)
                    return cd().readChr<BankSize::B1K>(m_CHRReg4,addr);
                else if(addr >= 0x1C00 && addr < 0x2000)
                    return cd().readChr<BankSize::B1K>(m_CHRReg5,addr);
            }
            else if(!m_CHRMode && m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0400)
                    return cd().readChr<BankSize::B1K>(m_CHRReg0,addr);
                if(addr >= 0x0400 && addr < 0x0800)
                    return cd().readChr<BankSize::B1K>(m_CHRReg6,addr);
                else if(addr >= 0x0800 && addr < 0x0C00)
                    return cd().readChr<BankSize::B1K>(m_CHRReg1,addr);
                else if(addr >= 0x0C00 && addr < 0x1000)
                    return cd().readChr<BankSize::B1K>(m_CHRReg7,addr);
                else if(addr >= 0x1000 && addr < 0x1400)
                    return cd().readChr<BankSize::B1K>(m_CHRReg2,addr);
                else if(addr >= 0x1400 && addr < 0x1800)
                    return cd().readChr<BankSize::B1K>(m_CHRReg3,addr);
                else if(addr >= 0x1800 && addr < 0x1C00)
                    return cd().readChr<BankSize::B1K>(m_CHRReg4,addr);
                else if(addr >= 0x1C00 && addr < 0x2000)
                    return cd().readChr<BankSize::B1K>(m_CHRReg5,addr);
            }
            else if(m_CHRMode && !m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0400)
                    return cd().readChr<BankSize::B1K>(m_CHRReg2,addr);
                if(addr >= 0x0400 && addr < 0x0800)
                    return cd().readChr<BankSize::B1K>(m_CHRReg3,addr);
                if(addr >= 0x0800 && addr < 0x0C00)
                    return cd().readChr<BankSize::B1K>(m_CHRReg4,addr);
                if(addr >= 0x0C00 && addr < 0x1000)
                    return cd().readChr<BankSize::B1K>(m_CHRReg5,addr);
                if(addr >= 0x1000 && addr < 0x1800)
                    return cd().readChr<BankSize::B2K>(m_CHRReg0>>1,addr);
                if(addr >= 0x1800 && addr < 0x2000)
                    return cd().readChr<BankSize::B2K>(m_CHRReg1>>1,addr);
            }
            else if(m_CHRMode && m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0400)
                    return cd().readChr<BankSize::B1K>(m_CHRReg2,addr);
                if(addr >= 0x0400 && addr < 0x0800)
                    return cd().readChr<BankSize::B1K>(m_CHRReg3,addr);
                else if(addr >= 0x0800 && addr < 0x0C00)
                    return cd().readChr<BankSize::B1K>(m_CHRReg4,addr);
                else if(addr >= 0x0C00 && addr < 0x1000)
                    return cd().readChr<BankSize::B1K>(m_CHRReg5,addr);
                else if(addr >= 0x1000 && addr < 0x1400)
                    return cd().readChr<BankSize::B1K>(m_CHRReg0,addr);
                else if(addr >= 0x1400 && addr < 0x1800)
                    return cd().readChr<BankSize::B1K>(m_CHRReg6,addr);
                else if(addr >= 0x1800 && addr < 0x1C00)
                    return cd().readChr<BankSize::B1K>(m_CHRReg1,addr);
                else if(addr >= 0x1C00 && addr < 0x2000)
                    return cd().readChr<BankSize::B1K>(m_CHRReg7,addr);
            }
        }

        return 0;
    } 

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring() ) return MirroringType::FOUR_SCREEN;
        if(m_mirroring) return MirroringType::HORIZONTAL;
        return MirroringType::VERTICAL;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag && m_delayToInterrupt == 0;
    }

    void setA12State(bool state) override
    {
        if(!m_a12LastState && state) {    

            if(m_cycleCounter > 3) {
                if(!m_irqMode) count(2);                
            }
        }
        else if(m_a12LastState && !state) {
            m_cycleCounter = 0;
        }   

        m_a12LastState = state;      
    } 

    GERANES_HOT void cycle() override
    {
        if((uint8_t)(m_cycleCounter+1) != 0)
            m_cycleCounter++;

        if(m_delayToInterrupt > 0) {
            --m_delayToInterrupt;
        }        

        if(m_irqMode) {

            m_cycleDivider = (m_cycleDivider + 1) & 0x03;

            if(m_cycleDivider == 0) {
                count(2);
            }
        }
    }

    void count(int delayToInterrupt)
    {
        if (m_reloadFlag)
        {
            m_reloadFlag = false;

            m_irqCounter = m_reloadValue;

            if(m_reloadValue != 0) m_irqCounter |= 1;

            if (m_irqMode) m_irqCounter |= 2;
        }
        else if (m_irqCounter == 0)
        {
            m_irqCounter = m_reloadValue;
        }
        else
        {
            m_irqCounter--; 
        }

        if(m_irqCounter == 0 && m_enableInterrupt) {
            m_delayToInterrupt = delayToInterrupt;
            m_interruptFlag = true;
        }

    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_CHRMode);
        SERIALIZEDATA(s, m_PRGMode);
        SERIALIZEDATA(s, m_addrReg);
        SERIALIZEDATA(s, m_CHRReg0);
        SERIALIZEDATA(s, m_CHRReg1);
        SERIALIZEDATA(s, m_CHRReg2);
        SERIALIZEDATA(s, m_CHRReg3);
        SERIALIZEDATA(s, m_CHRReg4);
        SERIALIZEDATA(s, m_CHRReg5);
        SERIALIZEDATA(s, m_CHRReg6);
        SERIALIZEDATA(s, m_CHRReg7);

        SERIALIZEDATA(s, m_PRGReg0);
        SERIALIZEDATA(s, m_PRGReg1);
        SERIALIZEDATA(s, m_PRGReg2);

        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_CHRMask);

        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_reloadValue);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_enableInterrupt);

        SERIALIZEDATA(s, m_irqMode);
        SERIALIZEDATA(s, m_reloadFlag);
        SERIALIZEDATA(s, m_interruptFlag);

        SERIALIZEDATA(s, m_a12LastState);
        SERIALIZEDATA(s, m_cycleCounter);
    }

};
