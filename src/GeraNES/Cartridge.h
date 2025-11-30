#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "defines.h"
#include "util/MapperUtil.h"
#include "NesCartridgeData/ICartridgeData.h"
#include "NesCartridgeData/_INesFormat.h"
#include "NesCartridgeData/DbOverwriteCartridgeData.h"
#include "Logger.h"

#include "Mappers/DummyMapper.h"

#include "Mappers/BaseMapper.h"
#include "Mappers/Mapper000.h"
#include "Mappers/Mapper001.h"
#include "Mappers/Mapper002.h"
#include "Mappers/Mapper003.h"

#include "Mappers/Mapper004.h"
#include "Mappers/Mapper004_3.h"


#include "Mappers/Mapper007.h"
#include "Mappers/Mapper009.h"
#include "Mappers/Mapper010.h"
#include "Mappers/Mapper011.h"
#include "Mappers/Mapper013.h"
#include "Mappers/Mapper015.h"
#include "Mappers/Mapper016.h"
#include "Mappers/Mapper018.h"
#include "Mappers/Mapper019.h"
#include "Mappers/Mapper021.h"
#include "Mappers/Mapper022.h"
#include "Mappers/Mapper023.h"
#include "Mappers/Mapper024.h"
#include "Mappers/Mapper025.h"
#include "Mappers/Mapper026.h"
#include "Mappers/Mapper034.h"

#include "Mappers/Mapper064.h"
#include "Mappers/Mapper065.h"
#include "Mappers/Mapper066.h"
#include "Mappers/Mapper069.h"
#include "Mappers/Mapper071.h"


#include "Mappers/Mapper118.h"
#include "Mappers/Mapper119.h"
#include "Mappers/Mapper210.h"

#include "RomFile.h"

#include "Serialization.h"

#include "GameDatabase.h"

class Cartridge
{
private:

    BaseMapper* m_mapper;
    ICartridgeData* m_nesCartridgeData;
    DummyMapper m_dummyMapper;

    bool m_isValid;

    RomFile m_romFile;

    BaseMapper* CreateMapper()
    {
        switch(m_nesCartridgeData->mapperId())
        {
        case 0: return new Mapper000(*m_nesCartridgeData);
        case 1: return new Mapper001(*m_nesCartridgeData);
        case 2: return new Mapper002(*m_nesCartridgeData);
        case 3: return new Mapper003(*m_nesCartridgeData);
        case 4: {
            
            if(m_nesCartridgeData->subMapperId() == 3 ) {
                return new Mapper004_3(*m_nesCartridgeData);
            }             

            return new Mapper004(*m_nesCartridgeData);
        }

        case 7: return new Mapper007(*m_nesCartridgeData);

        case 9: return new Mapper009(*m_nesCartridgeData);
        case 10: return new Mapper010(*m_nesCartridgeData);
        case 11: return new Mapper011(*m_nesCartridgeData);
        case 13: return new Mapper013(*m_nesCartridgeData);
        case 15: return new Mapper015(*m_nesCartridgeData);
        case 16: return new Mapper016(*m_nesCartridgeData);
        case 18: return new Mapper018(*m_nesCartridgeData);
        case 19: return new Mapper019(*m_nesCartridgeData);
        case 21: return new Mapper021(*m_nesCartridgeData);
        case 22: return new Mapper022(*m_nesCartridgeData);
        case 23: return new Mapper023(*m_nesCartridgeData);
        case 24: return new Mapper024(*m_nesCartridgeData);
        case 25: return new Mapper025(*m_nesCartridgeData);
        case 26: return new Mapper026(*m_nesCartridgeData);
        case 34: return new Mapper034(*m_nesCartridgeData);

        case 64: return new Mapper064(*m_nesCartridgeData);
        case 65: return new Mapper065(*m_nesCartridgeData);
        case 66: return new Mapper066(*m_nesCartridgeData);
        case 69: return new Mapper069(*m_nesCartridgeData);

        case 71: return new Mapper071(*m_nesCartridgeData);

        case 118: return new Mapper118(*m_nesCartridgeData);
        case 119: return new Mapper119(*m_nesCartridgeData);
        case 210: return new Mapper210(*m_nesCartridgeData);

        }         

        return &m_dummyMapper;
    }    

public:

    Cartridge()
    {
        m_isValid = false;
        m_mapper = &m_dummyMapper;
        m_nesCartridgeData = NULL;
    }

    ~Cartridge()
    {
        if(m_mapper != &m_dummyMapper) delete m_mapper;
        if(m_nesCartridgeData != NULL) delete m_nesCartridgeData;
    }

    void clear()
    {
        if(m_mapper != &m_dummyMapper) delete m_mapper;
        if(m_nesCartridgeData != NULL) delete m_nesCartridgeData;

        m_romFile = RomFile();

        m_mapper = &m_dummyMapper;
        m_nesCartridgeData = NULL;

        m_isValid = false;        
    }

