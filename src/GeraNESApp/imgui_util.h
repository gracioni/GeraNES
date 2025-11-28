#ifndef IMGUI_UTIL_H
#define IMGUI_UTIL_H

#include "imgui_include.h"

#ifdef __cplusplus
extern "C" {
#endif

    static void TextCenteredWrapped(const std::string& text) {
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth = ImGui::CalcTextSize(text.c_str()).x;

        // Calculate indentation for centering (adjust for wrapping)
        // This assumes the text fits on one line for the initial centering calculation.
        // For true wrapped centering, more complex layout calculations would be needed.
        float indentation = (windowWidth - textWidth) * 0.5f;

        // Ensure minimum indentation if text is too wide
        if (indentation < 0) {
            indentation = 0; // Or a small positive value for padding
        }

        ImGui::SameLine(indentation);
        ImGui::PushTextWrapPos(windowWidth - indentation); // Set wrap position relative to window
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopTextWrapPos();
    }

    // draw text with an outline (stroke)
    // drawList: ImDrawList*
    // font: ImFont* (use nullptr para a fonte padrão)
    // font_size: float (use 0.0f para usar g.FontSize? better pass explicit)
    // pos: ImVec2 (top-left of text)
    // col: ImU32 main color (e.g. IM_COL32(255,255,255,255))
    // outline_col: ImU32 outline color (e.g. IM_COL32(0,0,0,255))
    // thickness: integer pixels for outline radius (1..3 is common)
    // text: const char*
    static void DrawTextOutlined(ImDrawList* drawList, ImFont* font, float font_size,
                                ImVec2 pos, ImU32 col, ImU32 outline_col,
                                const char* text, int thickness = 1)
    {
        if (!drawList || !text) return;
        // draw offsets in a 3x3 grid skipping center
        for (int oy = -thickness; oy <= thickness; ++oy)
        {
            for (int ox = -thickness; ox <= thickness; ++ox)
            {
                if (ox == 0 && oy == 0) continue;
                drawList->AddText(font, font_size, ImVec2(pos.x + (float)ox, pos.y + (float)oy), outline_col, text);
            }
        }
        // main text on top
        drawList->AddText(font, font_size, pos, col, text);
    }

#ifdef __cplusplus
}
#endif

#endif
