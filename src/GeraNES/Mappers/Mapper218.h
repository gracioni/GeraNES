#pragma once

#include "BaseMapper.h"

class Mapper218 : public BaseMapper
{
private:
    enum class MirrorMode : uint8_t
    {
        Vertical = 0,
        Horizontal = 1,
        ScreenA = 2,
        ScreenB = 3
    };

    MirrorMode m_mode = MirrorMode::Horizontal;

public:
    Mapper218(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.useFourScreenMirroring()) {
            m_mode = (cd.mirroringType() == MirroringType::VERTICAL) ? MirrorMode::ScreenB : MirrorMode::ScreenA;
        } else if(cd.mirroringType() == MirroringType::VERTICAL) {
            m_mode = MirrorMode::Vertical;
        } else {
            m_mode = MirrorMode::Horizontal;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::CUSTOM;
    }

    GERANES_HOT uint8_t customMirroring(uint8_t blockIndex) override
    {
        switch(m_mode) {
        case MirrorMode::Vertical:
            return static_cast<uint8_t>(blockIndex & 0x01);
        case MirrorMode::Horizontal:
            return static_cast<uint8_t>((blockIndex >> 1) & 0x01);
        case MirrorMode::ScreenA:
            return 0;
        case MirrorMode::ScreenB:
            return 1;
        }
        return 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_mode);
    }
};
