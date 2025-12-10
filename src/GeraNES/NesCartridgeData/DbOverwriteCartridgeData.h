#pragma once

#include "ICartridgeData.h"
#include "GeraNES/GameDatabase.h"

class DbOverwriteCartridgeData : public ICartridgeData {

private:

    ICartridgeData* m_src;
    GameDatabase::Item* m_item;

    int m_prgSize;
    int m_chrSize;

    int m_ramSize;

    bool m_hasBattery;
    int m_saveRamSize;

    int m_chrRamSize;

    bool m_useFourScreenMirroring;

    MirroringType m_mirroringType;
    int m_mapperId;

    int m_subMapperId;

    std::string m_chip;

    GameDatabase::Sistem m_system;

    GameDatabase::InputType m_inputType;

public:

    DbOverwriteCartridgeData(ICartridgeData* src, GameDatabase::Item* item) : m_src(src), m_item(item), ICartridgeData(src->romFile()) {
               
        m_prgSize = m_item->PrgRomSize > 0 ? m_item->PrgRomSize*1024 : m_src->prgSize();
        m_chrSize = m_item->ChrRomSize > 0 ? m_item->ChrRomSize*1024 : m_src->chrSize();
        m_ramSize = m_item->WorkRamSize > 0 ? m_item->WorkRamSize*1024 : m_src->ramSize();
        m_chrRamSize = m_item->ChrRamSize > 0 ? m_item->ChrRamSize*1024 : m_src->chrRamSize();
        m_saveRamSize = m_item->SaveRamSize > 0 ? m_item->SaveRamSize*1024 : m_src->saveRamSize();

        switch(m_item->HasBattery) {
            case GameDatabase::Battery::Yes: m_hasBattery = true; break;
            case GameDatabase::Battery::No: m_hasBattery = false; break;
            case GameDatabase::Battery::Default: m_hasBattery = m_src->hasBattery(); break;            
        }

        m_useFourScreenMirroring = m_src->useFourScreenMirroring();

        switch(m_item->Mirroring) {
            case GameDatabase::MirroringType::HORIZONTAL:
                m_mirroringType = MirroringType::HORIZONTAL;
                break;
            case GameDatabase::MirroringType::VERTICAL:
                m_mirroringType = MirroringType::VERTICAL;
                break;
            case GameDatabase::MirroringType::FOUR_SCREEN:
                m_mirroringType = MirroringType::FOUR_SCREEN;
                m_useFourScreenMirroring = true;
                break;
            case GameDatabase::MirroringType::SINGLE_SCREEN_A:
                m_mirroringType = MirroringType::SINGLE_SCREEN_A;
                break;
            case GameDatabase::MirroringType::SINGLE_SCREEN_B:
                m_mirroringType = MirroringType::SINGLE_SCREEN_B;
                break;
            case GameDatabase::MirroringType::DEFAULT:
                m_mirroringType = m_src->mirroringType();
                break;
        }

        m_mapperId = m_item->MapperId >=0 ? m_item->MapperId : m_src->mapperId();

        m_subMapperId = m_item->SubmapperId >=0 ? m_item->SubmapperId : m_src->subMapperId();

        m_chip = !m_item->Chip.empty() ? m_item->Chip : m_src->chip();

        m_system = m_item->System != GameDatabase::Sistem::Unknown ? m_item->System : m_src->sistem();

        m_inputType = m_item->InputType != GameDatabase::InputType::Unspecified ? m_item->InputType : m_src->inputType();
    }

    virtual ~DbOverwriteCartridgeData() {
        delete m_src;
    }

    int mapperId() override {
        return m_mapperId;    
    }

    int subMapperId() override {
        return m_subMapperId;    
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

    std::string chip() override {
        return m_chip;
    }

    GameDatabase::Sistem sistem() override {
        return m_system;
    }

    GameDatabase::InputType inputType() override {
        return m_inputType;
    }

};
