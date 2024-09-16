#ifndef MAPPER066_H
#define MAPPER066_H

#include "IMapper.h"

class Mapper066 : public IMapper
{
private:

    uint8_t m_PRGREG = 0;
    uint8_t m_PRGREGMask = 0;
    uint8_t m_CHRREG = 0;
    uint8_t m_CHRREGMask = 0;

public:

    Mapper066(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGREGMask = calculateMask(m_cartridgeData.numberOfPRGBanks<W32K>());
        m_CHRREGMask = calculateMask(m_cartridgeData.numberOfCHRBanks<W8K>());
    }

    GERANES_HOT void writePRG32k(int /*addr*/, uint8_t data) override
    {
        m_PRGREG = ((data&0xF0)>>4)&m_PRGREGMask;
        m_CHRREG = (data&0x0F)&m_CHRREGMask;
    }

    GERANES_INLINE_HOT uint8_t readPRG32k(int addr) override
    {
        return m_cartridgeData.readPrg<W32K>(m_PRGREG,addr);
    }

    GERANES_INLINE_HOT uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM()) return IMapper::readCHR8k(addr);

        return m_cartridgeData.readChr<W8K>(m_CHRREG,addr);
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGREG);
        SERIALIZEDATA(s, m_PRGREGMask);
        SERIALIZEDATA(s, m_CHRREG);
        SERIALIZEDATA(s, m_CHRREGMask);
    }

};

#endif // MAPPER066_H
