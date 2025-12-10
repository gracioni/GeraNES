#pragma once

#include "ICartridgeData.h"

class DummyCartridgeData : public ICartridgeData
{
private:

    DummyCartridgeData() : ICartridgeData(RomFile::emptyRomFile()) {
    }

public:

    bool valid() override {
        return true;
    }

    int prgSize() override
    {
        return 0;
    }

    int chrSize() override
    {
        return 0;
    }

    MirroringType mirroringType() override
    {
        return MirroringType::HORIZONTAL;
    }

    bool hasBattery() override
    {
        return false;
    }

    bool hasTrainer() override
    {
        return false;
    }

    bool useFourScreenMirroring() override
    {
        return true;
    }

    int mapperId() override
    {
        return 0;
    }

    int subMapperId() override
    {
        return 0;
    }

    int ramSize() override
    {
        return 0;
    }

    uint8_t readTrainer(int /*addr*/) override
    {
        return 0;
    }

    uint8_t readPrg(int /*addr*/) override
    {
        return 0;
    }

    uint8_t readChr(int /*addr*/) override
    {
        return 0;
    }

    int saveRamSize() override {
        return 0;
    }

    int chrRamSize() override {
        return 0;
    }

    std::string chip() override {
        return "";
    }

    GameDatabase::Sistem sistem() override {
        return GameDatabase::Sistem::NesNtsc;
    }

    GameDatabase::InputType inputType() override {
        return GameDatabase::InputType::Unspecified;
    }
    
    static DummyCartridgeData& instance()
    {
        static DummyCartridgeData ret;
        return ret;
    }

};
