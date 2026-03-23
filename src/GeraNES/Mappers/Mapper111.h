#pragma once

#include <array>

#include "BaseMapper.h"

class Mapper111 : public BaseMapper
{
private:
    std::array<uint8_t, 0x4000> m_chrRamGtrom = {};
    uint8_t m_prgMask = 0;
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_ntBank = 0;

    GERANES_INLINE void updateRegister(uint8_t value)
    {
        m_prgBank = static_cast<uint8_t>(value & 0x0F) & m_prgMask;
        m_chrBank = static_cast<uint8_t>((value >> 4) & 0x01);
        m_ntBank = static_cast<uint8_t>((value >> 5) & 0x01);
    }

public:
    Mapper111(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
        reset();
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT void writeMapperRegister(int /*addr*/, uint8_t data) override
    {
        updateRegister(data);
    }

    GERANES_HOT uint8_t readMapperRegister(int /*addr*/, uint8_t openBusData) override
    {
        updateRegister(openBusData);
        return openBusData;
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t data) override
    {
        updateRegister(data);
    }

    GERANES_HOT uint8_t readSaveRam(int /*addr*/) override
    {
        return 0;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return m_chrRamGtrom[(static_cast<size_t>(m_chrBank) << 13) | static_cast<size_t>(addr & 0x1FFF)];
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        m_chrRamGtrom[(static_cast<size_t>(m_chrBank) << 13) | static_cast<size_t>(addr & 0x1FFF)] = data;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::FOUR_SCREEN;
    }

    GERANES_HOT bool useCustomNameTable(uint8_t /*index*/) override
    {
        return true;
    }

    GERANES_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr) override
    {
        return m_chrRamGtrom[(static_cast<size_t>(m_ntBank) << 13) | (static_cast<size_t>(index & 0x03) << 10) | static_cast<size_t>(addr & 0x03FF)];
    }

    GERANES_HOT void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data) override
    {
        m_chrRamGtrom[(static_cast<size_t>(m_ntBank) << 13) | (static_cast<size_t>(index & 0x03) << 10) | static_cast<size_t>(addr & 0x03FF)] = data;
    }

    void reset() override
    {
        m_chrRamGtrom.fill(0);
        m_prgBank = 0;
        m_chrBank = 0;
        m_ntBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_chrRamGtrom.data(), 1, m_chrRamGtrom.size());
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_ntBank);
    }
};
