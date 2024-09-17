#ifndef DB_OVERWRITE_CARTRIDGE_DATA_H
#define DB_OVERWRITE_CARTRIDGE_DATA_H

#include "ICartridgeData.h"
#include "GeraNES/Db.h"

class DbOverwriteCartridgeData : public ICartridgeData {

private:

    ICartridgeData* m_src;
    Db::Data* m_dbData;

public:

    DbOverwriteCartridgeData(ICartridgeData* src, Db::Data* dbData) : m_src(src), m_dbData(dbData), ICartridgeData(src->romFile()) {
    }

    virtual ~DbOverwriteCartridgeData() {
        delete m_src;
        delete m_dbData;
    }

    int mapperNumber() override
    {
        if(m_dbData->Mapper != "") return std::stoi(m_dbData->Mapper);
        return m_src->mapperNumber();
    }

    bool valid() override {
        return true;
    }

    int numberOfPrg16kBanks() override
    {
        return m_src->numberOfPrg16kBanks();
    }

    int numberOfChr8kBanks() override
    {
        return m_src->numberOfChr8kBanks();
    }

    GERANES_HOT int mirroringType() override
    {
        return m_src->mirroringType();
    }

    bool hasBatteryRam8k() override
    {
        return m_src->hasBatteryRam8k();
    }

    bool hasTrainer() override
    {
        return m_src->hasTrainer();
    }

    GERANES_HOT bool useFourScreenMirroring() override
    {
        return m_src->useFourScreenMirroring();
    }

    int numberOfRamBanks() override
    {
        return m_src->numberOfRamBanks();
    }

    GERANES_HOT uint8_t readTrainer(int addr) override
    {
        return m_src->readTrainer(addr);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return m_src->readPrg(addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return m_src->readChr(addr);
    }

};

#endif
