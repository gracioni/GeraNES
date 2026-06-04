#pragma once

#include "../BaseMapper.h"
#include "logger/logger.h"

namespace GeraNES {

class DeadEndMapper : public BaseMapper
{
private:
    int m_mapperId = 0;
    bool m_logged = false;

    void logOnce()
    {
        if(m_logged) return;
        m_logged = true;

        std::string msg = "Mapper " + std::to_string(m_mapperId) + ": undocumented/unsupported legacy assignment stub.";
        Logger::instance().log(msg, Logger::Type::WARNING);
    }

protected:
    DeadEndMapper(ICartridgeData& cd, int mapperId)
        : BaseMapper(cd), m_mapperId(mapperId)
    {
        logOnce();
    }

public:
    void reset() override
    {
        logOnce();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_mapperId);
        SERIALIZEDATA(s, m_logged);
    }
};

} // namespace GeraNES
