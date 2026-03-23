#pragma once

#include "BaseMapper.h"
#include "Helpers/TxcChip.h"

class Mapper132 : public BaseMapper
{
protected:
    TxcChip m_txc = TxcChip(false);

    GERANES_INLINE void writeAbsolute(uint16_t absolute, uint8_t value)
    {
        m_txc.write(absolute, static_cast<uint8_t>(value & 0x0F));
    }

public:
    Mapper132(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        writeAbsolute(static_cast<uint16_t>(addr + 0x4000), value);
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if((addr & 0x0103) == 0x0100) {
            return static_cast<uint8_t>((openBusData & 0xF0) | (m_txc.read() & 0x0F));
        }
        return openBusData;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        writeAbsolute(static_cast<uint16_t>(addr + 0x8000), value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(static_cast<uint8_t>((m_txc.output() >> 2) & 0x01), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(static_cast<uint8_t>(m_txc.output() & 0x03), addr);
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
