#ifndef MAPPER064_H
#define MAPPER064_H

#include "IMapper.h"

//Tengen RAMBO-1
//games: Klax, Skull and Crossbones, Shinobi

class Mapper064 : public IMapper
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
    uint8_t m_scanlinesCounter = 0;
    bool m_enableInterrupt = false;

    bool m_irqMode = false; //false = scanline A12 / true = cycle mode
    bool m_reloadScanlineCounterFlag = false;

    bool m_interruptFlag = false;

    int m_cycleDivider = 0;

    bool m_coolFlag = false; //hack flag to enable the interrupt only when a new reload value is set
                           //without this, the menu in the megaman3 last boss is glitched
                           //because improper interrupts happen
                           //however, some mmc3 tests fail :(

    int m_delayToInterrupt = -1; //small delay to activate the interrupt flag

public:

    Mapper064(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGMask = calculateMask(m_cartridgeData.numberOfPRGBanks<W8K>());
        m_CHRMask = calculateMask(m_cartridgeData.numberOfCHRBanks<W1K>());
    }

    GERANES_INLINE_HOT uint8_t readPRG32k(int addr) override
    {
        if(!m_PRGMode)
        {
            if(addr >= 0x0000 && addr < 0x2000)
                return m_cartridgeData.readPrg<W8K>(m_PRGReg0,addr);
            else if(addr >= 0x2000 && addr < 0x4000)
                return m_cartridgeData.readPrg<W8K>(m_PRGReg1,addr);
            else if(addr >= 0x4000 && addr < 0x6000)
                return m_cartridgeData.readPrg<W8K>(m_PRGReg2,addr);
            else if(addr >= 0x6000 && addr < 0x8000)
                return m_cartridgeData.readPrg<W8K>(m_cartridgeData.numberOfPRGBanks<W8K>()-1,addr);
        }
        else
        {
            if(addr >= 0x0000 && addr < 0x2000)
                return m_cartridgeData.readPrg<W8K>(m_PRGReg2,addr);
            else if(addr >= 0x2000 && addr < 0x4000)
                return m_cartridgeData.readPrg<W8K>(m_PRGReg0,addr);
            else if(addr >= 0x4000 && addr < 0x6000)
                return m_cartridgeData.readPrg<W8K>(m_PRGReg1,addr);
            else if(addr >= 0x6000 && addr < 0x8000)
                return m_cartridgeData.readPrg<W8K>(m_cartridgeData.numberOfPRGBanks<W8K>()-1,addr);
        }

        return 0;
    }

    GERANES_INLINE_HOT void writePRG32k(int addr, uint8_t data) override
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
            m_coolFlag = true;
            break;

        case 0x4001:
            m_reloadScanlineCounterFlag = true;
            m_irqMode = data & 0x01;
            break;

        case 0x6000:
            m_interruptFlag = false; m_delayToInterrupt = -1;
            m_enableInterrupt = false;
            break;

        case 0x6001:
            m_enableInterrupt = true;
            break;
        }
    }


    GERANES_INLINE_HOT uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM())
        {
            return IMapper::readCHR8k(addr);
        }
        else
        {
            if(!m_CHRMode && !m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0800)
                    return m_cartridgeData.readChr<W2K>(m_CHRReg0>>1,addr);
                else if(addr >= 0x0800 && addr < 0x1000)
                    return m_cartridgeData.readChr<W2K>(m_CHRReg1>>1,addr);
                else if(addr >= 0x1000 && addr < 0x1400)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg2,addr);
                else if(addr >= 0x1400 && addr < 0x1800)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg3,addr);
                else if(addr >= 0x1800 && addr < 0x1C00)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg4,addr);
                else if(addr >= 0x1C00 && addr < 0x2000)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg5,addr);
            }
            else if(!m_CHRMode && m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0400)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg0,addr);
                if(addr >= 0x0400 && addr < 0x0800)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg6,addr);
                else if(addr >= 0x0800 && addr < 0x0C00)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg1,addr);
                else if(addr >= 0x0C00 && addr < 0x1000)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg7,addr);
                else if(addr >= 0x1000 && addr < 0x1400)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg2,addr);
                else if(addr >= 0x1400 && addr < 0x1800)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg3,addr);
                else if(addr >= 0x1800 && addr < 0x1C00)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg4,addr);
                else if(addr >= 0x1C00 && addr < 0x2000)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg5,addr);
            }
            else if(m_CHRMode && !m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0400)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg2,addr);
                if(addr >= 0x0400 && addr < 0x0800)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg3,addr);
                if(addr >= 0x0800 && addr < 0x0C00)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg4,addr);
                if(addr >= 0x0C00 && addr < 0x1000)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg5,addr);
                if(addr >= 0x1000 && addr < 0x1800)
                    return m_cartridgeData.readChr<W2K>(m_CHRReg0>>1,addr);
                if(addr >= 0x1800 && addr < 0x2000)
                    return m_cartridgeData.readChr<W2K>(m_CHRReg1>>1,addr);
            }
            else if(m_CHRMode && m_CHR1KMode)
            {
                if(addr >= 0x0000 && addr < 0x0400)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg2,addr);
                if(addr >= 0x0400 && addr < 0x0800)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg3,addr);
                else if(addr >= 0x0800 && addr < 0x0C00)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg4,addr);
                else if(addr >= 0x0C00 && addr < 0x1000)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg5,addr);
                else if(addr >= 0x1000 && addr < 0x1400)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg0,addr);
                else if(addr >= 0x1400 && addr < 0x1800)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg6,addr);
                else if(addr >= 0x1800 && addr < 0x1C00)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg1,addr);
                else if(addr >= 0x1C00 && addr < 0x2000)
                    return m_cartridgeData.readChr<W1K>(m_CHRReg7,addr);
            }
        }

        return 0;
    } 

    GERANES_INLINE_HOT IMapper::MirroringType mirroringType() override
    {
        if(m_cartridgeData.useFourScreenMirroring() ) return IMapper::FOUR_SCREEN;
        if(m_mirroring) return IMapper::HORIZONTAL;
        return IMapper::VERTICAL;
    }

    bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    void tick() override
    {
        if(m_irqMode == true) return;
        haha();
    }

    void cycle() override
    {
        if(m_delayToInterrupt > -1) {
            --m_delayToInterrupt;

            if(m_delayToInterrupt == 0)
                m_interruptFlag = true;
        }


        if(m_irqMode == false) return;

        ++m_cycleDivider;

        if(m_cycleDivider == 3) {
            m_cycleDivider = 0;
            haha();
        }
    }

    void haha()
    {
        //should reload and set irq every clock when reloadValue == 0 (like MMC3-C)
        if(m_reloadValue == 0 && m_scanlinesCounter == 0){
            m_scanlinesCounter = m_reloadValue;
            if(m_enableInterrupt) m_delayToInterrupt = 2;
        }


        if(m_reloadScanlineCounterFlag || m_scanlinesCounter == 0) {
            m_scanlinesCounter = m_reloadValue;
            m_reloadScanlineCounterFlag = false;
            m_cycleDivider = 0;
            //coolFlag = true;
        }
        else if(m_scanlinesCounter > 0) m_scanlinesCounter--;

        if(m_scanlinesCounter == 0 && m_enableInterrupt) {

            if(m_coolFlag){
                m_delayToInterrupt = 2;
                m_coolFlag = false;
            }

        }
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

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
        SERIALIZEDATA(s, m_scanlinesCounter);
        SERIALIZEDATA(s, m_enableInterrupt);

        SERIALIZEDATA(s, m_irqMode);
        SERIALIZEDATA(s, m_reloadScanlineCounterFlag);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_coolFlag);
    }

};

#endif // MAPPER064_H
