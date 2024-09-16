#ifndef INES_FORMAT_H
#define INES_FORMAT_H

#include "GeraNES/defines.h"
#include "ICartridgeData.h"

#define INES_HEADER_SIZE 16

#include <fstream>
#include <string>
#include <vector>
#include <string>
#include <cstring>

class _INesFormat : public ICartridgeData
{
private:

    std::string m_error;

    size_t m_PRGStartIndex;
    size_t m_CHRStartIndex; 

public:

    _INesFormat(RomFile& romFile) : ICartridgeData(romFile)
    {
        m_error = "invalid iNes file";

        if(m_romFile.size() < 4) return;

        const char iNesBytes[] = "NES\x1A";
        size_t size = strlen(iNesBytes);

        char aux[size];
        for(int i = 0; i < size; ++i) aux[i] = m_romFile.data(i);

        if(strncmp(aux, iNesBytes, size) != 0) return;

        m_PRGStartIndex = INES_HEADER_SIZE + (hasTrainer()?512:0);
        m_CHRStartIndex = INES_HEADER_SIZE + (hasTrainer()?512:0) + numberOfPrg16kBanks()*0x4000/*16KB*/;

        m_error = "";
    }

    GERANES_INLINE std::string error() override
    {
        return m_error;
    }

    GERANES_INLINE int numberOfPrg16kBanks() override
    {
        return m_romFile.data(4);
    }

    GERANES_INLINE int numberOfChr8kBanks() override
    {
        return m_romFile.data(5);
    }

    GERANES_INLINE int mirroringType() override
    {
        /*
        Byte 6
        Bit 0 - Indicates the type of mirroring used by the game
        where 0 indicates horizontal mirroring, 1 indicates
        vertical mirroring.
        */
        return m_romFile.data(6) & 0x01;
    }

    GERANES_INLINE bool hasBatteryRam8k() override
    {
        /*
        Byte 6
        Bit 1 - Indicates the presence of battery-backed RAM at
        memory locations $6000-$7FFF.
        */
        return m_romFile.data(6) & 0x02;
    }

    GERANES_INLINE bool hasTrainer() override
    {
        /*
        Byte 6
        Bit 2 - Indicates the presence of a 512-uint8_t trainer at
        memory locations $7000-$71FF.
        */
        return m_romFile.data(6) & 0x04;
    }

    GERANES_INLINE bool useFourScreenMirroring() override
    {
        /*
        Byte 6
        Bit 3 - If this bit is set it overrides bit 0 to indicate four-
        screen mirroring should be used.
        */
        return m_romFile.data(6) & 0x08;
    }

    GERANES_INLINE int mapperNumber() override
    {
        return ((m_romFile.data(6)&0xF0) >> 4) | (m_romFile.data(7)&0xF0);
    }

    /*
    Byte 8
    Number of 8 KB RAM banks. For compatibility with previous
    versions of the iNES format, assume 1 page of RAM when
    this is 0.
    */
    GERANES_INLINE_HOT int numberOfRamBanks() override
    {
        int n = m_romFile.data(8);
        if(n == 0) n = 1;
        return n;
    }

    GERANES_INLINE_HOT uint8_t readTrainer(int addr) override
    {
        if(hasTrainer()) return m_romFile.data(INES_HEADER_SIZE + addr);
        return 0;
    }

    GERANES_INLINE_HOT uint8_t readPrg(int addr) override
    {
        return m_romFile.data(m_PRGStartIndex + addr);
    }

    GERANES_INLINE_HOT uint8_t readChr(int addr) override
    {
        return m_romFile.data(m_CHRStartIndex + addr);
    }

};

#endif
