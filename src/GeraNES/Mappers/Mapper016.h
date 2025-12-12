#pragma once

#include "BaseMapper.h"

//bandai

//TODO: need work

class Mapper016 : public BaseMapper
{
private:

    uint8_t m_PRGMask = 0;
    uint8_t m_CHRMask = 0;

    uint8_t m_CHRBank[8];
    uint8_t m_PRGBank = 0;
    uint8_t m_mirroring = 0;
    bool m_enableIRQ = false;
    uint16_t m_IRQCounter = 0;
    bool m_IRQFlag = false;

public:

    Mapper016(ICartridgeData& cd) : BaseMapper(cd)
    {
        memset(m_CHRBank, 0x00, 8);

        m_PRGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_CHRMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        //qDebug() << "PRG write add: " << QString::number(addr,16) << " data: " << QString::number(data,16);

        switch(addr&0x0F)
        {

        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            m_CHRBank[addr&0x07] = data&m_CHRMask;
            break;

        case 8:
            m_PRGBank = data&m_PRGMask;
            break;

        case 9:
            m_mirroring = data&0x03;
            break;

        case 0xA:
            m_enableIRQ = data&1;
            m_IRQFlag = false;
            break;

        case 0xB:
            m_IRQCounter = (m_IRQCounter&0xFF00) | data;
            break;

        case 0xC:
            m_IRQCounter = (m_IRQCounter&0x00FF) | (((uint16_t)data)<<8);
            break;

        case 0xD:
            //eeprom(data);
            break;

        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_PRGBank,addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>()-1,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        addr &= 0x1FFF;
        return cd().readChr<BankSize::B1K>(m_CHRBank[(addr/0x0400)&0x07], addr);
        return 0;
    }

    GERANES_HOT void cycle() override
    {
        if(m_enableIRQ) {
            --m_IRQCounter;
            if(m_IRQCounter == 0) m_IRQFlag = true;
        }
    };

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_IRQFlag;
    };

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroring)
        {
        case 0: return MirroringType::VERTICAL; break;
        case 1: return MirroringType::HORIZONTAL; break;
        case 2: return MirroringType::SINGLE_SCREEN_A; break;
        case 3: return MirroringType::SINGLE_SCREEN_B; break;
        }

        return MirroringType::FOUR_SCREEN;
    }


};
