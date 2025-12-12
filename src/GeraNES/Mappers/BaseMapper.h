#pragma once

#include <memory>
#include <fstream>

#include "../defines.h"
#include "../NesCartridgeData/ICartridgeData.h"

#include "../Serialization.h"

#include <filesystem>
namespace fs = std::filesystem;

#include "GeraNES/util/bank.h"

class BaseMapper
{

protected:

    ICartridgeData& m_cd;

    template<int WindowSize>
    struct ChrShift { static constexpr int value = __builtin_ctz(WindowSize); };

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrRam(int bank, int addr) {
        return m_chrRam[(bank << log2(bs)) + (addr&(static_cast<int>(bs)-1))];
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrRam(int bank, int addr, uint8_t data) {
        m_chrRam[(bank << log2(bs)) + (addr&(static_cast<int>(bs)-1))] = data;
    }

private:

    uint8_t* m_sRam = nullptr;

    uint8_t* m_chrRam = nullptr;

    std::string saveRamFile() {
        auto romFile = m_cd.romFile();
        return std::string(SRAM_FOLDER) + basename(romFile.fileName()) + ".sram";
    }

    void loadSaveRamFromFile()
    {
        if(!m_cd.hasBattery()) return;

        std::ifstream f(saveRamFile(), std::ios::binary);

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

    void writeSaveRamToFile()
    {
        if(!m_cd.hasBattery()) return;

        std::string dir = fs::path(saveRamFile()).parent_path().string();
        if(!fs::exists(dir)) fs::create_directory(dir);

        std::ofstream f(saveRamFile(), std::ios::binary | std::ios::trunc);
        if(f.is_open()) {
            f.write(reinterpret_cast<char*>(m_sRam), m_cd.saveRamSize());
            f.close();
        }
    }

public:

    BaseMapper(ICartridgeData& cd) : m_cd(cd)
    {               
    }

    // we cant call virtual methods from constructor, so we need an init method
    void init() {

        if(m_cd.saveRamSize() > 0) {
            m_sRam = new uint8_t[m_cd.saveRamSize()];
            memset(m_sRam, 0, m_cd.saveRamSize());
            loadSaveRamFromFile();
        }

        if(m_cd.chrRamSize() > 0) {
            m_chrRam = new uint8_t[m_cd.chrRamSize()];
            memset(m_chrRam, 0, m_cd.chrRamSize());
        }
    }

    uint8_t* getChrRam() {
        return m_chrRam;
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

    virtual void writeSaveRam(int addr, uint8_t data)
    {
        if(m_sRam != nullptr)
            m_sRam[addr&(m_cd.saveRamSize()-1)] = data;
    }

    virtual uint8_t readSaveRam(int addr)
    {
        if(m_sRam != nullptr)
            return m_sRam[addr&(m_cd.saveRamSize()-1)];

        return 0;
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

    virtual ~BaseMapper()
    {
        writeSaveRamToFile();

        if(m_chrRam != nullptr) delete[] m_chrRam;
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

        bool hasChrRam = (m_chrRam != NULL);
        SERIALIZEDATA(s, hasChrRam);
        if(hasChrRam) {
            s.array(m_chrRam, 1, m_cd.chrRamSize());
        }
    }

    GERANES_INLINE bool hasChrRam() {
        return m_cd.chrRamSize() > 0;
    }



};
