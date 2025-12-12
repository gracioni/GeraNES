#pragma once

#include "BaseMapper.h"

//alguns jogos nao funcionam com o overclock ativado

class Mapper071 : public BaseMapper
{
private:

    uint8_t m_PRGRegMask = 0;
    uint8_t m_PRGReg = 0;

    uint8_t m_mirroring = 0xFF;

public:

    Mapper071(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGRegMask = calculateMask(m_cd.numberOfPRGBanks<WindowSize::W16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        //Fire Hawk only writes this register at the address $9000, and other games like Micro Machines and
        //Ultimate Stuntman write $00 to $8000 on startup.
        //For compatibility without using a submapper, FCEUX begins all games with fixed mirroring,
        //and applies single screen mirroring only once $9000-9FFF is written, ignoring writes to $8000-8FFF.

        if(addr < 0x1000) {}
        else if(addr < 0x4000) m_mirroring = (data >> 4) & 0x01;
        else m_PRGReg = data & m_PRGRegMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch(addr>>14) { //addr/16K
            case 0: return m_cd.readPrg<WindowSize::W16K>(m_PRGReg,addr);
            case 1: return m_cd.readPrg<WindowSize::W16K>(m_cd.numberOfPRGBanks<WindowSize::W16K>()-1,addr);
        }

        return 0;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_mirroring == 0xFF) return BaseMapper::mirroringType();

        switch(m_mirroring) {
        case 0: return MirroringType::SINGLE_SCREEN_A;
        case 1: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::FOUR_SCREEN;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGRegMask);
        SERIALIZEDATA(s, m_PRGReg);
        SERIALIZEDATA(s, m_mirroring);

    }
};
