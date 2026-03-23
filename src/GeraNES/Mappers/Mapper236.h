#pragma once

#include "BaseMapper.h"

class Mapper236 : public BaseMapper
{
private:
    uint8_t m_bankMode = 0;
    uint8_t m_outerBank = 0;
    uint8_t m_prgReg = 0;
    uint8_t m_chrReg = 0;
    bool m_useOuterBankForPrg = false;
    uint8_t m_prgBankLo = 0;
    uint8_t m_prgBankHi = 7;
    bool m_horizontalMirroring = false;

    GERANES_INLINE void updateState()
    {
        switch(m_bankMode) {
        case 0x20: {
            const uint8_t bank = static_cast<uint8_t>((m_outerBank | m_prgReg) & 0xFE);
            m_prgBankLo = bank;
            m_prgBankHi = static_cast<uint8_t>(bank + 1);
            break;
        }
        case 0x30:
            m_prgBankLo = static_cast<uint8_t>(m_outerBank | m_prgReg);
            m_prgBankHi = m_prgBankLo;
            break;
        case 0x00:
        case 0x10:
        default:
            m_prgBankLo = static_cast<uint8_t>(m_outerBank | m_prgReg);
            m_prgBankHi = static_cast<uint8_t>(m_outerBank | 0x07);
            break;
        }
    }

public:
    Mapper236(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrSize() == 0 && cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B8K));
        m_useOuterBankForPrg = cd.chrSize() == 0;
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*value*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if((absolute & 0x4000) != 0) {
            m_bankMode = static_cast<uint8_t>(absolute & 0x30);
            m_prgReg = static_cast<uint8_t>(absolute & 0x07);
        }
        else {
            m_horizontalMirroring = (absolute & 0x20) != 0;
            if(m_useOuterBankForPrg) {
                m_outerBank = static_cast<uint8_t>((absolute & 0x03) << 3);
            }
            else {
                m_chrReg = static_cast<uint8_t>(absolute & 0x07);
            }
        }

        updateState();
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(m_bankMode == 0x10) {
            return BaseMapper::readSaveRam(addr & 0x1FF0);
        }
        return BaseMapper::readSaveRam(addr);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBankLo, addr);
        return cd().readPrg<BankSize::B16K>(m_prgBankHi, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) {
            return BaseMapper::readChr(addr);
        }
        return cd().readChr<BankSize::B8K>(m_chrReg, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_bankMode = 0;
        m_outerBank = 0;
        m_prgReg = 0;
        m_chrReg = 0;
        m_prgBankLo = 0;
        m_prgBankHi = 7;
        m_horizontalMirroring = false;
        updateState();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bankMode);
        SERIALIZEDATA(s, m_outerBank);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_chrReg);
        SERIALIZEDATA(s, m_useOuterBankForPrg);
        SERIALIZEDATA(s, m_prgBankLo);
        SERIALIZEDATA(s, m_prgBankHi);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
