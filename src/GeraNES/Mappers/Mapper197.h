#pragma once

#include "Mapper004.h"

class Mapper197 : public Mapper004
{
public:
    Mapper197(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(!m_chrMode) {
            switch((addr >> 11) & 0x03) {
            case 0: return readChrBank<BankSize::B4K>(m_chrReg[0] << 1, addr);
            case 1: return readChrBank<BankSize::B2K>(m_chrReg[2] << 1, addr);
            default: return readChrBank<BankSize::B2K>(m_chrReg[3] << 1, addr);
            }
        }

        switch((addr >> 11) & 0x03) {
        case 0: return readChrBank<BankSize::B4K>(m_chrReg[2] << 1, addr);
        case 1: return readChrBank<BankSize::B2K>(m_chrReg[0] << 1, addr);
        default: return readChrBank<BankSize::B2K>(m_chrReg[0] << 1, addr);
        }
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        if(!m_chrMode) {
            switch((addr >> 11) & 0x03) {
            case 0: writeChrBank<BankSize::B4K>(m_chrReg[0] << 1, addr, data); break;
            case 1: writeChrBank<BankSize::B2K>(m_chrReg[2] << 1, addr, data); break;
            default: writeChrBank<BankSize::B2K>(m_chrReg[3] << 1, addr, data); break;
            }
            return;
        }

        switch((addr >> 11) & 0x03) {
        case 0: writeChrBank<BankSize::B4K>(m_chrReg[2] << 1, addr, data); break;
        case 1: writeChrBank<BankSize::B2K>(m_chrReg[0] << 1, addr, data); break;
        default: writeChrBank<BankSize::B2K>(m_chrReg[0] << 1, addr, data); break;
        }
    }
};
