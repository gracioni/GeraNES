#ifndef MAPPER118_H
#define MAPPER118_H

#include "IMapper.h"
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
        if(m_CHRMode) {

            switch(index) {
            case 0: return m_CHRReg[2] >> 7;
            case 1: return m_CHRReg[3] >> 7;
            case 2: return m_CHRReg[4] >> 7;
            case 3: return m_CHRReg[5] >> 7;
            }

        }
        else {

            switch(index) {
            case 0: return m_CHRReg[0] >> 7;
            case 1: return m_CHRReg[0] >> 7;
            case 2: return m_CHRReg[1] >> 7;
            case 3: return m_CHRReg[1] >> 7;
            }

        }

        return 0;
    }

};

#endif
