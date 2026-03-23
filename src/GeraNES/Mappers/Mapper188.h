#pragma once

#include "BaseMapper.h"

class Mapper188 : public BaseMapper
{
private:
    uint8_t m_bank = 0;
    bool m_useInternalRom = false;
    bool m_horizontalMirroring = false;
    uint8_t m_internalBankMask = 0;
    uint8_t m_externalBankMask = 0;

    GERANES_INLINE uint8_t mappedBank() const
    {
        const uint8_t internalBanks = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B16K>() >= 8 ? 8 : cd().numberOfPRGBanks<BankSize::B16K>());
        if(m_useInternalRom || cd().numberOfPRGBanks<BankSize::B16K>() <= internalBanks) {
            return m_bank & m_internalBankMask;
        }

        const uint8_t externalBase = internalBanks;
        return static_cast<uint8_t>(externalBase + (m_bank & m_externalBankMask));
    }

public:
    Mapper188(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) {
            allocateChrRam(static_cast<int>(BankSize::B8K));
        }

        const int totalBanks = cd.numberOfPRGBanks<BankSize::B16K>();
        const int internalBanks = totalBanks >= 8 ? 8 : totalBanks;
        const int externalBanks = totalBanks > internalBanks ? (totalBanks - internalBanks) : 0;

        m_internalBankMask = calculateMask(internalBanks);
        m_externalBankMask = externalBanks > 0 ? calculateMask(externalBanks) : 0;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        value &= readPrg(addr);
        m_bank = static_cast<uint8_t>(value & 0x0F);
        m_useInternalRom = (value & 0x20) != 0;
        m_horizontalMirroring = (value & 0x40) != 0;
    }

    GERANES_HOT uint8_t readSaveRam(int /*addr*/) override
    {
        return 0x07;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(mappedBank(), addr);
        return cd().readPrg<BankSize::B16K>(7, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return BaseMapper::readChr(addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        BaseMapper::writeChr(addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_bank = 0;
        m_useInternalRom = false;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bank);
        SERIALIZEDATA(s, m_useInternalRom);
        SERIALIZEDATA(s, m_horizontalMirroring);
        SERIALIZEDATA(s, m_internalBankMask);
        SERIALIZEDATA(s, m_externalBankMask);
    }
};
