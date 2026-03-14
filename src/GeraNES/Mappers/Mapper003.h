#pragma once

#include "BaseMapper.h"

//CNROM
class Mapper003 : public BaseMapper
{
private:

    uint8_t m_CHRREG = 0;
    uint8_t m_CHRREGMask = 0;
    bool m_hasBusConflicts = false;

public:

    Mapper003(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_CHRREGMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());

        // NES 2.0 Mapper 3:
        // submapper 1 = no bus conflicts, submapper 2 = AND bus conflicts.
        m_hasBusConflicts = (cd.subMapperId() == 2);
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(m_hasBusConflicts) {
            data &= readPrg(addr);
        }

        m_CHRREG = data&m_CHRREGMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(0,addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>()==2?1:0,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        return cd().readChr<BankSize::B8K>(m_CHRREG,addr);
    }

    void reset() override
    {
        m_CHRREG = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_CHRREG);
        SERIALIZEDATA(s, m_CHRREGMask);
        SERIALIZEDATA(s, m_hasBusConflicts);
    }

};
