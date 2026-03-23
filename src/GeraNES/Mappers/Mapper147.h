#pragma once

#include "BaseMapper.h"
#include "Helpers/TxcChip.h"

class Mapper147 : public BaseMapper
{
private:
    TxcChip m_txc = TxcChip(true);

    GERANES_INLINE void writeAbsolute(uint16_t absolute, uint8_t value)
    {
        m_txc.write(absolute, static_cast<uint8_t>(((value & 0xFC) >> 2) | ((value & 0x03) << 6)));
    }

    GERANES_INLINE uint8_t prgBank() const
    {
        const uint8_t out = m_txc.output();
        return static_cast<uint8_t>(((out & 0x20) >> 4) | (out & 0x01));
    }

    GERANES_INLINE uint8_t chrBank() const
    {
        return static_cast<uint8_t>((m_txc.output() & 0x1E) >> 1);
    }

public:
    Mapper147(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        writeAbsolute(static_cast<uint16_t>(addr + 0x4000), value);
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if((addr & 0x0103) == 0x0100) {
            const uint8_t v = m_txc.read();
            return static_cast<uint8_t>(((v & 0x3F) << 2) | ((v & 0xC0) >> 6));
        }
        return openBusData;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        writeAbsolute(static_cast<uint16_t>(addr + 0x8000), value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(prgBank(), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(chrBank(), addr);
    }

    void reset() override
    {
        m_txc.reset();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        m_txc.serialization(s);
    }
};
