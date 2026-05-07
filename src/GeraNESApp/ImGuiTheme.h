#pragma once

#include "imgui_include.h"

inline void ApplyImGuiTheme()
{
    ImGui::StyleColorsLight();

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
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.44f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.82f, 0.82f, 0.79f, 0.98f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.85f, 0.85f, 0.82f, 0.80f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.86f, 0.85f, 0.82f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.33f, 0.33f, 0.35f, 0.75f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.74f, 0.74f, 0.71f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.79f, 0.79f, 0.76f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.68f, 0.68f, 0.65f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.72f, 0.72f, 0.69f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.77f, 0.77f, 0.74f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.72f, 0.72f, 0.69f, 0.90f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.72f, 0.72f, 0.69f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.78f, 0.78f, 0.75f, 0.90f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.52f, 0.52f, 0.50f, 0.95f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.60f, 0.58f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.44f, 0.44f, 0.42f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.72f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.72f, 0.16f, 0.16f, 0.92f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.82f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.63f, 0.63f, 0.60f, 0.95f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.70f, 0.67f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.58f, 0.58f, 0.55f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.72f, 0.16f, 0.16f, 0.82f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.78f, 0.24f, 0.24f, 0.92f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.63f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.38f, 0.40f, 0.44f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.54f, 0.54f, 0.57f, 0.90f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.66f, 0.66f, 0.69f, 0.60f);
    colors[ImGuiCol_Separator] = ImVec4(0.44f, 0.44f, 0.46f, 0.85f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.72f, 0.16f, 0.16f, 0.35f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.82f, 0.22f, 0.22f, 0.70f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.88f, 0.28f, 0.28f, 0.90f);
    colors[ImGuiCol_Tab] = ImVec4(0.69f, 0.69f, 0.66f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.77f, 0.77f, 0.74f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.78f, 0.24f, 0.24f, 0.92f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.67f, 0.67f, 0.64f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.72f, 0.72f, 0.69f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.34f, 0.34f, 0.38f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.72f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.80f, 0.22f, 0.22f, 0.25f);
    if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}
