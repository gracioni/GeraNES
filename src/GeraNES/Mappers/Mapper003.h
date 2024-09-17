#ifndef MAPPER003_H
#define MAPPER003_H

#include "IMapper.h"

//CNROM
class Mapper003 : public IMapper
{
private:

    uint8_t m_CHRREG = 0;
    uint8_t m_CHRREGMask = 0;

public:

    Mapper003(ICartridgeData& cd) : IMapper(cd)
    {
        m_CHRREGMask = calculateMask(m_cd.numberOfCHRBanks<W8K>());
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_CHRREG = data&m_CHRREGMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return m_cd.readPrg<W16K>(0,addr);
        return m_cd.readPrg<W16K>(m_cd.numberOfPRGBanks<W16K>()==2?1:0,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return IMapper::readChr(addr);

        return m_cd.readChr<W8K>(m_CHRREG,addr);
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

        SERIALIZEDATA(s, m_CHRREG);
        SERIALIZEDATA(s, m_CHRREGMask);
    }

};

#endif // MAPPER003_H
