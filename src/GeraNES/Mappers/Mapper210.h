#ifndef MAPPER210_H
#define MAPPER210_H

#include "Mapper019.h"

class Mapper210 : public Mapper019 {

public:

    Mapper210(ICartridgeData& cd) : Mapper019(cd)
    {
    }

    GERANES_HOT bool useCustomNameTable(uint8_t index) override
    {
        return false;
    }
    
    GERANES_HOT MirroringType mirroringType() override
    {
        return BaseMapper::mirroringType();
    }

};

#endif
