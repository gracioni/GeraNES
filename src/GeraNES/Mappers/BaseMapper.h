#pragma once

#include <memory>
#include <fstream>
#include <string>

#include "../defines.h"
#include "../NesCartridgeData/ICartridgeData.h"

#include "../Serialization.h"

#include <filesystem>
namespace fs = std::filesystem;

#include "GeraNES/util/bank.h"

class BaseMapper
{
protected:    

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrRam(int bank, int addr) {
        return m_chrRam[(bank << log2(bs)) + (addr&(static_cast<int>(bs)-1))];
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrRam(int bank, int addr, uint8_t data) {
        m_chrRam[(bank << log2(bs)) + (addr&(static_cast<int>(bs)-1))] = data;
    }

    //helper function to generate bit mask
    static uint8_t calculateMask(int nBanks)
    {
        uint8_t mask = 0;

        int n = nBanks - 1;

        while(n > 0) {
            mask <<= 1;
            mask |= 1;
            n >>= 1;
        }

        return mask;
    }

    ICartridgeData& cd() const {
        return m_cd;
    }

    uint8_t* chrRam() {
        return m_chrRam;
    }

private:

    ICartridgeData& m_cd;
    uint8_t* m_chrRam = nullptr;
    int m_chrRamSize = 0;
    uint8_t* m_sRam = nullptr;    

    std::string saveRamFile() {
        auto romFile = cd().romFile();
        return std::string(SRAM_FOLDER) + basename(romFile.fileName()) + ".sram";
    }

    void loadSaveRamFromFile()
    {
        if(!cd().hasBattery()) return;

        std::ifstream f(saveRamFile(), std::ios::binary);

        if(f.is_open()) {

            std::streampos begin,end;
            begin = f.tellg();
            f.seekg (0, std::ios::end);
            end = f.tellg();
            f.seekg (0, std::ios::beg);

            size_t size = end-begin;

            if(size == cd().saveRamSize())
                f.read(reinterpret_cast<char*>(m_sRam), cd().saveRamSize());

            f.close();
        }
    }

    void writeSaveRamToFile()
    {
        if(!cd().hasBattery()) return;

        std::string dir = fs::path(saveRamFile()).parent_path().string();
        if(!fs::exists(dir)) fs::create_directory(dir);

        std::ofstream f(saveRamFile(), std::ios::binary | std::ios::trunc);
        if(f.is_open()) {
            f.write(reinterpret_cast<char*>(m_sRam), cd().saveRamSize());
            f.close();
        }
    }

    // we cant call virtual methods from constructor, so we need an init method
    void init() {

        if(cd().saveRamSize() > 0) {
            m_sRam = new uint8_t[cd().saveRamSize()];
            memset(m_sRam, 0, cd().saveRamSize());
            loadSaveRamFromFile();
        }

        if(cd().chrRamSize() > 0) allocateChrRam(cd().chrRamSize());
    }

protected:

    BaseMapper(ICartridgeData& cd) : m_cd(cd)
    {               
    }

    void allocateChrRam(int size)
    {
        if(size <= 0 || m_chrRam != nullptr) return;
        m_chrRam = new uint8_t[size];
        m_chrRamSize = size;
        memset(m_chrRam, 0, size);
    }
    
public:

    template<std::derived_from<BaseMapper> T>
    static T* create(ICartridgeData& cd) {
        auto ret = new T(cd);
        ret->init();
        return ret;
    }    

    virtual void reset(){}

    virtual void writePrg(int /*addr*/, uint8_t /*data*/) {}
    
    virtual uint8_t readPrg(int /*addr*/) { return 0; }

    virtual void writeChr(int addr, uint8_t data) {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(0, addr, data);
    }

    virtual uint8_t readChr(int addr) {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(0, addr);
        return 0;
    }

    virtual void writeMapperRegister(int /*addr*/, uint8_t /*data*/) {}

    virtual uint8_t readMapperRegister(int /*addr*/, uint8_t openBusData) { return openBusData; }

    virtual void setA12State(bool /*state*/){}

    virtual void cycle(){ } //cpu cycle

    virtual bool getInterruptFlag(){ return false; }

    virtual bool useCustomNameTable(uint8_t index) { return false; }

    virtual uint8_t readCustomNameTable(uint8_t index, uint16_t addr) { return 0; }

    virtual void writeCustomNameTable(uint8_t /*index*/, uint16_t /*addr*/, uint8_t /*data*/) {}

    virtual void writeSaveRam(int addr, uint8_t data)
    {
        if(m_sRam != nullptr)
            m_sRam[addr&(cd().saveRamSize()-1)] = data;
    }

    virtual uint8_t readSaveRam(int addr)
    {
        if(m_sRam != nullptr)
            return m_sRam[addr&(cd().saveRamSize()-1)];

        return 0;
    }

    virtual MirroringType mirroringType()
    {
        if(cd().useFourScreenMirroring() ) return MirroringType::FOUR_SCREEN;
        else {
            return cd().mirroringType();
        }
    }

    virtual uint8_t customMirroring(uint8_t /*blockIndex*/)
    {
        return 0;
    }

    virtual void onScanlineStart(bool /*renderingEnabled*/)
    {
    }
    virtual void onScanlineStart(bool renderingEnabled, int /*scanline*/)
    {
        onScanlineStart(renderingEnabled);
    }

    virtual void setPpuFetchSource(bool /*isSpriteFetch*/)
    {
    }

    virtual uint8_t transformNameTableRead(uint8_t /*index*/, uint16_t /*addr*/, uint8_t value)
    {
        return value;
    }

    virtual void setSpriteSize8x16(bool /*sprite8x16*/)
    {
    }

    virtual void setPpuMask(uint8_t /*mask*/)
    {
    }

    virtual void onPpuStatusRead(bool /*vblankSet*/)
    {
    }

    virtual void onPpuRead(uint16_t /*addr*/)
    {
    }

    virtual void onPpuCycle(int /*scanline*/, int /*cycle*/, bool /*isRendering*/, bool /*isPreLine*/)
    {
    }

    virtual void onCpuRead(uint16_t /*addr*/)
    {
    }

    virtual void onCpuWrite(uint16_t /*addr*/, uint8_t /*data*/)
    {
    }

#ifdef ENABLE_NFS_PLAYER
    virtual bool consumeNsfPlayerInstructionRedirect(uint16_t& /*cpuAddr*/)
    {
        return false;
    }

    virtual void preloadNsfMemory(uint8_t* /*cpuRam*/, size_t /*cpuRamSize*/)
    {
    }
#endif

    virtual void applyExternalActions(uint8_t /*pending*/)
    {
    }

    virtual float getExpansionAudioSample()
    {
        return 0.0f;
    }

    virtual std::string getAudioChannelsJson() const
    {
        return "{\"channels\":[]}";
    }

    virtual bool setAudioChannelVolumeById(const std::string& /*id*/, float /*volume*/)
    {
        return false;
    }

    virtual ~BaseMapper()
    {
        writeSaveRamToFile();

        if(m_chrRam != nullptr) delete[] m_chrRam;
        if(m_sRam != nullptr) delete[] m_sRam;
    }    

    virtual void serialization(SerializationBase& s)
    {
        s.array(m_sRam, 1, cd().saveRamSize());

        bool hasChrRam = (m_chrRam != NULL);
        SERIALIZEDATA(s, hasChrRam);
        if(hasChrRam) {
            s.array(m_chrRam, 1, m_chrRamSize);
        }
    }

    GERANES_INLINE bool hasChrRam() const {
        return (m_chrRam != nullptr) || (cd().chrRamSize() > 0);
    }

    GERANES_INLINE uint8_t* saveRamData()
    {
        return m_sRam;
    }

    GERANES_INLINE size_t saveRamSize() const
    {
        if(m_sRam == nullptr) return 0;
        return static_cast<size_t>(cd().saveRamSize());
    }

    GERANES_INLINE bool hasBatterySaveRam() const
    {
        return m_sRam != nullptr && cd().hasBattery();
    }



};
