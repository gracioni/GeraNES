#ifndef LOGGER_H
#define LOGGER_H

#include "signal/SigSlot.h"
#include <string>

class Logger
{

private:

    Logger() {} //singleton

    Logger(const Logger&) = delete;
    Logger& operator = (const Logger&) = delete;

public:

    enum class Type {INFO = 1, WARNING = 2, ERROR = 3, DEBUG = 4};

    SigSlot::Signal<const std::string&, Type> signalLog;

    static Logger& instance()
    {
        static Logger instance;
        return instance;
    }

    void log(const std::string& s, Type type = Type::INFO)
    {
        signalLog(s, type);
    }

};

#endif // LOGGER_H
