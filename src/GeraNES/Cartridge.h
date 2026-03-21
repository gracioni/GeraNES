#pragma once

#include "defines.h"
#include "util/MapperUtil.h"
#include "NesCartridgeData/ICartridgeData.h"
#include "NesCartridgeData/_INesFormat.h"
#include "NesCartridgeData/_FdsFormat.h"
#ifdef ENABLE_NSF_PLAYER
#include "NesCartridgeData/_NsfFormat.h"
#endif
#include "NesCartridgeData/DbOverwriteCartridgeData.h"
#include "logger/logger.h"
#include "util/Crc32.h"

#include "Mappers/DummyMapper.h"

#include "Mappers/BaseMapper.h"
#include "Mappers/Mapper000.h"
#include "Mappers/Mapper001.h"
#include "Mappers/Mapper002.h"
#include "Mappers/Mapper003.h"

#include "Mappers/Mapper004.h"
#include "Mappers/Mapper004_3.h"
#include "Mappers/Mapper005.h"
#include "Mappers/Mapper006.h"

#include "Mappers/Mapper007.h"
#include "Mappers/Mapper008.h"
#include "Mappers/Mapper009.h"
#include "Mappers/Mapper010.h"
#include "Mappers/Mapper011.h"
#include "Mappers/Mapper012.h"
#include "Mappers/Mapper013.h"
#include "Mappers/Mapper014.h"
#include "Mappers/Mapper015.h"
#include "Mappers/Mapper016.h"
#include "Mappers/Mapper017.h"
#include "Mappers/Mapper018.h"
#include "Mappers/Mapper019.h"
#include "Mappers/Mapper020.h"
#include "Mappers/Mapper021.h"
#include "Mappers/Mapper022.h"
#include "Mappers/Mapper023.h"
#include "Mappers/Mapper024.h"
#include "Mappers/Mapper025.h"
#include "Mappers/Mapper026.h"
#include "Mappers/Mapper027.h"
#include "Mappers/Mapper028.h"
#include "Mappers/Mapper029.h"
#include "Mappers/Mapper030.h"
#include "Mappers/Mapper031.h"
#include "Mappers/Mapper032.h"
#include "Mappers/Mapper033.h"
#include "Mappers/Mapper034.h"
#include "Mappers/Mapper036.h"
#include "Mappers/Mapper038.h"
#include "Mappers/Mapper041.h"
#include "Mappers/Mapper044.h"
#include "Mappers/Mapper045.h"
#include "Mappers/Mapper048.h"
#include "Mappers/Mapper047.h"

#include "Mappers/Mapper064.h"
#include "Mappers/Mapper065.h"
#include "Mappers/Mapper066.h"
#include "Mappers/Mapper067.h"
#include "Mappers/Mapper068.h"
#include "Mappers/Mapper069.h"
#include "Mappers/Mapper070.h"
#include "Mappers/Mapper071.h"
#include "Mappers/Mapper072.h"
#include "Mappers/Mapper074.h"
#include "Mappers/Mapper075.h"
#include "Mappers/Mapper076.h"
#include "Mappers/Mapper078.h"
#include "Mappers/Mapper079.h"
#include "Mappers/Mapper080.h"
#include "Mappers/Mapper082.h"
#include "Mappers/Mapper089.h"
#include "Mappers/Mapper087.h"
#include "Mappers/Mapper088.h"
#include "Mappers/Mapper091.h"
#include "Mappers/Mapper090.h"
#include "Mappers/Mapper092.h"
#include "Mappers/Mapper093.h"
#include "Mappers/Mapper094.h"
#include "Mappers/Mapper095.h"
#include "Mappers/Mapper097.h"
#include "Mappers/Mapper099.h"
#include "Mappers/Mapper101.h"


