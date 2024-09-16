#ifndef MAPPER210_H
#define MAPPER210_H

#include "Mapper019.h"

class Mapper210 : public Mapper019 {

public:

    Mapper210(ICartridgeData& cd) : Mapper019(cd)
    {
    }

    bool useCustomNameTable(uint8_t index) override
    {
        return false;
    }
    
    MirroringType mirroringType() override
    {
        return IMapper::mirroringType();
    }

};

#endif
