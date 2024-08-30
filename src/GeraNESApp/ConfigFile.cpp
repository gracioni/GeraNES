#include "ConfigFile.h"

std::unique_ptr<ConfigFile> ConfigFile::_instance;

ConfigFile& ConfigFile::instance() {
    if (!_instance) {
        _instance.reset(new ConfigFile());
        _instance->load();
    }
    return *_instance;
}

ConfigFile::ConfigFile() {
}

ConfigFile::~ConfigFile() {
    save();
}