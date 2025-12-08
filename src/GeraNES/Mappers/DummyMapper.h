#pragma once

#include "BaseMapper.h"
#include "GeraNES/NesCartridgeData/ICartridgeData.h"
#include "GeraNES/NesCartridgeData/DummyCartridgeData.h"

class DummyMapper : public BaseMapper
{

public:

    DummyMapper() : BaseMapper(DummyCartridgeData::instance())
    {
    }

};
