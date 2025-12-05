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

    void add(const Data& data) {
        m_keyMap.insert(make_pair(data.key,data));
    }

    Data* get(const std::string key) {        
        if(m_keyMap.count(key)) return &(m_keyMap[key]);
        return nullptr;
    }

    void invokeShortcut(const std::string& shortcut) {

        //std::cout << "calling " << shortcut << std::endl;

        for (const auto& pair : m_keyMap) {

            if(pair.second.shortcut == shortcut) {
                pair.second.action();
                break;
            }
        }
    }

};
