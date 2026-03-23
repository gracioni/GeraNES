#pragma once

#include "BaseMapper.h"
#include "Helpers/TxcChip.h"

class Mapper220 : public BaseMapper
{
private:
    TxcChip m_txc = TxcChip(false);
    uint8_t m_chrBank = 0;
    uint8_t m_prgBank = 0;

    GERANES_INLINE void updateState()
    {
        m_prgBank = static_cast<uint8_t>(m_txc.output() & 0x03);
    }

public:
    Mapper220(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        if(addr >= 0x4100 && addr <= 0x5FFF) {
            if((addr & 0xF200) == 0x4200) {
                m_chrBank = value;
            }
            m_txc.write(addr, static_cast<uint8_t>((value >> 4) & 0x03));
            updateState();
        }
    }

    GERANES_HOT uint8_t readMapperRegisterAbsolute(uint16_t addr, uint8_t openBusData) override
    {
        if(addr >= 0x4100 && addr <= 0x5FFF) {
            uint8_t value = openBusData;
            if((addr & 0x0103) == 0x0100) {
                value = static_cast<uint8_t>((openBusData & 0xCF) | ((m_txc.read() << 4) & 0x30));
            }
            updateState();
            return value;
        }
        return openBusData;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        writeMapperRegisterAbsolute(static_cast<uint16_t>(addr + 0x8000), value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(m_chrBank, addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(m_chrBank, addr, data);
    }

    void reset() override
    {
        m_txc.reset();
        m_chrBank = 0;
        m_prgBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        m_txc.serialization(s);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_prgBank);
    }
};