#include "Mappers/Mapper113.h"
#include "Mappers/Mapper118.h"
#include "Mappers/Mapper119.h"
#include "Mappers/Mapper154.h"
#include "Mappers/Mapper152.h"
#include "Mappers/Mapper180.h"
#include "Mappers/Mapper185.h"
#include "Mappers/Mapper206.h"
#include "Mappers/Mapper210.h"
#include "Mappers/Mapper232.h"
#include "Mappers/Mapper245.h"
#ifdef ENABLE_NSF_PLAYER
#include "Mappers/MapperNSF.h"
#endif

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
        case 0: return BaseMapper::create<Mapper000>(*m_nesCartridgeData);
        case 1: return BaseMapper::create<Mapper001>(*m_nesCartridgeData);
        case 2: return BaseMapper::create<Mapper002>(*m_nesCartridgeData);
        case 3: return BaseMapper::create<Mapper003>(*m_nesCartridgeData);
        case 4: {
            if(m_nesCartridgeData->subMapperId() == 3) {
                return BaseMapper::create<Mapper004_3>(*m_nesCartridgeData);
            }
            return BaseMapper::create<Mapper004>(*m_nesCartridgeData);
        }
        case 5: return BaseMapper::create<Mapper005>(*m_nesCartridgeData);
        case 6: return BaseMapper::create<Mapper006>(*m_nesCartridgeData);
        case 7: return BaseMapper::create<Mapper007>(*m_nesCartridgeData);
        case 8: return BaseMapper::create<Mapper008>(*m_nesCartridgeData);
        case 9: return BaseMapper::create<Mapper009>(*m_nesCartridgeData);
        case 10: return BaseMapper::create<Mapper010>(*m_nesCartridgeData);
        case 11: return BaseMapper::create<Mapper011>(*m_nesCartridgeData);
        case 12: return BaseMapper::create<Mapper012>(*m_nesCartridgeData);
        case 13: return BaseMapper::create<Mapper013>(*m_nesCartridgeData);
        case 14: return BaseMapper::create<Mapper014>(*m_nesCartridgeData);
        case 15: return BaseMapper::create<Mapper015>(*m_nesCartridgeData);
        case 16: return BaseMapper::create<Mapper016>(*m_nesCartridgeData);
        case 17: return BaseMapper::create<Mapper017>(*m_nesCartridgeData);
        case 18: return BaseMapper::create<Mapper018>(*m_nesCartridgeData);
        case 19: return BaseMapper::create<Mapper019>(*m_nesCartridgeData);
        case 20: return BaseMapper::create<Mapper020>(*m_nesCartridgeData);
        case 21: return BaseMapper::create<Mapper021>(*m_nesCartridgeData);
        case 22: return BaseMapper::create<Mapper022>(*m_nesCartridgeData);
        case 23: return BaseMapper::create<Mapper023>(*m_nesCartridgeData);
        case 24: return BaseMapper::create<Mapper024>(*m_nesCartridgeData);
        case 25: return BaseMapper::create<Mapper025>(*m_nesCartridgeData);
        case 26: return BaseMapper::create<Mapper026>(*m_nesCartridgeData);
        case 27: return BaseMapper::create<Mapper027>(*m_nesCartridgeData);
        case 28: return BaseMapper::create<Mapper028>(*m_nesCartridgeData);
        case 29: return BaseMapper::create<Mapper029>(*m_nesCartridgeData);
        case 30: return BaseMapper::create<Mapper030>(*m_nesCartridgeData);
        case 31: return BaseMapper::create<Mapper031>(*m_nesCartridgeData);
        case 32: return BaseMapper::create<Mapper032>(*m_nesCartridgeData);
        case 33: return BaseMapper::create<Mapper033>(*m_nesCartridgeData);
        case 34: return BaseMapper::create<Mapper034>(*m_nesCartridgeData);
        case 36: return BaseMapper::create<Mapper036>(*m_nesCartridgeData);
        case 38: return BaseMapper::create<Mapper038>(*m_nesCartridgeData);
        case 41: return BaseMapper::create<Mapper041>(*m_nesCartridgeData);
        case 44: return BaseMapper::create<Mapper044>(*m_nesCartridgeData);
        case 45: return BaseMapper::create<Mapper045>(*m_nesCartridgeData);
        case 47: return BaseMapper::create<Mapper047>(*m_nesCartridgeData);
        case 48: return BaseMapper::create<Mapper048>(*m_nesCartridgeData);
        case 64: return BaseMapper::create<Mapper064>(*m_nesCartridgeData);
        case 65: return BaseMapper::create<Mapper065>(*m_nesCartridgeData);
        case 66: return BaseMapper::create<Mapper066>(*m_nesCartridgeData);
        case 67: return BaseMapper::create<Mapper067>(*m_nesCartridgeData);
        case 68: return BaseMapper::create<Mapper068>(*m_nesCartridgeData);
        case 69: return BaseMapper::create<Mapper069>(*m_nesCartridgeData);
        case 70: return BaseMapper::create<Mapper070>(*m_nesCartridgeData);
        case 71: return BaseMapper::create<Mapper071>(*m_nesCartridgeData);
        case 72: return BaseMapper::create<Mapper072>(*m_nesCartridgeData);
        case 74: return BaseMapper::create<Mapper074>(*m_nesCartridgeData);
        case 75: return BaseMapper::create<Mapper075>(*m_nesCartridgeData);
        case 76: return BaseMapper::create<Mapper076>(*m_nesCartridgeData);
        case 78: return BaseMapper::create<Mapper078>(*m_nesCartridgeData);
        case 79: return BaseMapper::create<Mapper079>(*m_nesCartridgeData);
        case 80: return BaseMapper::create<Mapper080>(*m_nesCartridgeData);
        case 82: return BaseMapper::create<Mapper082>(*m_nesCartridgeData);
        case 87: return BaseMapper::create<Mapper087>(*m_nesCartridgeData);
        case 88: return BaseMapper::create<Mapper088>(*m_nesCartridgeData);
        case 89: return BaseMapper::create<Mapper089>(*m_nesCartridgeData);
        case 90: return BaseMapper::create<Mapper090>(*m_nesCartridgeData);
        case 91: return BaseMapper::create<Mapper091>(*m_nesCartridgeData);
        case 92: return BaseMapper::create<Mapper092>(*m_nesCartridgeData);
        case 93: return BaseMapper::create<Mapper093>(*m_nesCartridgeData);
        case 94: return BaseMapper::create<Mapper094>(*m_nesCartridgeData);
        case 95: return BaseMapper::create<Mapper095>(*m_nesCartridgeData);
        case 97: return BaseMapper::create<Mapper097>(*m_nesCartridgeData);
        case 99: return BaseMapper::create<Mapper099>(*m_nesCartridgeData);
        case 101: return BaseMapper::create<Mapper101>(*m_nesCartridgeData);
        case 113: return BaseMapper::create<Mapper113>(*m_nesCartridgeData);
        case 118: return BaseMapper::create<Mapper118>(*m_nesCartridgeData);
        case 119: return BaseMapper::create<Mapper119>(*m_nesCartridgeData);
        case 152: return BaseMapper::create<Mapper152>(*m_nesCartridgeData);
        case 154: return BaseMapper::create<Mapper154>(*m_nesCartridgeData);
        case 180: return BaseMapper::create<Mapper180>(*m_nesCartridgeData);
        case 185: return BaseMapper::create<Mapper185>(*m_nesCartridgeData);
        case 206: return BaseMapper::create<Mapper206>(*m_nesCartridgeData);
        case 210: return BaseMapper::create<Mapper210>(*m_nesCartridgeData);
        case 232: return BaseMapper::create<Mapper232>(*m_nesCartridgeData);
        case 245: return BaseMapper::create<Mapper245>(*m_nesCartridgeData);