    bool open(const std::string& filename)
    {
        clear();

        m_romFile.open(filename);

        if(m_romFile.error() != "") {            
            Logger::instance().log(std::string("Error processing file '") + filename + "': " + m_romFile.error(), Logger::Type::ERROR);
            clear();
            return false;
        }

        //try open various file formats here, currently only iNes is supported
        m_nesCartridgeData = new _INesFormat(m_romFile);

        if(!m_nesCartridgeData->valid())
        {
            clear();
            Logger::instance().log("Invalid ROM", Logger::Type::ERROR);
            return false;
        }

        // try other formats files here

        uint32_t prgCrc = m_nesCartridgeData->prgCrc32();
        uint32_t prgChrCrc = m_nesCartridgeData->prgChrCrc32();

        std::string prgCrcStr = Crc32::toString(prgCrc);
        std::string prgChrCrcStr = Crc32::toString(prgChrCrc);

        GameDatabase::Item* item = GameDatabase::instance().findByCrc(prgChrCrcStr);        

        if(item != nullptr) {
            Logger::instance().log("ROM found in database\nUsing DB header", Logger::Type::INFO);
            m_nesCartridgeData = new DbOverwriteCartridgeData(m_nesCartridgeData, item);
            m_nesCartridgeData->log("(DB)");
        }
        else {
            Logger::instance().log("ROM not found in database\nUsing default header", Logger::Type::INFO);
        }

        m_mapper = CreateMapper();

        if(m_mapper == &m_dummyMapper)
        {
            char num[64];

            sprintf(num, "%d", m_nesCartridgeData->mapperId());

            auto msg = std::string("Mapper not supported: (") + num + ")" +
                getMapperName(m_nesCartridgeData->mapperId());

            Logger::instance().log(msg, Logger::Type::ERROR);

            clear();

            return false;
        }
        else m_mapper->init();

        m_isValid = true;

        return true;
    }

    static std::vector<std::string> listFiles(const fs::path& directory) {

        std::vector<std::string> filePaths;

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            // Verifica se é um arquivo (não um diretório)
            if (fs::is_regular_file(entry.status())) {
                filePaths.push_back(entry.path().string());
            }
        }

        return filePaths;
    }

    GERANES_INLINE void writePrg(int addr, uint8_t data)
    {
        m_mapper->writePrg(addr,data);
    }

    GERANES_INLINE uint8_t readPrg(int addr)
    {
        return m_mapper->readPrg(addr);
    }

    GERANES_INLINE void writeSaveRam(int addr, uint8_t data)
    {
        m_mapper->writeSaveRam(addr,data);
    }

    GERANES_INLINE uint8_t readSaveRam(int addr)
    {
        return m_mapper->readSaveRam(addr);
    }

    GERANES_INLINE void writeChr(int addr, uint8_t data)
    {
        m_mapper->writeChr(addr,data);
    }

    GERANES_INLINE uint8_t readChr(int addr)
    {
        return m_mapper->readChr(addr);
    }

    GERANES_INLINE void write0x4000(int addr, uint8_t data)
    {
        m_mapper->write0x4000(addr,data);
    }

    GERANES_INLINE uint8_t read0x4000(int addr, uint8_t openBusData)
    {
        return m_mapper->read0x4000(addr,openBusData);
    }

    GERANES_INLINE MirroringType getMirroringType()
    {
        return m_mapper->mirroringType();
    }

    //return nametable index with preperly mirroring
    GERANES_INLINE_HOT uint8_t mirroring(uint8_t blockIndex)
    {
        static const uint8_t HORIZONTAL_MIRROR[] = {0,0,1,1};
        static const uint8_t VERTICAL_MIRROR[] = {0,1,0,1};
        static const uint8_t FOUR_SCREEN_MIRROR[] = {0,1,2,3};

        switch(m_mapper->mirroringType()){

            case MirroringType::HORIZONTAL:
                return HORIZONTAL_MIRROR[blockIndex];
            case MirroringType::VERTICAL:
                return VERTICAL_MIRROR[blockIndex];
            case MirroringType::SINGLE_SCREEN_A:
                return 0;
            case MirroringType::SINGLE_SCREEN_B:
                return 1;
            case MirroringType::FOUR_SCREEN:
                return FOUR_SCREEN_MIRROR[blockIndex];
            default: //CUSTOM
                return m_mapper->customMirroring(blockIndex);
        }
    }    

    GERANES_INLINE bool getInterruptFlag()
    {
        return m_mapper->getInterruptFlag();
    }

    GERANES_INLINE void setA12State(bool state)
    {
        m_mapper->setA12State(state);
    }

    GERANES_INLINE void cycle()
    {    
        m_mapper->cycle();
    }    

    GERANES_INLINE_HOT bool useCustomNameTable(uint8_t index)
    {
        return m_mapper->useCustomNameTable(index);
    }

    GERANES_INLINE_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr)
    {
        return m_mapper->readCustomNameTable(index,addr);
    }

    GERANES_INLINE GameDatabase::System system()
    {
        return m_nesCartridgeData->system();
    }

    GERANES_INLINE bool isValid()
    {
        return m_isValid;
    }

    GERANES_INLINE const RomFile& romFile() {
        return m_romFile;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_isValid);
        m_mapper->serialization(s);
    }

};

#endif
