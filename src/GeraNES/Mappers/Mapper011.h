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
        m_PRGMask = calculateMask(m_cartridgeData.numberOfPRGBanks<W32K>());
        m_CHRMask = calculateMask(m_cartridgeData.numberOfCHRBanks<W8K>());
    }

    GERANES_INLINE_HOT void writePRG32k(int /*addr*/, uint8_t data) override
    {
        m_PRGBank = (data&0x03)&m_PRGMask;
        m_CHRBank = ((data&0xF0)>>4)&m_CHRMask;
    };

    GERANES_INLINE_HOT uint8_t readPRG32k(int addr) override
    {
        addr &= 0x7FFF;
        return m_cartridgeData.readPrg<W32K>(m_PRGBank, addr);
    }

    GERANES_INLINE_HOT uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM()) return IMapper::readCHR8k(addr);

        addr &= 0x1FFF;
        return m_cartridgeData.readChr<W8K>(m_CHRBank,addr);
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
