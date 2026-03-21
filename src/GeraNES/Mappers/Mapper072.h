#pragma once

#include "BaseMapper.h"

// Jaleco JF-17 / JF-19 style latch-on-rising-edge mapper.
class Mapper072 : public BaseMapper
{
protected:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0;
    bool m_prevPrgLatch = false;
    bool m_prevChrLatch = false;

    virtual uint8_t prgBits(uint8_t data) const
    {
        return static_cast<uint8_t>(data & 0x0F);
    }

    virtual uint8_t chrBits(uint8_t data) const
    {
        return static_cast<uint8_t>(data & 0x0F);
    }

    GERANES_INLINE uint8_t applyBusConflicts(int addr, uint8_t data)
    {
        return static_cast<uint8_t>(data & readPrg(addr));
    }

public:
    Mapper072(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B8K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        data = applyBusConflicts(addr, data);

        const bool prgLatch = (data & 0x80) != 0;
        const bool chrLatch = (data & 0x40) != 0;

        if(prgLatch && !m_prevPrgLatch) {
            m_prgBank = prgBits(data) & m_prgMask;
        }
        if(chrLatch && !m_prevChrLatch) {
            m_chrBank = chrBits(data) & m_chrMask;
        }

        m_prevPrgLatch = prgLatch;
        m_prevChrLatch = chrLatch;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 14) & 0x01) {
        case 0: return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        default: return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(m_chrBank, addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(m_chrBank, addr, data);
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
        m_prevPrgLatch = false;
        m_prevChrLatch = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_prevPrgLatch);
        SERIALIZEDATA(s, m_prevChrLatch);
    }
};
