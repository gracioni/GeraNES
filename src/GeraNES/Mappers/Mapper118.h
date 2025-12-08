#pragma once

#include "BaseMapper.h"
#include "Mapper004.h"

//modified mmc3
class Mapper118 : public Mapper004
{

public:

    Mapper118(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT MirroringType mirroringType() override
    {
         return MirroringType::CUSTOM;
    }

    GERANES_HOT uint8_t customMirroring(uint8_t index) override
    {
        if(m_chrMode) {

            switch(index) {
            case 0: return m_chrReg[2] >> 7;
            case 1: return m_chrReg[3] >> 7;
            case 2: return m_chrReg[4] >> 7;
            case 3: return m_chrReg[5] >> 7;
            }

        }
        else {

            switch(index) {
            case 0: return m_chrReg[0] >> 7;
            case 1: return m_chrReg[0] >> 7;
            case 2: return m_chrReg[1] >> 7;
            case 3: return m_chrReg[1] >> 7;
            }

        }

        return 0;
    }

};
