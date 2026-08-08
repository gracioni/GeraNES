#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "GeraNES/Settings.h"
#include "GeraNES/APU/NoiseChannel.h"
#include "GeraNES/Cartridge.h"
#include "GeraNES/PPU.h"

using namespace GeraNES;

namespace
{
void advancePpuTo(PPU& ppu, int scanline, int cycle)
{
    constexpr int kMaximumFrameCycles = 312 * 341;
    for(int elapsed = 0; elapsed <= kMaximumFrameCycles; ++elapsed) {
        if(ppu.scanline() == scanline && ppu.cycle() == cycle) {
            return;
        }
        ppu.ppuCycle();
    }

    FAIL("PPU did not reach the expected Dendy coordinate");
}

class TemporaryNesFile
{
    std::filesystem::path m_path;

public:
    explicit TemporaryNesFile(uint8_t timingMode)
    {
        static uint32_t nextId = 0;
        m_path = std::filesystem::temp_directory_path() /
            ("geranes_dendy_timing_" + std::to_string(++nextId) + ".nes");

        std::vector<uint8_t> rom(16u + 0x4000u, 0);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1A;
        rom[4] = 1;       // One 16 KiB PRG-ROM bank.
        rom[7] = 0x08;    // NES 2.0 identifier.
        rom[11] = 0x07;   // 8 KiB CHR-RAM.
        rom[12] = timingMode;

        std::ofstream output(m_path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
        REQUIRE(static_cast<bool>(output));
    }

    ~TemporaryNesFile()
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    std::string path() const
    {
        return m_path.string();
    }
};
}

TEST_CASE("Dendy uses PAL frame length with NTSC-derived clocks", "[dendy][timing]")
{
    Settings settings;
    settings.setRegion(Settings::Region::DENDY);

    CHECK(settings.CPUClockHz() == 1773448);
    CHECK(settings.PPULinesPerFrame() == 312);

    NoiseChannel noise(settings);
    noise.write(0x0002, 0x0F);
    CHECK(noise.getPeriod() == 4068); // NTSC/Dendy, not the PAL value 3778.
}

TEST_CASE("Dendy vblank begins on scanline 291", "[dendy][ppu][timing]")
{
    Settings settings;
    settings.setRegion(Settings::Region::DENDY);
    Cartridge cartridge;
    PPU ppu(settings, cartridge);

    advancePpuTo(ppu, 241, 2);
    CHECK((ppu.readPPUSTATUS(false) & 0x80) == 0);

    advancePpuTo(ppu, 291, 2);
    CHECK((ppu.readPPUSTATUS(false) & 0x80) != 0);

    advancePpuTo(ppu, 311, 2);
    CHECK((ppu.readPPUSTATUS(false) & 0x80) == 0);
}

TEST_CASE("NES 2.0 timing mode selects Dendy automatically", "[dendy][rom][nes2]")
{
    TemporaryNesFile rom(0x03);
    Cartridge cartridge;

    REQUIRE(cartridge.openRom(rom.path()));
    CHECK(cartridge.system() == GameDatabase::System::Dendy);
}
