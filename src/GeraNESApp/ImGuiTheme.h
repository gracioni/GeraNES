#pragma once

#include "imgui_include.h"

inline void ApplyImGuiTheme()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.57f, 0.62f, 0.68f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.09f, 0.12f, 0.97f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.11f, 0.14f, 0.70f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.12f, 0.16f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.28f, 0.35f, 0.85f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.16f, 0.22f, 0.90f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.25f, 0.35f, 0.95f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.32f, 0.46f, 0.95f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.20f, 0.30f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.12f, 0.17f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.10f, 0.13f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.31f, 0.43f, 0.95f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.78f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.31f, 0.74f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.38f, 0.86f, 0.97f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.24f, 0.34f, 0.90f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.17f, 0.34f, 0.48f, 0.95f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.43f, 0.60f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.14f, 0.28f, 0.40f, 0.90f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.39f, 0.55f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.49f, 0.67f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.31f, 0.39f, 0.85f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.44f, 0.60f, 0.65f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.31f, 0.59f, 0.78f, 0.85f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.38f, 0.72f, 0.95f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.20f, 0.30f, 0.95f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.21f, 0.40f, 0.56f, 0.95f);
    colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.33f, 0.47f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.50f, 0.78f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.50f, 0.90f, 0.75f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.21f, 0.53f, 0.77f, 0.45f);
}
