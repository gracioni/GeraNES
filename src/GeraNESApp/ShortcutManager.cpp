#include "GeraNESApp/ShortcutManager.h"

#include <utility>

void ShortcutManager::add(const Data& data)
{
    m_keyMap.insert(std::make_pair(data.key, data));
}

ShortcutManager::Data* ShortcutManager::get(const std::string key)
{
    if(m_keyMap.count(key)) return &(m_keyMap[key]);
    return nullptr;
}

void ShortcutManager::invokeShortcut(const std::string& shortcut)
{
    for(const auto& pair : m_keyMap) {
        if(pair.second.shortcut == shortcut) {
            pair.second.action();
            break;
        }
    }
}
