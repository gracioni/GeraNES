#ifndef INCLUDE_IMAPPER
#define INCLUDE_IMAPPER

#include <memory>
#include <fstream>

#include "../defines.h"
#include "../NesCartridgeData/ICartridgeData.h"

#include "../Serialization.h"

#ifdef __GNUC__
    #if __GNUC__ >= 8
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

    ICartridgeData& m_cartridgeData;

private:

    uint8_t m_SRAM[0x2000]; //8k

    uint8_t* m_VRAM = NULL;

    std::string sramFile() {
        auto romFile = m_cartridgeData.romFile();
        return std::string(SRAM_FOLDER) + romFile.hash() + ".sram";
    }

    void loadSRAM()
    {
        if(!m_cartridgeData.hasBatteryRAM8k()) return;

        std::ifstream f(sramFile(), std::ios::binary);

        if(f.is_open()) {

            std::streampos begin,end;
            begin = f.tellg();
            f.seekg (0, std::ios::end);
            end = f.tellg();
            f.seekg (0, std::ios::beg);

            size_t size = end-begin;

            if(size == sizeof(m_SRAM))
                f.read(reinterpret_cast<char*>(&m_SRAM[0]),sizeof(m_SRAM));

            f.close();


        }

    }

    void saveSRAM()
    {
        if(!m_cartridgeData.hasBatteryRAM8k()) return;

        std::string dir = fs::path(sramFile()).parent_path().string();
        if(!fs::exists(dir)) fs::create_directory(dir);

        std::ofstream f(sramFile(), std::ios::binary | std::ios::trunc);
        if(f.is_open()) {
            f.write(reinterpret_cast<char*>(&m_SRAM[0]), sizeof(m_SRAM));
            f.close();
        }
    }

public:

    enum MirroringType { HORIZONTAL, VERTICAL, SINGLE_SCREEN_A, SINGLE_SCREEN_B, FOUR_SCREEN, CUSTOM };

    //window size
    enum { W1K = 0x400, W2K = 0x800, W4K = 0x1000, W8K = 0x2000, W16K = 0x4000, W32K = 0x8000 };

    IMapper(ICartridgeData& cd) : m_cartridgeData(cd)
    {
        if(m_cartridgeData.hasBatteryRAM8k()) {
            loadSRAM();
        }
        else memset(m_SRAM, 0, sizeof(m_SRAM));        
    }

    //we cant call virtual methods from constructor, so we need an init method
    void init() {
        if(m_cartridgeData.numberOfCHRBanks<W8K>() == 0 || VRAMRequired()) {
            m_VRAM = new uint8_t[VRAMSize()];
        }
    }

    virtual bool VRAMRequired() {
        return false;
    }

    virtual int VRAMSize() {
        return 0x2000; //default 8k
    }

    uint8_t* getVRAM() {
        return m_VRAM;
    }

    virtual void reset(void) {}
    virtual void writePRG32k(int /*addr*/, uint8_t /*data*/) {}
    virtual uint8_t readPRG32k(int /*addr*/) { return 0; }

    virtual void writeCHR8k(int addr, uint8_t data) {
        if(m_VRAM != nullptr)  m_VRAM[addr&(VRAMSize()-1)] = data;
    }

    virtual uint8_t readCHR8k(int addr) {
        if(m_VRAM != nullptr) return m_VRAM[addr&(VRAMSize()-1)];
        return 0;
    }

    virtual void write0x4000(int /*addr*/, uint8_t /*data*/) {}
    virtual uint8_t read0x4000(int /*addr*/, uint8_t openBusData) { return openBusData; }

    virtual void tick(void) {}
    virtual void cycle(void) { } //cpu cycle


    virtual bool getInterruptFlag(void) { return false; }

    virtual bool useCustomNameTable(uint8_t index) { return false; }
    virtual void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data) { }
    virtual uint8_t readCustomNameTable(uint8_t index, uint16_t addr) { return 0; }

    virtual void writeSRAM8k(int addr, uint8_t data)
    {
        m_SRAM[addr&0x1FFF] = data;
    }

    virtual uint8_t readSRAM8k(int addr)
    {
        return m_SRAM[addr&0x1FFF];
    }

    virtual MirroringType mirroringType(void)
    {
        if(m_cartridgeData.useFourScreenMirroring() ) return IMapper::FOUR_SCREEN;
        else
        {
            if(m_cartridgeData.mirroringType() == 0) return IMapper::HORIZONTAL;
            else return IMapper::VERTICAL;
        }
    }

    virtual uint8_t customMirroring(uint8_t /*blockIndex*/)
    {
        return 0;
    }

    virtual ~IMapper()
    {
        saveSRAM();

        if(m_VRAM != nullptr) delete[] m_VRAM;
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
        s.array(m_SRAM, 1, 0x2000);

        bool hasVRAM = (m_VRAM != NULL);
        SERIALIZEDATA(s, hasVRAM);
        if(hasVRAM) {
            s.array(m_VRAM, 1, VRAMSize());
        }
    }

    GERANES_INLINE bool has8kVRAM() {
        return m_VRAM != nullptr;
    }



};

#endif
