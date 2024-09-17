#ifndef DB_OVERWRITE_CARTRIDGE_DATA_H
#define DB_OVERWRITE_CARTRIDGE_DATA_H

#include "ICartridgeData.h"
#include "GeraNES/Db.h"

class DbOverwriteCartridgeData : public ICartridgeData {

private:

    ICartridgeData* m_src;
    Db::Data* m_dbData;

    MirroringType m_mirroringType;
    int m_mapper;

public:

    DbOverwriteCartridgeData(ICartridgeData* src, Db::Data* dbData) : m_src(src), m_dbData(dbData), ICartridgeData(src->romFile()) {
               
        switch(m_dbData->Mirroring) {
            case Db::MirroringType::HORIZONTAL: m_mirroringType = MirroringType::HORIZONTAL; break;
            case Db::MirroringType::VERTICAL: m_mirroringType = MirroringType::VERTICAL; break;
            case Db::MirroringType::FOUR_SCREEN: m_mirroringType = MirroringType::FOUR_SCREEN; break;
            case Db::MirroringType::SINGLE_SCREEN_A: m_mirroringType = MirroringType::SINGLE_SCREEN_A; break;
            case Db::MirroringType::SINGLE_SCREEN_B: m_mirroringType = MirroringType::SINGLE_SCREEN_B; break;
            case Db::MirroringType::DEFAULT: m_mirroringType = m_mirroringType = m_src->mirroringType(); break;
        }

        if(m_dbData->Mapper >=0) m_mapper = m_dbData->Mapper;
        else  m_mapper = m_src->mapperNumber();
    }

    virtual ~DbOverwriteCartridgeData() {
        delete m_src;
    }

    int mapperNumber() override {
        return m_mapper;    
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

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_mirroringType;
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
