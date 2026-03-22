#pragma once

#include "BaseMapper.h"

class Mapper063 : public BaseMapper
{
private:
    bool m_openBus = false;
    uint16_t m_prgPage[4] = {0, 1, 2, 3};
    uint16_t m_prgMask = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper063(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        m_openBus = ((absolute & 0x0300) == 0x0300);

        const uint16_t baseA = static_cast<uint16_t>((absolute >> 1) & 0x01FC);
        const uint16_t baseB = static_cast<uint16_t>(absolute & 0x07C);

        if(!m_openBus) {
            m_prgPage[0] = static_cast<uint16_t>((baseA | ((absolute & 0x02) ? 0x00 : ((absolute >> 1) & 0x02) | 0x00)) & m_prgMask);
            m_prgPage[1] = static_cast<uint16_t>((baseA | ((absolute & 0x02) ? 0x01 : ((absolute >> 1) & 0x02) | 0x01)) & m_prgMask);
        }

        m_prgPage[2] = static_cast<uint16_t>((baseA | ((absolute & 0x02) ? 0x02 : ((absolute >> 1) & 0x02) | 0x00)) & m_prgMask);
        m_prgPage[3] = static_cast<uint16_t>(((absolute & 0x800)
            ? (baseB | ((absolute & 0x06) ? 0x03 : 0x01))
            : (baseA | ((absolute & 0x02) ? 0x03 : (((absolute >> 1) & 0x02) | 0x01)))) & m_prgMask);

        m_horizontalMirroring = (absolute & 0x01) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        if(m_openBus && slot < 2) return 0;
        return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(m_prgPage[slot] & m_prgMask), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_openBus = false;
        m_prgPage[0] = 0;
        m_prgPage[1] = 1;
        m_prgPage[2] = 2;
        m_prgPage[3] = 3;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_openBus);
        SERIALIZEDATA(s, m_prgPage[0]);
        SERIALIZEDATA(s, m_prgPage[1]);
        SERIALIZEDATA(s, m_prgPage[2]);
        SERIALIZEDATA(s, m_prgPage[3]);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
