#ifndef FILESYSTEM_INCLUDE_H
#define FILESYSTEM_INCLUDE_H

#if __GNUC__
    #if __GNUC__ >= 8 || defined(__EMSCRIPTEN__)
        #include <filesystem>
        namespace fs = std::filesystem;
    #else
        #include <experimental/filesystem>
        namespace fs = std::experimental::filesystem;
    #endif
#else
    #include <filesystem>
    namespace fs = std::filesystem;
#endif

#endif
