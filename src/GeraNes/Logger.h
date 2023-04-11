#ifndef LOGGER_H
#define LOGGER_H

#include "signal/SigSlot.h"
#include <string>

class Logger
{

private:

    Logger() {} //singleton

public:

    SigSlot::Signal<const std::string&> signalLog;

    static Logger& instance()
    {
        static Logger instance;

        return instance;
    }

    void append(const std::string& s)
    {
        signalLog(s);
    }

};

#endif // LOGGER_H
