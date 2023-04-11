#ifndef DUMMYMAPPER_H
#define DUMMYMAPPER_H

#include "IMapper.h"
#include "GeraNes/NesCartridgeData/ICartridgeData.h"
#include "GeraNes/NesCartridgeData/dummycartridgedata.h"



class DummyMapper : public IMapper
{


public:

    DummyMapper() : IMapper(DummyCartridgeData::instance())
    {
    }



};

#endif // DUMMYMAPPER_H
