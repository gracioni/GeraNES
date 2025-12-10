#pragma once

#include "GeraNES/defines.h"
#include "GeraNES/util/MapperUtil.h"
#include <string>
#include <sstream>
#include <vector>

#include "GeraNES/RomFile.h"
#include "GeraNES/util/Crc32.h"

#include "GeraNES/GameDatabase.h"

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

    virtual int prgSize() = 0;

    virtual int chrSize() = 0;

    virtual MirroringType mirroringType() = 0;

    /*
      Indicates the presence of battery-backed RAM at
      memory locations $6000-$7FFF.
    */
    virtual bool hasBattery() = 0;

    virtual bool hasTrainer() = 0;

    virtual bool useFourScreenMirroring() = 0;

    virtual int mapperId() = 0;

    virtual int subMapperId() = 0;

    virtual int ramSize() = 0;

    virtual int chrRamSize() = 0;   

    virtual uint8_t readTrainer(int addr) = 0;

    virtual uint8_t readPrg(int addr) = 0;

    virtual uint8_t readChr(int addr) = 0;

    virtual int saveRamSize() = 0;

    virtual std::string chip() = 0; 

    virtual GameDatabase::Sistem sistem() = 0;

    virtual GameDatabase::InputType inputType() = 0;

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
        return prgSize()/WindowSize;
    }

    template<int WindowSize>
    GERANES_INLINE uint32_t numberOfCHRBanks()
    {
        return chrSize()/WindowSize;
    }

    uint32_t prgCrc32() {

        Crc32 crc;

        for(int i = 0; i < prgSize(); i++) {
            crc.add(readPrg(i));
        }

        return crc.get();
    }

    uint32_t prgChrCrc32() {

        Crc32 crc(prgCrc32());

        for(int i = 0; i < chrSize(); i++) {
            crc.add(readChr(i));
        }

        return crc.get();
    }    

    void log(const std::string& prefix)
    {
        std::stringstream aux;

        aux << prefix << " PRG-ROM size: " << prgSize() << " bytes" << std::endl;
        aux << prefix << " CHR-ROM size: " << chrSize() << " bytes" << std::endl;
        aux << prefix << " Mirroring type: " << (int)mirroringType() << std::endl;
        aux << prefix << " Has battery: " << hasBattery() << std::endl;
        aux << prefix << " Has trainer: " << hasTrainer() << std::endl;
        aux << prefix << " Use four-screen mirroring: " << useFourScreenMirroring() << std::endl;
        aux << prefix << " RAM size: " << ramSize() << " bytes" << std::endl;
        aux << prefix << " Mapper: " << mapperId() <<  " (" << getMapperName(mapperId()) << ")" << std::endl;
        aux << prefix << " PRG CRC32: " << Crc32::toString(prgCrc32()) << std::endl;
        aux << prefix << " PRG+CHR CRC32: " << Crc32::toString(prgChrCrc32());

        Logger::instance().log(aux.str(), Logger::Type::INFO);
    }

};
