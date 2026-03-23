#pragma once

#include "BaseMapper.h"

class Mapper178 : public BaseMapper
{
private:
    uint8_t m_regs[4] = {0};
    uint8_t m_workRam[0x8000] = {0};

    GERANES_INLINE void prgBanks(uint16_t& low, uint16_t& high) const
    {
        const uint16_t smallBank = static_cast<uint16_t>(m_regs[1] & 0x07);
        const uint16_t bigBank = m_regs[2];

        if((m_regs[0] & 0x02) != 0) {
            low = static_cast<uint16_t>((bigBank << 3) | smallBank);
            if((m_regs[0] & 0x04) != 0) {
                high = static_cast<uint16_t>((bigBank << 3) | 0x06 | (m_regs[1] & 0x01));
            } else {
                high = static_cast<uint16_t>((bigBank << 3) | 0x07);
            }
        } else {
            const uint16_t bank = static_cast<uint16_t>((bigBank << 3) | smallBank);
            if((m_regs[0] & 0x04) != 0) {
                low = bank;
                high = bank;
            } else {
                low = static_cast<uint16_t>(bank & ~1u);
                high = static_cast<uint16_t>(low + 1);
            }
        }
    }

public:
    Mapper178(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        if(addr >= 0x4800 && addr <= 0x4FFF) {
            m_regs[addr & 0x03] = value;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        uint16_t low = 0;
        uint16_t high = 0;
        prgBanks(low, high);

        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(low, addr);
        return cd().readPrg<BankSize::B16K>(high, addr);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        const uint16_t bank = static_cast<uint16_t>(m_regs[3] & 0x03);
        return m_workRam[(bank << 13) | (addr & 0x1FFF)];
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        const uint16_t bank = static_cast<uint16_t>(m_regs[3] & 0x03);
        m_workRam[(bank << 13) | (addr & 0x1FFF)] = data;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return (m_regs[0] & 0x01) != 0 ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        memset(m_regs, 0, sizeof(m_regs));
        memset(m_workRam, 0, sizeof(m_workRam));
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_regs, 1, 4);
        s.array(m_workRam, 1, sizeof(m_workRam));
    }
};
