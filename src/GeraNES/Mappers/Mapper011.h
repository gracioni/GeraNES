#ifndef MAPPER011_H
#define MAPPER011_H

#include "IMapper.h"

class Mapper011 : public IMapper
{
private:

    uint8_t m_PRGMask = 0;
    uint8_t m_CHRMask = 0;

    uint8_t m_PRGBank = 0;
    uint8_t m_CHRBank = 0;

public:

    Mapper011(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGMask = calculateMask(m_cd.numberOfPRGBanks<W32K>());
        m_CHRMask = calculateMask(m_cd.numberOfCHRBanks<W8K>());
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_PRGBank = (data&0x03)&m_PRGMask;
        m_CHRBank = ((data&0xF0)>>4)&m_CHRMask;
    };

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        addr &= 0x7FFF;
        return m_cd.readPrg<W32K>(m_PRGBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return IMapper::readChr(addr);

        addr &= 0x1FFF;
        return m_cd.readChr<W8K>(m_CHRBank,addr);
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_CHRMask);
        SERIALIZEDATA(s, m_PRGBank);
        SERIALIZEDATA(s, m_CHRBank);
    }
};

#endif // MAPPER011_H
