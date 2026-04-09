#pragma once

#include "imgui_include.h"
#include <algorithm>

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

    static void InputTextMultilineLog(const char* wrapper_id,
                                      const char* input_id,
                                      char* buf,
                                      size_t buf_size,
                                      const ImVec2& size,
                                      bool auto_scroll = true)
    {
        if (buf == nullptr || buf_size == 0) return;

        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 text_size = ImGui::CalcTextSize(buf, nullptr, false);
        const float visible_width = size.x > 0.0f ? size.x : ImGui::GetContentRegionAvail().x;
        const float visible_height = size.y;
        const float required_width = text_size.x + style.FramePadding.x * 2.0f + 1.0f;
        const float required_height = text_size.y + style.FramePadding.y * 2.0f + ImGui::GetFrameHeight();
        const bool need_horizontal_scroll = required_width > visible_width;
        const float available_inner_height =
            (visible_height > 0.0f && need_horizontal_scroll)
                ? std::max(1.0f, visible_height - style.ScrollbarSize)
                : visible_height;
        const float input_width = -1.0f;
        const float input_height = visible_height > 0.0f ? std::max(required_height, available_inner_height) : 0.0f;
        const ImGuiWindowFlags wrapper_flags = need_horizontal_scroll ? ImGuiWindowFlags_HorizontalScrollbar : ImGuiWindowFlags_None;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowContentSize(ImVec2(need_horizontal_scroll ? required_width : 0.0f, input_height));
        if (ImGui::BeginChild(wrapper_id, size, ImGuiChildFlags_None, wrapper_flags))
        {
            ImGuiWindow* const wrapper_window = ImGui::GetCurrentWindow();
            ImGui::InputTextMultiline(input_id,
                                      buf,
                                      buf_size,
                                      ImVec2(input_width, input_height <= 0.0f ? 0.0f : input_height),
                                      ImGuiInputTextFlags_ReadOnly);

            ImGuiContext& g = *GImGui;
            const char* child_window_name = nullptr;
            ImFormatStringToTempBuffer(&child_window_name,
                                       nullptr,
                                       "%s/%s_%08X",
                                       g.CurrentWindow->Name,
                                       input_id,
                                       ImGui::GetID(input_id));
            ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);

            if (child_window)
            {
                child_window->Scroll.x = wrapper_window->Scroll.x;
                if (auto_scroll && !(ImGui::IsItemActive() || ImGui::IsItemEdited()))
                {
                    ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

#ifdef __cplusplus
}
#endif
