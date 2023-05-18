#ifndef LOGGER_H
#define LOGGER_H

#include "signal/SigSlot.h"
#include <string>

class Logger
{

private:

    Logger() {} //singleton

public:

    enum LogFlags {INFO = 1, DEBUG = 2, ERROR2 = 4};

    SigSlot::Signal<const std::string&, int> signalLog;

    static Logger& instance()
    {
        static Logger instance;

        return instance;
    }

    void log(const std::string& s, int flags = LogFlags::INFO)
    {
        signalLog(s, flags);
    }

};

#endif // LOGGER_H
