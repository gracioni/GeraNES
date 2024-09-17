#ifndef MAPPER009_H
#define MAPPER009_H

#include "IMapper.h"

//MMC2 PxROM
class Mapper009 : public IMapper
{
private:

    uint8_t m_PRGRegMask = 0;
    uint8_t m_CHRMask = 0;

    uint8_t m_PRGReg = 0;
    uint8_t m_CHRReg0A = 0;
    uint8_t m_CHRReg0B = 0;
    uint8_t m_CHRReg1A = 0;
    uint8_t m_CHRReg1B = 0;
    bool m_mirrorMode = false;

    bool m_latch1 = false;
    bool m_latch2 = false;

    uint8_t numberOfPRG8kBanks = 0;

public:

    Mapper009(ICartridgeData& cd) : IMapper(cd)
    {
        numberOfPRG8kBanks  = m_cd.numberOfPRGBanks<W8K>();

        m_PRGRegMask = calculateMask(numberOfPRG8kBanks);
        m_CHRMask = calculateMask(m_cd.numberOfCHRBanks<W4K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(addr >= 0x2000 && addr < 0x3000) {
            m_PRGReg = data&m_PRGRegMask;
        }
        else if (addr < 0x4000) {
            m_CHRReg0A = data&m_CHRMask;
        }
        else if (addr < 0x5000) {
            m_CHRReg0B = data&m_CHRMask;
        }
        else if (addr < 0x6000) {
            m_CHRReg1A = data&m_CHRMask;
        }
        else if (addr < 0x7000) {
            m_CHRReg1B = data&m_CHRMask;
        }
        else if (addr < 0x8000) {
            m_mirrorMode = data&1;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        addr &= 0x7FFF;

        if(addr < 0x2000) return m_cd.readPrg<W8K>(m_PRGReg,addr);
        else if(addr < 0x4000) return m_cd.readPrg<W8K>(numberOfPRG8kBanks-3,addr);
        else if(addr < 0x6000) return m_cd.readPrg<W8K>(numberOfPRG8kBanks-2,addr);

        return m_cd.readPrg<W8K>(numberOfPRG8kBanks-1,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasVRAM()) return IMapper::readChr(addr);

        uint8_t ret = 0;

        addr &= 0x1FFF;

        if(addr < 0x1000) {
            if(!m_latch1) ret = m_cd.readChr<W4K>(m_CHRReg0A, addr);
            else ret = m_cd.readChr<W4K>(m_CHRReg0B, addr);
        }
        else {
            if(!m_latch2) ret = m_cd.readChr<W4K>(m_CHRReg1A, addr);
            else ret = m_cd.readChr<W4K>(m_CHRReg1B, addr);
        }

        if(addr == 0x0FD8) m_latch1 = false;
        else if(addr == 0x0FE8) m_latch1 = true;
        else if(addr >= 0x1FD8 && addr <= 0x1FDF) m_latch2 = false;
        else if(addr >= 0x1FE8 && addr <= 0x1FEF) m_latch2 = true;


        return ret;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_mirrorMode) return MirroringType::HORIZONTAL;
        return MirroringType::VERTICAL;
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGRegMask);
        SERIALIZEDATA(s, m_CHRMask);

        SERIALIZEDATA(s, m_PRGReg);
        SERIALIZEDATA(s, m_CHRReg0A);
        SERIALIZEDATA(s, m_CHRReg0B);
        SERIALIZEDATA(s, m_CHRReg1A);
        SERIALIZEDATA(s, m_CHRReg1B);
        SERIALIZEDATA(s, m_mirrorMode);

        SERIALIZEDATA(s, m_latch1);
        SERIALIZEDATA(s, m_latch2);

        SERIALIZEDATA(s, numberOfPRG8kBanks);
    }

};

#endif // MAPPER009_H
