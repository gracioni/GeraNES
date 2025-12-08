#pragma once

#include "BaseMapper.h"

//NROM
class Mapper007 : public BaseMapper
{
private:

    uint8_t m_PRGReg = 0;
    uint8_t m_PRGRegMask = 0;

    bool m_mirroring = false;

public:

    Mapper007(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGRegMask = calculateMask(m_cd.numberOfPRGBanks<W32K>());    
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_mirroring = data&0x10;
        m_PRGReg = data&m_PRGRegMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return m_cd.readPrg<W32K>(m_PRGReg,addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(!m_mirroring) return MirroringType::SINGLE_SCREEN_A;
        return MirroringType::SINGLE_SCREEN_B;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGReg);
        SERIALIZEDATA(s, m_PRGRegMask);
        SERIALIZEDATA(s, m_mirroring);
    }

};
