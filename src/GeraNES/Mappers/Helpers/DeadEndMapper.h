#pragma once

#include "../BaseMapper.h"
#include "logger/logger.h"

class DeadEndMapper : public BaseMapper
{
private:
    int m_mapperId = 0;
    const char* m_message = nullptr;
    bool m_logged = false;

    void logOnce()
    {
        if(m_logged) return;
        m_logged = true;

        std::string msg = "Mapper " + std::to_string(m_mapperId) + ": ";
        msg += (m_message != nullptr) ? m_message : "undocumented/unsupported legacy assignment stub.";
        Logger::instance().log(msg, Logger::Type::WARNING);
    }

protected:
    DeadEndMapper(ICartridgeData& cd, int mapperId, const char* message)
        : BaseMapper(cd), m_mapperId(mapperId), m_message(message)
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
