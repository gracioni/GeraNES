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

    virtual MirroringType mirroringType() = 0;

    /*
      Indicates the presence of battery-backed RAM at
      memory locations $6000-$7FFF.
    */
    virtual bool hasBattery() = 0;

    virtual bool hasTrainer() = 0;

    virtual bool useFourScreenMirroring() = 0;

    virtual int mapperId() = 0;

    virtual int numberOf8kRamBanks() = 0;

    virtual uint8_t readTrainer(int addr) = 0;

    virtual uint8_t readPrg(int addr) = 0;

    virtual uint8_t readChr(int addr) = 0;

    virtual int SaveRamSize() = 0;

    GERANES_INLINE std::string debug()
    {
        std::stringstream aux;

        aux << m_romFile.debug() << std::endl;

        aux << "Number of PRG-ROM banks: " << numberOfPrg16kBanks() << std::endl;
        aux << "Number of CHR-ROM banks: " << numberOfChr8kBanks() << std::endl;
        aux << "Mirroring type: " << (int)mirroringType() << std::endl;
        aux << "Has battery-backed RAM: " << hasBattery() << std::endl;
        aux <<  "Has trainer: " << hasTrainer() << std::endl;
        aux << "Use four-screen mirroring: " << useFourScreenMirroring() << std::endl;
        aux << "Number of 8KB RAM banks: " << numberOf8kRamBanks() << std::endl;
        aux << "Mapper: " << mapperId() <<  " (" << getMapperName(mapperId()) << ")" << std::endl;



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
