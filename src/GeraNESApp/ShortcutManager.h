#pragma once

#include <string>
#include <functional>
#include <map>

class ShortcutManager {

public:

    struct Data {
        std::string key;
        std::string label;
        std::string shortcut;
        std::function<void()> action;
    };

private:

    std::map<std::string,Data> m_keyMap;

public:

    void add(const Data& data);
    Data* get(const std::string key);
    void invokeShortcut(const std::string& shortcut);

};
