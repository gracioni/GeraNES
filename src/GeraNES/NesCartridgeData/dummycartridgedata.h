#ifndef DUMMYCARTRIDGEDATA_H
#define DUMMYCARTRIDGEDATA_H

#include "ICartridgeData.h"

class DummyCartridgeData : public ICartridgeData
{
private:

    DummyCartridgeData() : ICartridgeData(RomFile::emptyRomFile) {
    }

public:

    bool valid() override {
        return true;
    }

    GERANES_INLINE int numberOfPrg16kBanks() override
    {
        return false;
    }

    GERANES_INLINE int numberOfChr8kBanks() override
    {
        return false;
    }

    GERANES_INLINE MirroringType mirroringType() override
    {
        return MirroringType::HORIZONTAL;
    }

    bool hasBatteryRam8k() override
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

    GERANES_INLINE_HOT int numberOfRamBanks() override
    {
        return 0;
    }

    GERANES_INLINE_HOT uint8_t readTrainer(int /*addr*/) override
    {
        return 0;
    }

    GERANES_INLINE_HOT uint8_t readPrg(int /*addr*/) override
    {
        return 0;
    }

    GERANES_INLINE_HOT uint8_t readChr(int /*addr*/) override
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
