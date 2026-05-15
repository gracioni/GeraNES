#include "CrashHandler.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <typeinfo>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif defined(__has_include)
    #if __has_include(<execinfo.h>)
        #include <execinfo.h>
        #define GERANES_HAS_EXECINFO 1
    #endif
#endif

namespace
{
    constexpr const char* kLogFile = "log.txt";
    std::atomic_flag g_writingCrashLog = ATOMIC_FLAG_INIT;

    const char* signalName(int signalNumber)
    {
        switch(signalNumber) {
            case SIGABRT: return "SIGABRT";
            case SIGFPE: return "SIGFPE";
            case SIGILL: return "SIGILL";
            case SIGINT: return "SIGINT";
            case SIGSEGV: return "SIGSEGV";
            case SIGTERM: return "SIGTERM";
            default: return "unknown signal";
        }
    }

    void writeTimestamp(FILE* file)
    {
        const std::time_t now = std::time(nullptr);
        std::tm localTime{};
#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif
        char buffer[64]{};
        if(std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime) > 0) {
            std::fprintf(file, "Time: %s\n", buffer);
        }
    }

    void writeStackTrace(FILE* file)
    {
#ifdef _WIN32
        void* frames[64]{};
        const USHORT frameCount = CaptureStackBackTrace(0, static_cast<DWORD>(sizeof(frames) / sizeof(frames[0])), frames, nullptr);
        std::fprintf(file, "Stack frames: %hu\n", frameCount);
        for(USHORT i = 0; i < frameCount; ++i) {
            std::fprintf(file, "  #%02hu %p\n", i, frames[i]);
        }
#elif defined(GERANES_HAS_EXECINFO)
        void* frames[64]{};
        const int frameCount = backtrace(frames, static_cast<int>(sizeof(frames) / sizeof(frames[0])));
        std::fprintf(file, "Stack frames: %d\n", frameCount);
        for(int i = 0; i < frameCount; ++i) {
            std::fprintf(file, "  #%02d %p\n", i, frames[i]);
        }
#else
        std::fprintf(file, "Stack frames: unavailable on this platform\n");
#endif
    }

    void writeCrashHeader(FILE* file, const char* reason)
    {
        std::fprintf(file, "\n==== GeraNES crash report ====\n");
        writeTimestamp(file);
        std::fprintf(file, "Reason: %s\n", reason != nullptr ? reason : "unknown");
    }

    void writeCurrentException(FILE* file)
    {
        const std::exception_ptr current = std::current_exception();
        if(!current) {
            std::fprintf(file, "Current C++ exception: none\n");
            return;
        }

        try {
            std::rethrow_exception(current);
        } catch(const std::exception& ex) {
            std::fprintf(file, "Current C++ exception: %s: %s\n", typeid(ex).name(), ex.what());
        } catch(...) {
            std::fprintf(file, "Current C++ exception: non-standard exception\n");
        }
    }

    FILE* openCrashLog()
    {
        return std::fopen(kLogFile, "ab");
    }

    void appendCrashReport(const char* reason, void* detailContext = nullptr, void (*writeDetails)(FILE*, void*) = nullptr)
    {
        if(g_writingCrashLog.test_and_set(std::memory_order_acq_rel)) {
            return;
        }

        FILE* file = openCrashLog();
        if(file != nullptr) {
            writeCrashHeader(file, reason);
            if(writeDetails != nullptr) {
                writeDetails(file, detailContext);
            }
            writeCurrentException(file);
            writeStackTrace(file);
            std::fprintf(file, "==== end crash report ====\n");
            std::fflush(file);
            std::fclose(file);
        }

        g_writingCrashLog.clear(std::memory_order_release);
    }

    void terminateHandler()
    {
        appendCrashReport("std::terminate");
        std::signal(SIGABRT, SIG_DFL);
        std::abort();
    }

    void signalHandler(int signalNumber)
    {
        appendCrashReport(
            signalName(signalNumber),
            &signalNumber,
            [](FILE* file, void* context) {
                const int signal = context != nullptr ? *static_cast<int*>(context) : 0;
                std::fprintf(file, "Signal: %d (%s)\n", signal, signalName(signal));
            }
        );

        std::signal(signalNumber, SIG_DFL);
        std::raise(signalNumber);
    }

#ifdef _WIN32
    const char* windowsExceptionName(DWORD code)
    {
        switch(code) {
            case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
            case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
            case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
            case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
            case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
            case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
            case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
            case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
            case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
            case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
            case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
            case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
            case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
            case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
            case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
            case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
            case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
            case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
            default: return "unknown Windows exception";
        }
    }

    LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
    {
        appendCrashReport(
            "unhandled Windows exception",
            exceptionPointers,
            [](FILE* file, void* context) {
            auto* pointers = static_cast<EXCEPTION_POINTERS*>(context);
            if(pointers == nullptr || pointers->ExceptionRecord == nullptr) {
                std::fprintf(file, "Windows exception: missing exception record\n");
                return;
            }

            const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
            std::fprintf(
                file,
                "Windows exception: 0x%08lX (%s)\n",
                static_cast<unsigned long>(record->ExceptionCode),
                windowsExceptionName(record->ExceptionCode)
            );
            std::fprintf(file, "Exception address: %p\n", record->ExceptionAddress);

            if(record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
                const ULONG_PTR operation = record->ExceptionInformation[0];
                const ULONG_PTR address = record->ExceptionInformation[1];
                const char* operationName =
                    operation == 0 ? "read" :
                    operation == 1 ? "write" :
                    operation == 8 ? "execute" :
                    "access";
                std::fprintf(file, "Access violation: attempted %s at %p\n", operationName, reinterpret_cast<void*>(address));
            }
            }
        );
        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif
}

namespace CrashHandler
{
    void install()
    {
        std::set_terminate(terminateHandler);

        std::signal(SIGABRT, signalHandler);
        std::signal(SIGFPE, signalHandler);
        std::signal(SIGILL, signalHandler);
        std::signal(SIGSEGV, signalHandler);
        std::signal(SIGTERM, signalHandler);

#ifdef _WIN32
        SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
    }
}
