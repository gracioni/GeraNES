#pragma once

#include "Mapper090.h"

namespace GeraNES {

class Mapper211 : public Mapper090
{
public:
    static constexpr uint32_t kMapperHookCaps =
        Mapper090::kMapperHookCaps |
        BaseMapper::HookCap_UseCustomNameTable;

    Mapper211(ICartridgeData& cd) : Mapper090(cd)
    {
    }

protected:
    GERANES_INLINE bool jySupportsAdvancedNametables() const override
    {
        return true;
    }

    GERANES_INLINE bool jyForceExtendedMirroring() const override
    {
        return true;
    }

    GERANES_INLINE bool jyForceRomNametables() const override
    {
        return true;
    }
};

} // namespace GeraNES
