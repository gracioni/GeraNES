#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

class AccuracyTrace
{
public:
    static void reset()
    {
        std::lock_guard<std::mutex> lock(mutex());
        std::ofstream out(logPath(), std::ios::trunc);
        lineCount() = 0;
    }

    static void log(const std::string& line)
    {
        std::lock_guard<std::mutex> lock(mutex());

        if(lineCount() >= 50000) {
            return;
        }

        std::ofstream out(logPath(), std::ios::app);
        out << line << '\n';
        ++lineCount();
    }

private:
    static std::filesystem::path logPath()
    {
        return std::filesystem::current_path() / "accuracy_trace.log";
    }

    static std::mutex& mutex()
    {
        static std::mutex m;
        return m;
    }

    static int& lineCount()
    {
        static int count = 0;
        return count;
    }
};