#ifdef ENABLE_NSF_PLAYER
        case _NsfFormat::NSF_MAPPER_ID: return BaseMapper::create<MapperNSF>(*m_nesCartridgeData);
#endif

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

        // Try iNES first, then FDS, then NSF.
        _INesFormat* iNes = new _INesFormat(m_romFile);
        if(iNes->valid()) {
            m_nesCartridgeData = iNes;
        }
        else {
            const bool iNesSizeMismatch = (iNes->error() == "file length does not match header information");
            delete iNes;
            iNes = nullptr;

            _FdsFormat* fds = new _FdsFormat(m_romFile);
            if(fds->valid()) {
                m_nesCartridgeData = fds;
            }
            else {
                const std::string fdsError = fds->error();
                delete fds;
                fds = nullptr;

                if(fs::path(filename).extension() == ".fds") {
                    clear();
                    Logger::instance().log(
                        fdsError.empty() ? "Invalid FDS image" : fdsError,
                        Logger::Type::ERROR
                    );
                    return false;
                }

#ifdef ENABLE_NSF_PLAYER
                _NsfFormat* nsf = new _NsfFormat(m_romFile);
                if(nsf->valid()) {
                    m_nesCartridgeData = nsf;
                }
                else {
                    delete nsf;
                    nsf = nullptr;
                    clear();
                    if(iNesSizeMismatch) {
                        Logger::instance().log("ROM file size/header mismatch detected (iNES). Aborting load.", Logger::Type::ERROR);
                    }
                    else {
                        Logger::instance().log("Invalid ROM", Logger::Type::USER);
                    }
                    return false;
                }
#else
                clear();
                if(iNesSizeMismatch) {
                    Logger::instance().log("ROM file size/header mismatch detected (iNES). Aborting load.", Logger::Type::ERROR);
                }
                else {
                    Logger::instance().log("Invalid ROM", Logger::Type::USER);
                }
                return false;
#endif
            }
        }

        uint32_t prgCrc = m_nesCartridgeData->prgCrc32();
        uint32_t prgChrCrc = m_nesCartridgeData->prgChrCrc32();

        std::string prgCrcStr = Crc32::toString(prgCrc);
        std::string prgChrCrcStr = Crc32::toString(prgChrCrc);

        // NSF is not an iNES cartridge dump, so DB header overwrite doesn't apply.
        if(
#ifdef ENABLE_NSF_PLAYER
           dynamic_cast<_NsfFormat*>(m_nesCartridgeData) == nullptr &&
#endif
           dynamic_cast<_FdsFormat*>(m_nesCartridgeData) == nullptr) {
            GameDatabase::Item* item = GameDatabase::instance().findByCrc(prgChrCrcStr);

            if(item != nullptr) {
                Logger::instance().log("ROM found in database\nUsing DB header", Logger::Type::INFO);
                m_nesCartridgeData = new DbOverwriteCartridgeData(m_nesCartridgeData, item);
                m_nesCartridgeData->log("(DB)");
            }
            else {
                Logger::instance().log("ROM not found in database\nUsing default header", Logger::Type::INFO);
            }
        }
        else {
            if(dynamic_cast<_FdsFormat*>(m_nesCartridgeData) != nullptr) {
                Logger::instance().log("FDS file detected\nUsing FDS mapper", Logger::Type::INFO);
            }
#ifdef ENABLE_NSF_PLAYER
            else {
                Logger::instance().log("NSF file detected\nUsing NSF player mapper", Logger::Type::INFO);
            }
#endif
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

        m_isValid = true;

        return true;
    }

    void reset()
    {
        if(m_mapper != nullptr) {
            m_mapper->reset();
        }
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

    GERANES_INLINE void writeMapperRegister(int addr, uint8_t data)
    {
        m_mapper->writeMapperRegister(addr,data);
    }

    GERANES_INLINE uint8_t readMapperRegister(int addr, uint8_t openBusData)
    {
        return m_mapper->readMapperRegister(addr,openBusData);
    }

    GERANES_INLINE void writeMapperRegisterAbsolute(uint16_t addr, uint8_t data)
    {
        m_mapper->writeMapperRegisterAbsolute(addr, data);
    }

    GERANES_INLINE uint8_t readMapperRegisterAbsolute(uint16_t addr, uint8_t openBusData)
    {
        return m_mapper->readMapperRegisterAbsolute(addr, openBusData);
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

    GERANES_INLINE_HOT void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data)
    {
        m_mapper->writeCustomNameTable(index,addr,data);
    }

    GERANES_INLINE void onScanlineStart(bool renderingEnabled, int scanline)
    {
        m_mapper->onScanlineStart(renderingEnabled, scanline);
    }

    GERANES_INLINE void setPpuFetchSource(bool isSpriteFetch)
    {
        m_mapper->setPpuFetchSource(isSpriteFetch);
    }

    GERANES_INLINE uint8_t transformNameTableRead(uint8_t index, uint16_t addr, uint8_t value)
    {
        return m_mapper->transformNameTableRead(index, addr, value);
    }

    GERANES_INLINE void setSpriteSize8x16(bool sprite8x16)
    {
        m_mapper->setSpriteSize8x16(sprite8x16);
    }

    GERANES_INLINE void setPpuMask(uint8_t mask)
    {
        m_mapper->setPpuMask(mask);
    }

    GERANES_INLINE void onPpuStatusRead(bool vblankSet)
    {
        m_mapper->onPpuStatusRead(vblankSet);
    }

    GERANES_INLINE void onPpuRead(uint16_t addr)
    {
        m_mapper->onPpuRead(addr);
    }

    GERANES_INLINE void onPpuCycle(int scanline, int cycle, bool isRendering, bool isPreLine)
    {
        m_mapper->onPpuCycle(scanline, cycle, isRendering, isPreLine);
    }

    GERANES_INLINE void onCpuRead(uint16_t addr)
    {
        m_mapper->onCpuRead(addr);
    }

    GERANES_INLINE void onCpuWrite(uint16_t addr, uint8_t data)
    {
        m_mapper->onCpuWrite(addr, data);
    }

#ifdef ENABLE_NSF_PLAYER
    GERANES_INLINE bool consumeNsfPlayerInstructionRedirect(uint16_t& cpuAddr)
    {
        return m_mapper->consumeNsfPlayerInstructionRedirect(cpuAddr);
    }

    GERANES_INLINE void preloadNsfMemory(uint8_t* cpuRam, size_t cpuRamSize)
    {
        m_mapper->preloadNsfMemory(cpuRam, cpuRamSize);
    }
#endif

    GERANES_INLINE float getExpansionAudioSample()
    {
        return m_mapper->getExpansionAudioSample();
    }

    GERANES_INLINE float getMixWeight() const
    {
        return m_mapper->getMixWeight();
    }

    GERANES_INLINE float getExpansionOutputGain() const
    {
        return m_mapper->getExpansionOutputGain();
    }

    GERANES_INLINE std::string getMapperAudioChannelsJson() const
    {
        return m_mapper->getAudioChannelsJson();
    }

    GERANES_INLINE bool setMapperAudioChannelVolumeById(const std::string& id, float volume)
    {
        return m_mapper->setAudioChannelVolumeById(id, volume);
    }

    GERANES_INLINE void applyExternalActions(uint8_t pending)
    {
        m_mapper->applyExternalActions(pending);
    }

    GERANES_INLINE GameDatabase::System system()
    {
        return m_nesCartridgeData->sistem();
    }

    GERANES_INLINE GameDatabase::InputType inputType()
    {
        return m_nesCartridgeData->inputType();
    }

    GERANES_INLINE GameDatabase::PpuModel vsPpuModel()
    {
        return m_nesCartridgeData->vsPpuModel();
    }

    GERANES_INLINE bool isValid()
    {
        return m_isValid;
    }

    GERANES_INLINE bool hasCartridgeData() const
    {
        return m_nesCartridgeData != NULL;
    }

    GERANES_INLINE int mapperId() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->mapperId();
    }

    GERANES_INLINE int subMapperId() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->subMapperId();
    }

    GERANES_INLINE int prgSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->prgSize();
    }

    GERANES_INLINE int chrSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->chrSize();
    }

    GERANES_INLINE int chrRamSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->chrRamSize();
    }

    GERANES_INLINE int ramSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->ramSize();
    }

    GERANES_INLINE int dbSaveRamSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->saveRamSize();
    }

    GERANES_INLINE bool hasBattery() const
    {
        if(m_nesCartridgeData == NULL) return false;
        return m_nesCartridgeData->hasBattery();
    }

    GERANES_INLINE std::string chip() const
    {
        if(m_nesCartridgeData == NULL) return "";
        return m_nesCartridgeData->chip();
    }

    GERANES_INLINE std::string prgChrCrc32String() const
    {
        if(m_nesCartridgeData == NULL) return "";
        return Crc32::toString(m_nesCartridgeData->prgChrCrc32());
    }

    GERANES_INLINE uint8_t* saveRamData()
    {
        return m_mapper->saveRamData();
    }

    GERANES_INLINE size_t saveRamSize() const
    {
        return m_mapper->saveRamSize();
    }

    GERANES_INLINE bool hasBatterySaveRam() const
    {
        return m_mapper->hasBatterySaveRam();
    }

    GERANES_INLINE const RomFile& romFile() {
        return m_romFile;
    }

    GERANES_INLINE bool isNsf() const
    {
#ifdef ENABLE_NSF_PLAYER
        return m_nesCartridgeData != NULL && m_nesCartridgeData->mapperId() == _NsfFormat::NSF_MAPPER_ID;
#else
        return false;
#endif
    }

    GERANES_INLINE int nsfTotalSongs() const
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return 0;
        const auto* mapper = dynamic_cast<const MapperNSF*>(m_mapper);
        return mapper != nullptr ? mapper->totalSongs() : 0;
#else
        return 0;
#endif
    }

    GERANES_INLINE int nsfCurrentSong() const
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return 0;
        const auto* mapper = dynamic_cast<const MapperNSF*>(m_mapper);
        return mapper != nullptr ? mapper->currentSong() : 0;
#else
        return 0;
#endif
    }

    GERANES_INLINE bool nsfIsPlaying() const
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        const auto* mapper = dynamic_cast<const MapperNSF*>(m_mapper);
        return mapper != nullptr ? mapper->isPlaying() : false;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfSetPlaying(bool playing)
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->setPlaying(playing);
        return true;
#else
        (void)playing;
        return false;
#endif
    }

    GERANES_INLINE bool nsfSetSong(int song1Based)
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->setSong(song1Based);
        return true;
#else
        (void)song1Based;
        return false;
#endif
    }

    GERANES_INLINE bool nsfNextSong()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->nextSong();
        return true;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfPrevSong()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->prevSong();
        return true;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfRequestSongInit()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->requestSongInit();
        return true;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfSongInitPending()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        return mapper->songInitPending();
#else
        return false;
#endif
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_isValid);
        m_mapper->serialization(s);
    }

};




