#ifndef MAPPER015_H
#define MAPPER015_H

#include "IMapper.h"

//game: 100-in-1 Contra Function 16

class Mapper015 : public IMapper
{

private:

    uint8_t m_mode = 0;
    bool m_mirroring = false;
    uint8_t m_PRGBank = 0;
    uint8_t m_b = 0;

public:

    Mapper015(ICartridgeData& cd) : IMapper(cd)
    {
    } 

    GERANES_HOT void writePRG32k(int addr, uint8_t data) override
    {
        m_mode = addr & 0x03;
        m_mirroring = data & 0x40;
        m_PRGBank = data & 0x3F;
        m_b = (data & 0x80) ? 1 : 0;
    };

    GERANES_HOT uint8_t readPRG32k(int addr) override
    {
        switch(m_mode) {

        case 0:
            if(addr < 0x4000) return m_cd.readPrg<W16K>(m_PRGBank,addr);
            return m_cd.readPrg<W16K>(m_PRGBank | 1,addr);
            break;

        case 1:
            if(addr < 0x4000) return m_cd.readPrg<W16K>(m_PRGBank,addr);
            return m_cd.readPrg<W16K>(m_PRGBank | 7,addr);
            break;

        case 2:
            return m_cd.readPrg<W8K>((m_PRGBank << 1) | m_b,addr);
            break;

        case 3:
            return m_cd.readPrg<W16K>(m_PRGBank,addr);
            break;
        }

        return 0;
    }

    GERANES_HOT void writeCHR8k(int addr, uint8_t data) override
    {
        if(m_mode == 0 || m_mode == 3) return; //write protected
        IMapper::writeCHR8k(addr,data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_mirroring) return MirroringType::HORIZONTAL;
        return MirroringType::VERTICAL;
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_PRGBank);
        SERIALIZEDATA(s, m_b);
    }

};

#endif // MAPPER015_H
