#pragma once

#include "Mapper090.h"

namespace GeraNES {

class Mapper209 : public Mapper090
{
public:
    static constexpr uint32_t kMapperHookCaps =
        Mapper090::kMapperHookCaps |
        BaseMapper::HookCap_UseCustomNameTable;

    Mapper209(ICartridgeData& cd) : Mapper090(cd)
    {
    }

protected:
    GERANES_INLINE bool jySupportsAdvancedNametables() const override
    {
        return true;
    }
};

} // namespace GeraNES
