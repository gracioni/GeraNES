#pragma once

#include <SDL.h>
#include <algorithm>
#include <cfloat>
#include <string>

#include "imgui_include.h"
#include "imgui_util.h"

class UserToastNotifier {
private:
    std::string m_message;
    Uint64 m_showUntil = 0;
    Uint64 m_fadeUntil = 0;

    static constexpr Uint64 SHOW_MS = 2200;
    static constexpr Uint64 FADE_MS = 450;
    static constexpr float FONT_SIZE = 24.0f;

public:
    void show(const std::string& message);
    void draw(ImDrawList* drawList, float viewportWidth, float viewportHeight, ImFont* font = nullptr);
};
