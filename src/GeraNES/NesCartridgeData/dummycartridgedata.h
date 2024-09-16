#ifndef DUMMYCARTRIDGEDATA_H
#define DUMMYCARTRIDGEDATA_H

#include "ICartridgeData.h"

class DummyCartridgeData : public ICartridgeData
{
private:

    DummyCartridgeData() : ICartridgeData(RomFile::emptyRomFile) {
    }

public:

    GERANES_INLINE std::string error() override
    {
        return "";
    }

    GERANES_INLINE int numberOfPRG16kBanks() override
    {
        return false;
    }

    GERANES_INLINE int numberOfCHR8kBanks() override
    {
        return false;
    }

    GERANES_INLINE int mirroringType() override
    {
        return 0;
    }

    bool hasBatteryRAM8k() override
    {
        return false;
    }

    GERANES_INLINE bool hasTrainer() override
    {
        return false;
    }

    GERANES_INLINE bool useFourScreenMirroring() override
    {
        return true;
    }

    GERANES_INLINE int mapperNumber() override
    {
        return 0;
    }

    GERANES_INLINE_HOT int numberOfRAMBanks() override
    {
        return 0;
    }

    GERANES_INLINE_HOT uint8_t readTrainer(int /*addr*/) override
    {
        return 0;
    }

    GERANES_INLINE_HOT uint8_t readPRG(int /*addr*/) override
    {
        return 0;
    }

    GERANES_INLINE_HOT uint8_t readCHR(int /*addr*/) override
    {
        return 0;
    }

    static DummyCartridgeData& instance()
    {
        static DummyCartridgeData ret;
        return ret;
    }

};

#endif // DUMMYCARTRIDGEDATA_H
