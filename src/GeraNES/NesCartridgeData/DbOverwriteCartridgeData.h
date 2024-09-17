#ifndef DB_OVERWRITE_CARTRIDGE_DATA_H
#define DB_OVERWRITE_CARTRIDGE_DATA_H

#include "ICartridgeData.h"
#include "GeraNES/Db.h"

class DbOverwriteCartridgeData : public ICartridgeData {

private:

    ICartridgeData* m_src;
    Db::Data* m_dbData;

    int m_prgSize;
    int m_chrSize;

    int m_ramSize;

    bool m_hasBattery;
    int m_saveRamSize;

    int m_chrRamSize;

    bool m_useFourScreenMirroring;

    MirroringType m_mirroringType;
    int m_mapperId;

public:

    DbOverwriteCartridgeData(ICartridgeData* src, Db::Data* dbData) : m_src(src), m_dbData(dbData), ICartridgeData(src->romFile()) {
               
        m_prgSize = m_dbData->PrgRomSize > 0 ? m_dbData->PrgRomSize*1024 : m_src->prgSize();
        m_chrSize = m_dbData->ChrRomSize > 0 ? m_dbData->ChrRomSize*1024 : m_src->chrSize();
        m_ramSize = m_dbData->WorkRamSize > 0 ? m_dbData->WorkRamSize*1024 : m_src->ramSize();
        m_chrRamSize = m_dbData->ChrRamSize > 0 ? m_dbData->ChrRamSize*1024 : m_src->chrRamSize();

        m_saveRamSize = m_dbData->SaveRamSize > 0 ? m_dbData->SaveRamSize*1024 : m_src->saveRamSize();

        switch(m_dbData->HasBattery) {
            case Db::Battery::Yes: m_hasBattery = true; break;
            case Db::Battery::No: m_hasBattery = false; break;
            case Db::Battery::Default: m_hasBattery = m_src->hasBattery(); break;            
        }

        m_useFourScreenMirroring = m_src->useFourScreenMirroring();

        switch(m_dbData->Mirroring) {
            case Db::MirroringType::HORIZONTAL: m_mirroringType = MirroringType::HORIZONTAL; break;
            case Db::MirroringType::VERTICAL: m_mirroringType = MirroringType::VERTICAL; break;
            case Db::MirroringType::FOUR_SCREEN:
                m_mirroringType = MirroringType::FOUR_SCREEN;
                m_useFourScreenMirroring = true;
                break;
            case Db::MirroringType::SINGLE_SCREEN_A: m_mirroringType = MirroringType::SINGLE_SCREEN_A; break;
            case Db::MirroringType::SINGLE_SCREEN_B: m_mirroringType = MirroringType::SINGLE_SCREEN_B; break;
            case Db::MirroringType::DEFAULT: m_mirroringType = m_mirroringType = m_src->mirroringType(); break;
        }

        if(m_dbData->MapperId >=0) m_mapperId = m_dbData->MapperId;
        else  m_mapperId = m_src->mapperId();
    }

    virtual ~DbOverwriteCartridgeData() {
        delete m_src;
    }

    int mapperId() override {
        return m_mapperId;    
    }

    bool valid() override {
        return true;
    }

    int prgSize() override
    {
        return m_prgSize;
    }

    int chrSize() override
    {
        return m_chrSize;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_mirroringType;
    }

    bool hasBattery() override
    {
        return m_hasBattery;
    }

    int saveRamSize() override {
        return m_saveRamSize;
    }

    int chrRamSize() override {
        return m_chrRamSize;
    }

    bool hasTrainer() override
    {
        return m_src->hasTrainer();
    }

    GERANES_HOT bool useFourScreenMirroring() override
    {
        return m_useFourScreenMirroring;
    }

    int ramSize() override
    {
        return m_ramSize;
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
