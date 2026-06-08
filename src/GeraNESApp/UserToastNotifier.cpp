#include "GeraNESApp/UserToastNotifier.h"

void UserToastNotifier::show(const std::string& message)
{
    m_message = message;
    const Uint64 now = SDL_GetTicks64();
    m_showUntil = now + SHOW_MS;
    m_fadeUntil = m_showUntil + FADE_MS;
}

void UserToastNotifier::draw(ImDrawList* drawList, ImVec2 origin, float viewportWidth, float viewportHeight, ImFont* font)
{
    if(drawList == nullptr || m_message.empty()) return;
    (void)viewportWidth;

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
    ImFont* activeFont = font != nullptr ? font : ImGui::GetFont();
    const float fontSize = font != nullptr ? font->LegacySize : ImGui::GetFontSize();
    const float scale = std::max(1.0f, fontSize / FONT_SIZE);
    const ImVec2 margin = ImVec2(26.0f * scale, 26.0f * scale);

    ImVec2 textSize = activeFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, m_message.c_str());
    ImVec2 textPos = ImVec2(origin.x + margin.x, origin.y + viewportHeight - margin.y - textSize.y);

    ImVec2 boxMin = ImVec2(textPos.x - 8.0f * scale, textPos.y - 6.0f * scale);
    ImVec2 boxMax = ImVec2(textPos.x + textSize.x + 10.0f * scale, textPos.y + textSize.y + 6.0f * scale);

    const int fillAlpha = static_cast<int>(220.0f * alpha);
    const int borderAlpha = static_cast<int>(220.0f * alpha);
    const int textAlpha = static_cast<int>(255.0f * alpha);

    drawList->AddRectFilled(boxMin, boxMax, IM_COL32(20, 28, 38, fillAlpha), 6.0f * scale);
    drawList->AddRect(boxMin, boxMax, IM_COL32(90, 140, 190, borderAlpha), 6.0f * scale);
    DrawTextOutlined(drawList, activeFont, fontSize, textPos, IM_COL32(240, 246, 255, textAlpha), IM_COL32(0, 0, 0, textAlpha), m_message.c_str());
}
