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
    void show(const std::string& message)
    {
        m_message = message;
        const Uint64 now = SDL_GetTicks64();
        m_showUntil = now + SHOW_MS;
        m_fadeUntil = m_showUntil + FADE_MS;
    }

    void draw(ImDrawList* drawList, float viewportWidth, float viewportHeight, ImFont* font = nullptr)
    {
        if(drawList == nullptr || m_message.empty()) return;

        const Uint64 now = SDL_GetTicks64();
        if(now >= m_fadeUntil) {
            m_message.clear();
            return;
        }

        float alpha = 1.0f;
        if(now > m_showUntil) {
            const float remaining = static_cast<float>(m_fadeUntil - now);
            alpha = std::clamp(remaining / static_cast<float>(FADE_MS), 0.0f, 1.0f);
        }

        // Moved 8px up/right from previous placement.
        const ImVec2 margin = ImVec2(26.0f, 26.0f);

        ImFont* activeFont = font != nullptr ? font : ImGui::GetFont();
        ImVec2 textSize = activeFont->CalcTextSizeA(FONT_SIZE, FLT_MAX, 0.0f, m_message.c_str());
        ImVec2 textPos = ImVec2(margin.x, viewportHeight - margin.y - textSize.y);

        ImVec2 boxMin = ImVec2(textPos.x - 8.0f, textPos.y - 6.0f);
        ImVec2 boxMax = ImVec2(textPos.x + textSize.x + 10.0f, textPos.y + textSize.y + 6.0f);

        const int fillAlpha = static_cast<int>(220.0f * alpha);
        const int borderAlpha = static_cast<int>(220.0f * alpha);
        const int textAlpha = static_cast<int>(255.0f * alpha);

        drawList->AddRectFilled(boxMin, boxMax, IM_COL32(20, 28, 38, fillAlpha), 6.0f);
        drawList->AddRect(boxMin, boxMax, IM_COL32(90, 140, 190, borderAlpha), 6.0f);
        DrawTextOutlined(drawList, activeFont, FONT_SIZE, textPos, IM_COL32(240, 246, 255, textAlpha), IM_COL32(0, 0, 0, textAlpha), m_message.c_str());
    }
};
