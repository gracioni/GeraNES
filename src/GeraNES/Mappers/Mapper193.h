#pragma once

#include "BaseMapper.h"

class Mapper193 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank2k0 = 0;
    uint8_t m_chrBank2k1 = 0;
    uint8_t m_chrBank2k2 = 0;

public:
    Mapper193(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        switch(addr & 0x03) {
        case 0:
            m_chrBank2k0 = static_cast<uint8_t>(value >> 1);
            break;
        case 1:
            m_chrBank2k1 = static_cast<uint8_t>(value >> 1);
            break;
        case 2:
            m_chrBank2k2 = static_cast<uint8_t>(value >> 1);
            break;
        case 3:
            m_prgBank = value;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgBank, addr);
        case 1: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 3), addr);
        case 2: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2), addr);
        default: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1), addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) {
            switch((addr >> 11) & 0x03) {
            case 0: return readChrRam<BankSize::B2K>(m_chrBank2k0, addr);
            case 1: return readChrRam<BankSize::B2K>(m_chrBank2k1, addr);
            case 2: return readChrRam<BankSize::B2K>(m_chrBank2k2, addr);
            default: return readChrRam<BankSize::B2K>(static_cast<uint8_t>(m_chrBank2k2 + 1), addr);
            }
        }

        switch((addr >> 11) & 0x03) {
        case 0: return cd().readChr<BankSize::B2K>(m_chrBank2k0, addr);
        case 1: return cd().readChr<BankSize::B2K>(m_chrBank2k1, addr);
        case 2: return cd().readChr<BankSize::B2K>(m_chrBank2k2, addr);
        default: return cd().readChr<BankSize::B2K>(static_cast<uint8_t>(m_chrBank2k2 + 1), addr);
        }
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        switch((addr >> 11) & 0x03) {
        case 0: writeChrRam<BankSize::B2K>(m_chrBank2k0, addr, data); break;
        case 1: writeChrRam<BankSize::B2K>(m_chrBank2k1, addr, data); break;
        case 2: writeChrRam<BankSize::B2K>(m_chrBank2k2, addr, data); break;
        default: writeChrRam<BankSize::B2K>(static_cast<uint8_t>(m_chrBank2k2 + 1), addr, data); break;
        }
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank2k0 = 0;
        m_chrBank2k1 = 0;
        m_chrBank2k2 = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBank2k0);
        SERIALIZEDATA(s, m_chrBank2k1);
        SERIALIZEDATA(s, m_chrBank2k2);
    }
};
