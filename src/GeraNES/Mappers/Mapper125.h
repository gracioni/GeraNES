#pragma once

#include "BaseMapper.h"

class Mapper125 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_workRam[0x2000] = {0};

    GERANES_INLINE uint8_t fixedBankFromBack(uint8_t back) const
    {
        const int totalBanks = cd().numberOfPRGBanks<BankSize::B8K>();
        return totalBanks > back ? static_cast<uint8_t>(totalBanks - back) : 0;
    }

public:
    Mapper125(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        if(addr != 0) return;
        m_prgBank = static_cast<uint8_t>(value & 0x0F & m_prgMask);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgBank, addr);
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        if(addr >= 0x4000 && addr < 0x6000) {
            m_workRam[addr & 0x1FFF] = value;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x2000) return cd().readPrg<BankSize::B8K>(fixedBankFromBack(4), addr);
        if(addr < 0x4000) return cd().readPrg<BankSize::B8K>(fixedBankFromBack(3), addr);
        if(addr < 0x6000) return m_workRam[addr & 0x1FFF];
        return cd().readPrg<BankSize::B8K>(fixedBankFromBack(1), addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgBank = 0;
        memset(m_workRam, 0, sizeof(m_workRam));
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        s.array(m_workRam, 1, sizeof(m_workRam));
    }
};
