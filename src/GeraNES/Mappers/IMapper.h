#ifndef INCLUDE_IMAPPER
#define INCLUDE_IMAPPER

#include <memory>
#include <fstream>

#include "../defines.h"
#include "../NesCartridgeData/ICartridgeData.h"

#include "../Serialization.h"

#if __GNUC__
    #if __GNUC__ >= 8 || defined(__EMSCRIPTEN__)
        #include <filesystem>
        namespace fs = std::filesystem;
    #else
        #include <experimental/filesystem>
        namespace fs = std::experimental::filesystem;
    #endif
#else
    #include <filesystem>
    namespace fs = std::filesystem;
#endif


class IMapper
{

protected:

    ICartridgeData& m_cd;

private:

    uint8_t* m_sRam = nullptr;

    uint8_t* m_vRam = nullptr;

    std::string sramFile() {
        auto romFile = m_cd.romFile();
        return std::string(SRAM_FOLDER) + basename(romFile.fileName()) + ".sram";
    }

    void loadSRAM()
    {
        if(!m_cd.hasBattery()) return;

        std::ifstream f(sramFile(), std::ios::binary);

        if(f.is_open()) {

            std::streampos begin,end;
            begin = f.tellg();
            f.seekg (0, std::ios::end);
            end = f.tellg();
            f.seekg (0, std::ios::beg);

            size_t size = end-begin;

            if(size == m_cd.saveRamSize())
                f.read(reinterpret_cast<char*>(m_sRam), m_cd.saveRamSize());

            f.close();


        }

    }

    void saveSRAM()
    {
        if(!m_cd.hasBattery()) return;

        std::string dir = fs::path(sramFile()).parent_path().string();
        if(!fs::exists(dir)) fs::create_directory(dir);

        std::ofstream f(sramFile(), std::ios::binary | std::ios::trunc);
        if(f.is_open()) {
            f.write(reinterpret_cast<char*>(m_sRam), m_cd.saveRamSize());
            f.close();
        }
    }

public:    

    //window size
    enum { W1K = 0x400, W2K = 0x800, W4K = 0x1000, W8K = 0x2000, W16K = 0x4000, W32K = 0x8000 };

    IMapper(ICartridgeData& cd) : m_cd(cd)
    {               
    }

    //we cant call virtual methods from constructor, so we need an init method
    void init() {

        if(m_cd.saveRamSize() > 0) {
            m_sRam = new uint8_t[m_cd.saveRamSize()];
            memset(m_sRam, 0, m_cd.saveRamSize());
            loadSRAM();
        }

        if(m_cd.ramSize() > 0) {
            m_vRam = new uint8_t[m_cd.ramSize()];
            memset(m_vRam, 0, m_cd.ramSize());
        }
    }

    uint8_t* getVRAM() {
        return m_vRam;
    }

    virtual void reset(){}
    virtual void writePRG32k(int /*addr*/, uint8_t /*data*/) {}
    virtual uint8_t readPRG32k(int /*addr*/) { return 0; }

    virtual void writeCHR8k(int addr, uint8_t data) {
        if(m_vRam != nullptr)  m_vRam[addr&(m_cd.ramSize()-1)] = data;
    }

    virtual uint8_t readCHR8k(int addr) {
        if(m_vRam != nullptr) return m_vRam[addr&(m_cd.ramSize()-1)];
        return 0;
    }

    virtual void write0x4000(int /*addr*/, uint8_t /*data*/) {}
    virtual uint8_t read0x4000(int /*addr*/, uint8_t openBusData) { return openBusData; }

    virtual void tick(){}
    virtual void cycle(){ } //cpu cycle


    virtual bool getInterruptFlag(){ return false; }

    virtual bool useCustomNameTable(uint8_t index) { return false; }
    virtual uint8_t readCustomNameTable(uint8_t index, uint16_t addr) { return 0; }

    virtual void writeSRAM8k(int addr, uint8_t data)
    {
        if(m_sRam != nullptr)
            m_sRam[addr&(m_cd.saveRamSize()-1)] = data;
    }

    virtual uint8_t readSRAM8k(int addr)
    {
        if(m_sRam != nullptr)
            return m_sRam[addr&(m_cd.saveRamSize()-1)];
    }

    virtual MirroringType mirroringType()
    {
        if(m_cd.useFourScreenMirroring() ) return MirroringType::FOUR_SCREEN;
        else {
            return m_cd.mirroringType();
        }
    }

    virtual uint8_t customMirroring(uint8_t /*blockIndex*/)
    {
        return 0;
    }

    virtual ~IMapper()
    {
        saveSRAM();

        if(m_vRam != nullptr) delete[] m_vRam;
        if(m_sRam != nullptr) delete[] m_sRam;
    }

    //helper function to generate bit mask
    uint8_t calculateMask(int nBanks)
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

    virtual void serialization(SerializationBase& s)
    {
        s.array(m_sRam, 1, m_cd.saveRamSize());

        bool hasVRAM = (m_vRam != NULL);
        SERIALIZEDATA(s, hasVRAM);
        if(hasVRAM) {
            s.array(m_vRam, 1, m_cd.ramSize());
        }
    }

    GERANES_INLINE bool hasVRAM() {
        return m_vRam != nullptr;
    }



};

#endif
