#ifndef DUMMYMAPPER_H
#define DUMMYMAPPER_H

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

#endif // DUMMYMAPPER_H
