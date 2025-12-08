#pragma once

#include <signal/signal.h>
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

    void log(std::string_view s, Type type = Type::INFO)
    {
        size_t start = 0;

        while (true)
        {
            size_t end = s.find('\n', start);

            if (end == std::string::npos)
            {
                signalLog(std::string(s.substr(start)), type);
                break;
            }

            signalLog(std::string(s.substr(start, end - start)), type);
            start = end + 1;
        }
    }

};
