#pragma once

#include "BaseMapper.h"

//MMC1
//SxROM
class Mapper001 : public BaseMapper
{
private:

    int m_shiftCounter = 0;
    uint8_t m_shiftRegister = 0;
    uint8_t m_control = 0x0C;
    uint8_t m_chrBank0 = 0;
    uint8_t m_chrBank1 = 0;
    uint8_t m_prgBank = 0;

    uint8_t m_PRGMask = 0; //16k banks mask
    uint8_t m_CHRMask = 0; //4k banks maks

public:

    Mapper001(ICartridgeData& cd) : BaseMapper(cd)
    { 
        m_PRGMask = calculateMask(m_cd.numberOfPRGBanks<W16K>());
        m_CHRMask = calculateMask(m_cd.numberOfCHRBanks<W4K>());

    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(data&0x80)
        {
            m_shiftCounter = 0;
            m_control |= 0x0C;
        }
        else
        {
            m_shiftRegister |= (data&0x01) << m_shiftCounter ;
            m_shiftCounter++;

            if(m_shiftCounter == 5)
            {
                if(addr >= 0x0000 && addr < 0x2000)
                {
                    //control
                    m_control = m_shiftRegister&0x1F;
                }
                else if(addr >= 0x2000 && addr < 0x4000)
                {
                    //chrbank0
                    m_chrBank0 = m_shiftRegister&m_CHRMask;
                }
                else if(addr >= 0x4000 && addr < 0x6000)
                {
                    //chrbak1
                    m_chrBank1 = m_shiftRegister&m_CHRMask;
                }
                else if(addr >= 0x6000 && addr < 0x8000)
                {
                    //prgbank
                    m_prgBank = m_shiftRegister&m_PRGMask;
                }

                m_shiftCounter = 0;
                m_shiftRegister = 0;
            }
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch( (m_control&0x0C)>>2 )
        {
        case 0: //switch 32 KB at $8000, ignoring low bit of bank number
        case 1:
            if(addr < 0x4000) return m_cd.readPrg<W16K>(m_prgBank>>1,addr);
            return m_cd.readPrg<W16K>((m_prgBank>>1)+1,addr);
            break;

        case 2:  //fix first bank at $8000 and switch 16 KB bank at $C000
            if(addr < 0x4000) return m_cd.readPrg<W16K>(0,addr);
            return m_cd.readPrg<W16K>(m_prgBank&0x0F,addr);
            break;

        case 3: //fix last bank at $C000 and switch 16 KB bank at $8000
            if(addr < 0x4000) return m_cd.readPrg<W16K>(m_prgBank&0x0F,addr);
            return m_cd.readPrg<W16K>(m_cd.numberOfPRGBanks<W16K>()-1,addr);
            break;
        }

        return 0;
    }


    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        else
        {
            if( !(m_control&0x10) ) //switch 8 KB at a time - low bit ignored in 8 KB mode
            {
                return m_cd.readChr<W8K>(m_chrBank0>>1,addr);
            }
            else //switch two separate 4 KB banks
            {
                if(addr < 0x1000)
                {
                    return m_cd.readChr<W4K>(m_chrBank0,addr);
                }
                else
                {
                    return m_cd.readChr<W4K>(m_chrBank1,addr);
                }
            }
        }

        return 0;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_control&0x03)
        {
        case 0: return MirroringType::SINGLE_SCREEN_A; break;
        case 1: return MirroringType::SINGLE_SCREEN_B; break;
        case 2: return MirroringType::VERTICAL; break;
        case 3: return MirroringType::HORIZONTAL; break;
        }

        return MirroringType::FOUR_SCREEN;
    }

    void serialization(SerializationBase &s) override
    {
        BaseMapper::serialization(s); 

        SERIALIZEDATA(s, m_shiftCounter);
        SERIALIZEDATA(s, m_shiftRegister);
        SERIALIZEDATA(s, m_control);
        SERIALIZEDATA(s, m_chrBank0);
        SERIALIZEDATA(s, m_chrBank1);
        SERIALIZEDATA(s, m_prgBank);

        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_CHRMask);
    }

};
