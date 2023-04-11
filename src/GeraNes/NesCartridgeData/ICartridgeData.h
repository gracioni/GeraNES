#ifndef INCLUDE_INESCARTRIDGEDATA
#define INCLUDE_INESCARTRIDGEDATA

#include "GeraNes/defines.h"
#include "GeraNes/functions.h"
#include <string>
#include <sstream>
#include <vector>

#include "GeraNes/RomFile.h"

class ICartridgeData
{
protected:

    RomFile& m_romFile;

public:

    ICartridgeData(RomFile& romFile) : m_romFile(romFile) {
    };

    const RomFile& romFile(void) {
        return m_romFile;
    }

    virtual std::string error(void) = 0;

    virtual int numberOfPRG16kBanks(void) = 0;

    virtual int numberOfCHR8kBanks(void) = 0;

    virtual int mirroringType(void) = 0;

    /*
      Indicates the presence of battery-backed RAM at
      memory locations $6000-$7FFF.
    */
    virtual bool hasBatteryRAM8k(void) = 0;

    virtual bool hasTrainer(void) = 0;

    virtual bool useFourScreenMirroring(void) = 0;

    //iNes mapper numbers as reference
    virtual int mapperNumber(void) = 0;

    virtual int numberOfRAMBanks(void) = 0;

    virtual uint8_t readTrainer(int addr) = 0;

    virtual uint8_t readPRG(int addr) = 0;

    virtual uint8_t readCHR(int addr) = 0;

    GERANES_INLINE std::string debug(void)
    {
        std::stringstream aux;

        aux << m_romFile.debug() << std::endl;

        aux << "Number of PRG-ROM banks: " << numberOfPRG16kBanks() << std::endl;
        aux << "Number of CHR-ROM banks: " << numberOfCHR8kBanks() << std::endl;
        aux << "Mirroring type: " << mirroringType() << std::endl;
        aux << "Has battery-backed RAM: " << hasBatteryRAM8k() << std::endl;
        aux <<  "Has trainer: " << hasTrainer() << std::endl;
        aux << "Use four-screen mirroring: " << useFourScreenMirroring() << std::endl;
        aux << "Number of 8KB RAM banks: " << numberOfRAMBanks() << std::endl;
        aux << "Mapper: " << mapperNumber() <<  " (" << getMapperName(mapperNumber()) << ")" << std::endl;



        return aux.str();
    }

    virtual ~ICartridgeData() {}

    template<int WindowSize>
    GERANES_INLINE uint8_t readPRG(int bank, int addr)
    {
        return readPRG(bank*WindowSize + (addr&(WindowSize-1)));
    }

    template<int WindowSize>
    GERANES_INLINE uint8_t readCHR(int bank, int addr)
    {
        return readCHR(bank*WindowSize + (addr&(WindowSize-1)));
    }

    template<int WindowSize>
    GERANES_INLINE uint32_t numberOfPRGBanks()
    {
        uint32_t size = numberOfPRG16kBanks() * 0x4000;
        return size/WindowSize;
    }

    template<int WindowSize>
    GERANES_INLINE uint32_t numberOfCHRBanks()
    {
        uint32_t size = numberOfCHR8kBanks() * 0x2000;
        return size/WindowSize;
    }

};

#endif
