#ifndef DUMMYMAPPER_H
#define DUMMYMAPPER_H

#include "IMapper.h"
#include "GeraNES/NesCartridgeData/ICartridgeData.h"
#include "GeraNES/NesCartridgeData/DummyCartridgeData.h"

class DummyMapper : public IMapper
{

public:

    DummyMapper() : IMapper(DummyCartridgeData::instance())
    {
    }

};

#endif // DUMMYMAPPER_H
