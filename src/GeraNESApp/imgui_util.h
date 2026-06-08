#pragma once

#include "imgui_include.h"
#include <algorithm>
#include <cfloat>
#include <unordered_map>

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

    static ImVec2 MainViewportWorkSizeOrFallback()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if(viewport == nullptr) {
            return ImGui::GetIO().DisplaySize;
        }
        return viewport->WorkSize;
    }

    static float EffectiveUiScale()
    {
        const float baseFontSize = 13.0f;
        const float currentFontSize = ImGui::GetFontSize();
        if(currentFontSize <= 0.0f) {
            return 1.0f;
        }
        return std::max(1.0f, currentFontSize / baseFontSize);
    }

    static ImVec2 ScaleUiSize(const ImVec2& size)
    {
        const float scale = EffectiveUiScale();
        return ImVec2(size.x > 0.0f ? size.x * scale : size.x,
                      size.y > 0.0f ? size.y * scale : size.y);
    }

    static ImVec2 ClampWindowSizeToMainViewport(const ImVec2& requestedSize,
                                                float padding = 16.0f,
                                                const ImVec2& minimumSize = ImVec2(160.0f, 80.0f))
    {
        const float scale = EffectiveUiScale();
        const ImVec2 workSize = MainViewportWorkSizeOrFallback();
        const ImVec2 scaledRequestedSize = ScaleUiSize(requestedSize);
        const ImVec2 scaledMinimumSize = ScaleUiSize(minimumSize);
        const float scaledPadding = padding * scale;
        const float maxWidth = std::max(scaledMinimumSize.x, workSize.x - scaledPadding);
        const float maxHeight = std::max(scaledMinimumSize.y, workSize.y - scaledPadding);

        ImVec2 size = scaledRequestedSize;
        if(size.x > 0.0f) {
            size.x = std::clamp(size.x, scaledMinimumSize.x, maxWidth);
        }
        if(size.y > 0.0f) {
            size.y = std::clamp(size.y, scaledMinimumSize.y, maxHeight);
        }
        return size;
    }

    static void SetNextWindowSizeClamped(const ImVec2& requestedSize,
                                         ImGuiCond cond = ImGuiCond_Always,
                                         float padding = 16.0f,
                                         const ImVec2& minimumSize = ImVec2(160.0f, 80.0f))
    {
        ImGui::SetNextWindowSize(
            ClampWindowSizeToMainViewport(requestedSize, padding, minimumSize),
            cond
        );
    }

    static void SetNextWindowPosCenteredOnMainViewport(ImGuiCond cond = ImGuiCond_Appearing)
    {
        if(const ImGuiViewport* viewport = ImGui::GetMainViewport(); viewport != nullptr) {
            ImGui::SetNextWindowPos(viewport->GetCenter(), cond, ImVec2(0.5f, 0.5f));
        } else {
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f), cond, ImVec2(0.5f, 0.5f));
        }
    }

    static void SetNextWindowCenteredOnMainViewport(const ImVec2& requestedSize,
                                                    ImGuiCond sizeCond = ImGuiCond_Appearing,
                                                    ImGuiCond posCond = ImGuiCond_Appearing,
                                                    float padding = 16.0f,
                                                    const ImVec2& minimumSize = ImVec2(160.0f, 80.0f))
    {
        SetNextWindowSizeClamped(requestedSize, sizeCond, padding, minimumSize);
        SetNextWindowPosCenteredOnMainViewport(posCond);
    }

    static void SetNextWindowSizeConstraintsClamped(const ImVec2& requestedMin,
                                                    const ImVec2& requestedMax,
                                                    float padding = 16.0f,
                                                    const ImVec2& fallbackMin = ImVec2(160.0f, 80.0f))
    {
        const float scale = EffectiveUiScale();
        const ImVec2 workSize = MainViewportWorkSizeOrFallback();
        const ImVec2 scaledRequestedMin = ScaleUiSize(requestedMin);
        const ImVec2 scaledFallbackMin = ScaleUiSize(fallbackMin);
        ImVec2 scaledRequestedMax = requestedMax;
        if(scaledRequestedMax.x < FLT_MAX) scaledRequestedMax.x *= scale;
        if(scaledRequestedMax.y < FLT_MAX) scaledRequestedMax.y *= scale;
        const float scaledPadding = padding * scale;
        const float maxAllowedX = std::max(scaledFallbackMin.x, workSize.x - scaledPadding);
        const float maxAllowedY = std::max(scaledFallbackMin.y, workSize.y - scaledPadding);

        ImVec2 minSize(
            std::min(std::max(scaledRequestedMin.x, 1.0f), maxAllowedX),
            std::min(std::max(scaledRequestedMin.y, 1.0f), maxAllowedY)
        );
        ImVec2 maxSize(
            scaledRequestedMax.x >= FLT_MAX ? maxAllowedX : std::min(std::max(scaledRequestedMax.x, minSize.x), maxAllowedX),
            scaledRequestedMax.y >= FLT_MAX ? maxAllowedY : std::min(std::max(scaledRequestedMax.y, minSize.y), maxAllowedY)
        );

        maxSize.x = std::max(maxSize.x, minSize.x);
        maxSize.y = std::max(maxSize.y, minSize.y);
        ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
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
            static std::unordered_map<ImGuiID, float> last_wrapper_scroll_max_y;
            const ImGuiID scroll_state_id = ImGui::GetID(wrapper_id);
            const float previous_wrapper_scroll_max_y = [&]() {
                auto it = last_wrapper_scroll_max_y.find(scroll_state_id);
                return it != last_wrapper_scroll_max_y.end() ? it->second : wrapper_window->ScrollMax.y;
            }();
            const bool was_at_bottom = wrapper_window->Scroll.y >= (previous_wrapper_scroll_max_y - 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
            ImGui::InputTextMultiline(input_id,
                                      buf,
                                      buf_size,
                                      ImVec2(input_width, input_height <= 0.0f ? 0.0f : input_height),
                                      ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleVar();

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
                    if (was_at_bottom) {
                        ImGui::SetScrollY(wrapper_window, wrapper_window->ScrollMax.y);
                        ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
                    }
                }
            }
            last_wrapper_scroll_max_y[scroll_state_id] = wrapper_window->ScrollMax.y;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    static std::string EllipsizeStartToFit(const std::string& text,
                                           float max_width,
                                           const char* ellipsis = "...")
    {
        if(text.empty() || max_width <= 0.0f) {
            return text;
        }

        if(ImGui::CalcTextSize(text.c_str()).x <= max_width) {
            return text;
        }

        const float ellipsis_width = ImGui::CalcTextSize(ellipsis).x;
        if(ellipsis_width >= max_width) {
            return ellipsis;
        }

        std::string fitted = text;
        size_t start_index = 0;
        while(start_index < text.size()) {
            fitted = std::string(ellipsis) + text.substr(start_index);
            if(ImGui::CalcTextSize(fitted.c_str()).x <= max_width) {
                return fitted;
            }
            ++start_index;
        }

        return ellipsis;
    }

    static std::string BuildEllipsizedMenuLabel(const std::string& visible_text,
                                                const std::string& unique_id,
                                                float max_width)
    {
        return EllipsizeStartToFit(visible_text, max_width) + "##" + unique_id;
    }

#ifdef __cplusplus
}
#endif
