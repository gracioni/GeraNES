#pragma once

#include "Mapper019.h"

namespace GeraNES {

class Mapper210 : public Mapper019 {

public:
    static constexpr uint32_t kMapperHookCaps = 0;

    Mapper210(ICartridgeData& cd) : Mapper019(cd)
    {
    }

    GERANES_HOT bool useCustomNameTable(uint8_t /*index*/) override
    {
        return false;
    }
    
    GERANES_HOT MirroringType mirroringType() override
    {
        return BaseMapper::mirroringType();
    }

};

} // namespace GeraNES
