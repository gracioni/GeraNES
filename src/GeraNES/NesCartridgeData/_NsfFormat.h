#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include "GeraNES/defines.h"
#include "ICartridgeData.h"

class _NsfFormat : public ICartridgeData
{
public:
    static constexpr int NSF_MAPPER_ID = 0x1000;

private:
    static constexpr int NSF_HEADER_SIZE = 0x80;

    bool m_isValid = false;
    std::string m_error;

    uint8_t m_totalSongs = 1;
    uint8_t m_startSong = 1;
    uint16_t m_loadAddress = 0x8000;
    uint16_t m_initAddress = 0x8000;
    uint16_t m_playAddress = 0x8003;
    uint16_t m_playSpeedNtsc = 16639;
    uint16_t m_playSpeedPal = 19997;
    uint8_t m_flags = 0;
    uint8_t m_soundChipFlags = 0;
    std::array<uint8_t, 8> m_bankInit = {0, 0, 0, 0, 0, 0, 0, 0};

    size_t m_prgStartIndex = NSF_HEADER_SIZE;
    size_t m_prgSize = 0;

    static uint16_t readLe16(const RomFile& rom, size_t offset)
    {
        const uint16_t lo = rom.data(offset);
        const uint16_t hi = rom.data(offset + 1);
        return static_cast<uint16_t>(lo | (hi << 8));
    }

public:
    _NsfFormat(RomFile& romFile)
        : ICartridgeData(romFile)
    {
        if(m_romFile.size() < NSF_HEADER_SIZE) {
            m_error = "invalid NSF header";
            return;
        }

        if(!(m_romFile.data(0) == 'N' && m_romFile.data(1) == 'E' && m_romFile.data(2) == 'S' &&
             m_romFile.data(3) == 'M' && m_romFile.data(4) == 0x1A)) {
            m_error = "invalid NSF signature";
            return;
        }

        m_totalSongs = m_romFile.data(0x06);
        m_startSong = m_romFile.data(0x07);
        m_loadAddress = readLe16(m_romFile, 0x08);
        m_initAddress = readLe16(m_romFile, 0x0A);
        m_playAddress = readLe16(m_romFile, 0x0C);
        m_playSpeedNtsc = readLe16(m_romFile, 0x6E);
        m_playSpeedPal = readLe16(m_romFile, 0x78);
        m_flags = m_romFile.data(0x7A);
        m_soundChipFlags = m_romFile.data(0x7B);
        for(int i = 0; i < 8; ++i) {
            m_bankInit[static_cast<size_t>(i)] = m_romFile.data(0x70 + i);
        }

        if(m_playSpeedNtsc == 0) m_playSpeedNtsc = 16639;
        if(m_playSpeedPal == 0) m_playSpeedPal = 19997;

        if(m_loadAddress < 0x8000) {
            m_error = "NSF load address below $8000 not supported";
            return;
        }

        m_prgSize = m_romFile.size() - NSF_HEADER_SIZE;
        if(m_prgSize == 0) {
            m_error = "NSF payload is empty";
            return;
        }

        m_isValid = true;
        log("(NSF)");
    }

    bool valid() override { return m_isValid; }
    const std::string& error() const { return m_error; }

    int prgSize() override { return static_cast<int>(m_prgSize); }
    int chrSize() override { return 0; }
    MirroringType mirroringType() override { return MirroringType::HORIZONTAL; }
    bool hasBattery() override { return false; }
    bool hasTrainer() override { return false; }
    bool useFourScreenMirroring() override { return false; }
    int mapperId() override { return NSF_MAPPER_ID; }
    int subMapperId() override { return 0; }
    int ramSize() override { return 0x2000; }
    int chrRamSize() override { return 0; }
    uint8_t readTrainer(int /*addr*/) override { return 0; }

    uint8_t readPrg(int addr) override
    {
        if(addr < 0) return 0;
        const size_t index = m_prgStartIndex + static_cast<size_t>(addr);
        if(index < m_romFile.size()) return m_romFile.data(index);
        return 0;
    }

    uint8_t readChr(int /*addr*/) override { return 0; }
    int saveRamSize() override { return 0x2000; }
    std::string chip() override { return "NSF"; }
    GameDatabase::System sistem() override { return GameDatabase::System::NesNtsc; }
    GameDatabase::InputType inputType() override { return GameDatabase::InputType::Unspecified; }
    GameDatabase::PpuModel vsPpuModel() override { return GameDatabase::PpuModel::Ppu2C02; }

    GERANES_INLINE uint8_t totalSongs() const { return m_totalSongs; }
    GERANES_INLINE uint8_t startSong() const { return m_startSong; }
    GERANES_INLINE uint16_t loadAddress() const { return m_loadAddress; }
    GERANES_INLINE uint16_t initAddress() const { return m_initAddress; }
    GERANES_INLINE uint16_t playAddress() const { return m_playAddress; }
    GERANES_INLINE uint16_t playSpeedNtsc() const { return m_playSpeedNtsc; }
    GERANES_INLINE uint16_t playSpeedPal() const { return m_playSpeedPal; }
    GERANES_INLINE uint8_t flags() const { return m_flags; }
    GERANES_INLINE uint8_t soundChipFlags() const { return m_soundChipFlags; }
    GERANES_INLINE uint8_t initRegionValue() const { return (m_flags & 0x01) ? 1 : 0; }
    GERANES_INLINE bool usesMmc5Audio() const { return (m_soundChipFlags & 0x08) != 0; }
    GERANES_INLINE bool usesBankSwitch() const
    {
        for(uint8_t v : m_bankInit) {
            if(v != 0) return true;
        }
        return false;
    }
    GERANES_INLINE uint8_t bankInitReg(int slot) const
    {
        return m_bankInit[static_cast<size_t>(slot & 0x07)];
    }
};
