#pragma once

#include "BaseMapper.h"

// iNES Mapper 227
// Address-latch multicart with 8KB CHR-RAM and several PRG layout modes.
class Mapper227 : public BaseMapper
{
private:
    uint8_t m_prgBankLo = 0;
    uint8_t m_prgBankHi = 0;
    uint8_t m_prgMask = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper227(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B8K));
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        const uint8_t prgBank = static_cast<uint8_t>((((absolute >> 2) & 0x1F) | ((absolute & 0x100) >> 3)) & m_prgMask);
        const bool sFlag = (absolute & 0x01) != 0;
        const bool lFlag = ((absolute >> 9) & 0x01) != 0;
        const bool prgMode = ((absolute >> 7) & 0x01) != 0;

        if(prgMode) {
            if(sFlag) {
                m_prgBankLo = static_cast<uint8_t>(prgBank & 0xFE) & m_prgMask;
                m_prgBankHi = static_cast<uint8_t>(m_prgBankLo + 1) & m_prgMask;
            }
            else {
                m_prgBankLo = prgBank;
                m_prgBankHi = prgBank;
            }
        }
        else {
            if(sFlag) {
                if(lFlag) {
                    m_prgBankLo = static_cast<uint8_t>(prgBank & 0x3E) & m_prgMask;
                    m_prgBankHi = static_cast<uint8_t>(prgBank | 0x07) & m_prgMask;
                }
                else {
                    m_prgBankLo = static_cast<uint8_t>(prgBank & 0x3E) & m_prgMask;
                    m_prgBankHi = static_cast<uint8_t>(prgBank & 0x38) & m_prgMask;
                }
            }
            else {
                if(lFlag) {
                    m_prgBankLo = prgBank;
                    m_prgBankHi = static_cast<uint8_t>(prgBank | 0x07) & m_prgMask;
                }
                else {
                    m_prgBankLo = prgBank;
                    m_prgBankHi = static_cast<uint8_t>(prgBank & 0x38) & m_prgMask;
                }
            }
        }

        m_horizontalMirroring = (absolute & 0x02) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBankLo, addr);
        return cd().readPrg<BankSize::B16K>(m_prgBankHi, addr);
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
        m_prgBankLo = 0;
        m_prgBankHi = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBankLo);
        SERIALIZEDATA(s, m_prgBankHi);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
