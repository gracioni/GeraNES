#ifndef INCLUDE_INESCARTRIDGEDATA
#define INCLUDE_INESCARTRIDGEDATA

#include "GeraNES/defines.h"
#include "GeraNES/functions.h"
#include <string>
#include <sstream>
#include <vector>

#include "GeraNES/RomFile.h"

class ICartridgeData
{
protected:

    RomFile& m_romFile;

public:

    ICartridgeData(RomFile& romFile) : m_romFile(romFile) {
    };

    RomFile& romFile() {
        return m_romFile;
    }

    virtual bool valid() = 0;

    virtual int numberOfPrg16kBanks() = 0;

    virtual int numberOfChr8kBanks() = 0;

    virtual int mirroringType() = 0;

    /*
      Indicates the presence of battery-backed RAM at
      memory locations $6000-$7FFF.
    */
    virtual bool hasBatteryRam8k() = 0;

    virtual bool hasTrainer() = 0;

    virtual bool useFourScreenMirroring() = 0;

    //iNes mapper numbers as reference
    virtual int mapperNumber() = 0;

    virtual int numberOfRamBanks() = 0;

    virtual uint8_t readTrainer(int addr) = 0;

    virtual uint8_t readPrg(int addr) = 0;

    virtual uint8_t readChr(int addr) = 0;

    GERANES_INLINE std::string debug()
    {
        std::stringstream aux;

        aux << m_romFile.debug() << std::endl;

        aux << "Number of PRG-ROM banks: " << numberOfPrg16kBanks() << std::endl;
        aux << "Number of CHR-ROM banks: " << numberOfChr8kBanks() << std::endl;
        aux << "Mirroring type: " << mirroringType() << std::endl;
        aux << "Has battery-backed RAM: " << hasBatteryRam8k() << std::endl;
        aux <<  "Has trainer: " << hasTrainer() << std::endl;
        aux << "Use four-screen mirroring: " << useFourScreenMirroring() << std::endl;
        aux << "Number of 8KB RAM banks: " << numberOfRamBanks() << std::endl;
        aux << "Mapper: " << mapperNumber() <<  " (" << getMapperName(mapperNumber()) << ")" << std::endl;



        return aux.str();
    }

    virtual ~ICartridgeData() {}

    template<int WindowSize>
    GERANES_INLINE uint8_t readPrg(int bank, int addr)
    {
        return readPrg(bank*WindowSize + (addr&(WindowSize-1)));
    }

    template<int WindowSize>
    GERANES_INLINE uint8_t readChr(int bank, int addr)
    {
        return readChr(bank*WindowSize + (addr&(WindowSize-1)));
    }

    template<int WindowSize>
    GERANES_INLINE uint32_t numberOfPRGBanks()
    {
        uint32_t size = numberOfPrg16kBanks() * 0x4000;
        return size/WindowSize;
    }

    template<int WindowSize>
    GERANES_INLINE uint32_t numberOfCHRBanks()
    {
        uint32_t size = numberOfChr8kBanks() * 0x2000;
        return size/WindowSize;
    }

};

#endif
