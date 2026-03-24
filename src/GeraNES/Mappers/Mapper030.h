#pragma once

#include "BaseMapper.h"

class Mapper030 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    bool m_hasBusConflicts = false;
    bool m_enableMirroringBit = false;
    bool m_verticalMirroringBit = false;

public:
    Mapper030(ICartridgeData& cd) : BaseMapper(cd)
    {
        // UNROM 512 uses 32 KB of CHR RAM regardless of what the header reports.
        // Some games only switch to non-zero CHR banks after gameplay starts.
        allocateChrRam(static_cast<int>(BankSize::B32K));
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(static_cast<int>(BankSize::B32K) / static_cast<int>(BankSize::B8K));
        m_hasBusConflicts = (cd.subMapperId() == 2) || (cd.subMapperId() == 0 && !cd.hasBattery());

        if(cd.subMapperId() == 3) {
            m_enableMirroringBit = true;
            m_verticalMirroringBit = true;
        }
        else if(cd.mirroringType() == MirroringType::SINGLE_SCREEN_A || cd.mirroringType() == MirroringType::SINGLE_SCREEN_B) {
            m_enableMirroringBit = true;
            m_verticalMirroringBit = false;
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(m_hasBusConflicts) {
            data &= readPrg(addr);
        }

        if(cd().hasBattery() && addr < 0x4000) {
            return;
        }

        m_prgBank = static_cast<uint8_t>(data & 0x1F) & m_prgMask;
        m_chrBank = static_cast<uint8_t>((data >> 5) & 0x03) & m_chrMask;
        if(m_enableMirroringBit) {
            m_verticalMirroringBit = (data & 0x80) != 0;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
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

    GERANES_HOT MirroringType mirroringType() override
    {
        if(!m_enableMirroringBit) return BaseMapper::mirroringType();
        if(cd().subMapperId() == 3) {
            return m_verticalMirroringBit ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
        }
        return m_verticalMirroringBit ? MirroringType::SINGLE_SCREEN_B : MirroringType::SINGLE_SCREEN_A;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
        if(cd().subMapperId() == 3) {
            m_verticalMirroringBit = true;
        }
        else {
            m_verticalMirroringBit = false;
        }
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_hasBusConflicts);
        SERIALIZEDATA(s, m_enableMirroringBit);
        SERIALIZEDATA(s, m_verticalMirroringBit);
    }
};
