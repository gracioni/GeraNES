#pragma once

inline void GeraNESApp::drawLogWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(600.0f, 0.0f), ImGuiCond_Once);

    if(ImGui::Begin("Log", &m_showLogWindow)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        InputTextMultilineLog("##LogOuterChild", "##LogMultilineInput", m_logBuf.data(), m_logBuf.size(), ImVec2(-1, 400));

        ImGui::Spacing();

        const char* copyBtnLabel = "Copy";
        const char* clearBtnLabel = "Clear";
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 copyBtnTextSize = ImGui::CalcTextSize(copyBtnLabel);
        const ImVec2 clearBtnTextSize = ImGui::CalcTextSize(clearBtnLabel);
        const ImVec2 copyBtnSize = ImVec2(copyBtnTextSize.x + style.FramePadding.x * 2.0f,
                                          copyBtnTextSize.y + style.FramePadding.y * 2.0f);
        const ImVec2 clearBtnSize = ImVec2(clearBtnTextSize.x + style.FramePadding.x * 2.0f,
                                           clearBtnTextSize.y + style.FramePadding.y * 2.0f);

        const float buttonRowWidth = copyBtnSize.x + style.ItemSpacing.x + clearBtnSize.x;
        const float windowWidth = ImGui::GetContentRegionAvail().x;
        const float posX = (windowWidth - buttonRowWidth) * 0.5f;
        ImGui::SetCursorPosX(posX);

        if(ImGui::Button(copyBtnLabel, copyBtnSize)) {
#ifdef __EMSCRIPTEN__
            emcriptenCopyTextToClipboardExact(m_log.c_str());
#else
            ImGui::SetClipboardText(m_log.c_str());
#endif
        }
        ImGui::SameLine();
        if(ImGui::Button(clearBtnLabel, clearBtnSize)) {
            m_log.clear();
            m_logBuf.clear();
            m_logBuf.push_back('\0');
        }
    }

    ImGui::End();
}
