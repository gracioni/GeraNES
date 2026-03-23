#pragma once

#include "BaseMapper.h"

// iNES Mapper 246 (G0151-1)
// $6000-$6003: 8KB PRG bank regs
// $6004-$6007: 2KB CHR bank regs
// $6800-$6FFF: 2KB PRG-RAM
class Mapper246 : public BaseMapper
{
private:
    uint8_t m_prgReg[4] = {0, 0, 0, 0xFF};
    uint8_t m_chrReg[4] = {0, 0, 0, 0};
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper246(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        uint8_t bank = m_prgReg[slot];

        if(slot == 3 && (addr & 0x1FF8) == 0x1FE0) {
            bank = static_cast<uint8_t>(bank | 0x10);
        }

        return cd().readPrg<BankSize::B8K>(bank & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 11) & 0x03);
        return cd().readChr<BankSize::B2K>(m_chrReg[slot] & m_chrMask, addr);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(addr < 0x0008) {
            const uint8_t reg = static_cast<uint8_t>(addr & 0x07);
            if(reg < 4) m_prgReg[reg] = data;
            else m_chrReg[reg & 0x03] = data;
            return;
        }

        if(addr >= 0x0800 && addr < 0x1000) {
            if(saveRamData() != nullptr && saveRamSize() > 0) {
                saveRamData()[(addr - 0x0800) & (saveRamSize() - 1)] = data;
            }
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(addr >= 0x0800 && addr < 0x1000) {
            if(saveRamData() != nullptr && saveRamSize() > 0) {
                return saveRamData()[(addr - 0x0800) & (saveRamSize() - 1)];
            }
        }
        return 0;
    }

    void reset() override
    {
        m_prgReg[0] = 0;
        m_prgReg[1] = 0;
        m_prgReg[2] = 0;
        m_prgReg[3] = 0xFF;
        m_chrReg[0] = 0;
        m_chrReg[1] = 0;
        m_chrReg[2] = 0;
        m_chrReg[3] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_prgReg, 1, 4);
        s.array(m_chrReg, 1, 4);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
    }
};
