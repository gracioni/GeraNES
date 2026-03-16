#pragma once

#include "GeraNES/defines.h"
#include "ICartridgeData.h"
#include "GeraNES/util/FileUtil.h"

#include <string>
#include <vector>

class _FdsFormat : public ICartridgeData
{
public:
    static constexpr int FDS_SIDE_SIZE = 65500;
    static constexpr int FDS_HEADER_SIZE = 16;
    static constexpr int FDS_MAPPER_ID = 20;

private:
    bool m_isValid = false;
    std::string m_error;
    std::vector<std::vector<uint8_t>> m_sides;

    bool hasFdsHeader() const
    {
        return m_romFile.size() >= FDS_HEADER_SIZE &&
            m_romFile.data(0) == 'F' &&
            m_romFile.data(1) == 'D' &&
            m_romFile.data(2) == 'S' &&
            m_romFile.data(3) == 0x1A;
    }

public:
    _FdsFormat(RomFile& romFile)
        : ICartridgeData(romFile)
    {
        size_t start = 0;
        size_t sideCount = 0;

        if(hasFdsHeader()) {
            start = FDS_HEADER_SIZE;
            sideCount = m_romFile.data(4);
        }

        std::vector<uint8_t> biosData;
        if(!readBinaryFile("disksys.rom", biosData) || biosData.size() < 0x2000) {
            m_error = "FDS BIOS 'disksys.rom' not found in executable directory";
            return;
        }

        const size_t payloadSize = m_romFile.size() - start;
        if(sideCount == 0) {
            if(payloadSize == 0 || (payloadSize % FDS_SIDE_SIZE) != 0) {
                m_error = "invalid FDS payload size";
                return;
            }
            sideCount = payloadSize / FDS_SIDE_SIZE;
        }

        if(payloadSize < sideCount * static_cast<size_t>(FDS_SIDE_SIZE)) {
            m_error = "FDS file shorter than declared side count";
            return;
        }

        m_sides.resize(sideCount);
        for(size_t side = 0; side < sideCount; ++side) {
            const size_t sideStart = start + side * static_cast<size_t>(FDS_SIDE_SIZE);
            m_sides[side].resize(FDS_SIDE_SIZE);
            for(int i = 0; i < FDS_SIDE_SIZE; ++i) {
                m_sides[side][static_cast<size_t>(i)] = m_romFile.data(sideStart + static_cast<size_t>(i));
            }
        }

        m_isValid = !m_sides.empty();
        if(m_isValid) {
            log("(FDS)");
        } else {
            m_error = "FDS file has no disk sides";
        }
    }

    bool valid() override { return m_isValid; }
    const std::string& error() const { return m_error; }

    int prgSize() override { return 0; }
    int chrSize() override { return 0; }
    MirroringType mirroringType() override { return MirroringType::HORIZONTAL; }
    bool hasBattery() override { return false; }
    bool hasTrainer() override { return false; }
    bool useFourScreenMirroring() override { return false; }
    int mapperId() override { return FDS_MAPPER_ID; }
    int subMapperId() override { return 0; }
    int ramSize() override { return 0; }
    int chrRamSize() override { return 0x2000; }
    uint8_t readTrainer(int /*addr*/) override { return 0; }
    uint8_t readPrg(int /*addr*/) override { return 0; }
    uint8_t readChr(int /*addr*/) override { return 0; }
    int saveRamSize() override { return 0; }
    std::string chip() override { return "FDS"; }
    GameDatabase::System sistem() override { return GameDatabase::System::FDS; }
    GameDatabase::InputType inputType() override { return GameDatabase::InputType::Unspecified; }
    GameDatabase::PpuModel vsPpuModel() override { return GameDatabase::PpuModel::Ppu2C02; }

    GERANES_INLINE int sideCount() const
    {
        return static_cast<int>(m_sides.size());
    }

    GERANES_INLINE const std::vector<uint8_t>& sideData(int index) const
    {
        return m_sides[static_cast<size_t>(index)];
    }
};
