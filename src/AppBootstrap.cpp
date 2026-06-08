#include "AppBootstrap.h"

#include <filesystem>

#ifdef __ANDROID__
#include <SDL_system.h>
#endif

#include "GeraNES/GameDatabase.h"
#include "GeraNES/PathConfig.h"
#include "GeraNESApp/AppSettings.h"

namespace
{
    void copyMissingFile(const std::filesystem::path& source, const std::filesystem::path& destination)
    {
        std::error_code ec;
        if(source.empty() || destination.empty() || !std::filesystem::exists(source, ec) || std::filesystem::exists(destination, ec)) {
            return;
        }

        std::filesystem::create_directories(destination.parent_path(), ec);
        if(ec) {
            return;
        }

        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::skip_existing, ec);
    }

    std::filesystem::path resolveAndroidBundledDbPath(const std::filesystem::path& runtimeDataDir)
    {
#ifdef __ANDROID__
        const std::filesystem::path directDbPath = runtimeDataDir / "db.txt";
        if(std::filesystem::exists(directDbPath)) {
            return directDbPath;
        }

        const std::filesystem::path dataDbPath = runtimeDataDir / "data" / "db.txt";
        if(std::filesystem::exists(dataDbPath)) {
            return dataDbPath;
        }
#else
        (void)runtimeDataDir;
#endif
        return {};
    }

    std::filesystem::path resolveAndroidContentRoot(const char* externalStoragePath, const std::filesystem::path& internalRoot)
    {
#ifdef __ANDROID__
        if(externalStoragePath != nullptr && externalStoragePath[0] != '\0') {
            return std::filesystem::path(externalStoragePath) / "GeraNES";
        }
#endif
        return internalRoot / "content";
    }

    void configureRuntimePaths(const char* argv0)
    {
        try {
#ifdef __ANDROID__
            const char* androidInternalStorage = SDL_AndroidGetInternalStoragePath();
            const char* androidExternalStorage = SDL_AndroidGetExternalStoragePath();
            if(androidInternalStorage != nullptr && androidInternalStorage[0] != '\0') {
                const std::filesystem::path internalStorageRoot(androidInternalStorage);
                const std::filesystem::path contentRoot = resolveAndroidContentRoot(androidExternalStorage, internalStorageRoot);
                const std::filesystem::path runtimeDataDir = internalStorageRoot / "runtime_data";
                const std::filesystem::path internalSettingsPath = internalStorageRoot / "settings.json";
                const std::filesystem::path contentSettingsPath = contentRoot / "settings.json";
                std::filesystem::create_directories(runtimeDataDir);
                std::filesystem::create_directories(contentRoot);
                copyMissingFile(internalSettingsPath, contentSettingsPath);
                std::filesystem::current_path(runtimeDataDir);
                GeraNES::setContentRoot(contentRoot);
                AppSettings::setStorageDirectory(internalStorageRoot);
                AppSettings::setContentDirectory(contentRoot);
                return;
            }
#endif

            if(argv0 != nullptr && argv0[0] != '\0') {
                const std::filesystem::path exePath = std::filesystem::absolute(argv0);
                const std::filesystem::path exeDir = exePath.parent_path();
                if(!exeDir.empty()) {
                    std::filesystem::current_path(exeDir);
                    GeraNES::setContentRoot(exeDir);
                    AppSettings::setStorageDirectory(exeDir);
                    AppSettings::setContentDirectory(exeDir);
                    return;
                }
            }
        }
        catch(...) {
        }

        GeraNES::setContentRoot(std::filesystem::current_path());
        AppSettings::setStorageDirectory(std::filesystem::current_path());
        AppSettings::setContentDirectory(std::filesystem::current_path());
    }

    void configureDatabasePath()
    {
        const std::filesystem::path contentRoot = AppSettings::contentDirectory();
        const std::filesystem::path cwd = std::filesystem::current_path();
        const std::filesystem::path contentDbPath = contentRoot / "db.txt";
#ifdef __ANDROID__
        const std::filesystem::path androidBundledDbPath = resolveAndroidBundledDbPath(cwd);
#endif
        const std::filesystem::path dataDbPath = cwd / "data" / "db.txt";
        if(std::filesystem::exists(contentDbPath)) {
            GeraNES::GameDatabase::setDatabasePath(contentDbPath.string());
        }
#ifdef __ANDROID__
        else if(!androidBundledDbPath.empty()) {
            GeraNES::GameDatabase::setDatabasePath(androidBundledDbPath.string());
        }
#endif
        else if(std::filesystem::exists(dataDbPath)) {
            GeraNES::GameDatabase::setDatabasePath(dataDbPath.string());
        } else {
            GeraNES::GameDatabase::setDatabasePath((cwd / "db.txt").string());
        }
    }
}

namespace AppBootstrap
{
    void initializeAppEnvironment(const char* argv0)
    {
        configureRuntimePaths(argv0);
        configureDatabasePath();
    }
}
