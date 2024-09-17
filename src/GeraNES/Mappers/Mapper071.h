#ifndef MAPPER071_H
#define MAPPER071_H

#include "IMapper.h"

//alguns jogos nao funcionam com o overclock ativado

class Mapper071 : public IMapper
{
private:

    uint8_t m_PRGRegMask = 0;
    uint8_t m_PRGReg = 0;

    uint8_t m_mirroring = 0xFF;

public:

    Mapper071(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGRegMask = calculateMask(m_cartridgeData.numberOfPRGBanks<W16K>());
    }

    virtual bool VRAMRequired() override {
        return true;
    }

    GERANES_HOT void writePRG32k(int addr, uint8_t data) override
    {
        //Fire Hawk only writes this register at the address $9000, and other games like Micro Machines and
        //Ultimate Stuntman write $00 to $8000 on startup.
        //For compatibility without using a submapper, FCEUX begins all games with fixed mirroring,
        //and applies single screen mirroring only once $9000-9FFF is written, ignoring writes to $8000-8FFF.

        if(addr < 0x1000) {}
        else if(addr < 0x4000) m_mirroring = (data >> 4) & 0x01;
        else m_PRGReg = data & m_PRGRegMask;
    }

    GERANES_HOT uint8_t readPRG32k(int addr) override
    {
        switch(addr>>14) { //addr/16K
            case 0: return m_cartridgeData.readPrg<W16K>(m_PRGReg,addr);
            case 1: return m_cartridgeData.readPrg<W16K>(m_cartridgeData.numberOfPRGBanks<W16K>()-1,addr);
        }

        return 0;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_mirroring == 0xFF) return IMapper::mirroringType();

        switch(m_mirroring) {
        case 0: return MirroringType::SINGLE_SCREEN_A;
        case 1: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::FOUR_SCREEN;
    }

    /*
    void writeSRAM8k(int addr, uint8_t data) override
    {
    }

    virtual uint8_t readSRAM8k(int addr) override
    {
        return 0;
    }
    */

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGRegMask);
        SERIALIZEDATA(s, m_PRGReg);
        SERIALIZEDATA(s, m_mirroring);

    }
};

#endif
