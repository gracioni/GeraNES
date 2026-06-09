#pragma once

inline void GeraNESApp::drawErrorWindow()
{
    ImVec2 minimumSize(160.0f, 80.0f);
#ifdef __ANDROID__
    minimumSize = ImVec2(320.0f, 120.0f);
#endif
    SetNextWindowPosCenteredOnMainViewport(ImGuiCond_Appearing);
    SetNextWindowSizeConstraintsClamped(minimumSize, ImVec2(FLT_MAX, FLT_MAX));

    bool lastState = m_showErrorWindow;

    if(ImGui::Begin("Error",
                    &m_showErrorWindow,
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if(ImGui::IsWindowAppearing()) {
            if(const ImGuiViewport* viewport = ImGui::GetMainViewport(); viewport != nullptr) {
                const ImVec2 windowSize = ImGui::GetWindowSize();
                ImGui::SetWindowPos(ImVec2(
                    viewport->GetCenter().x - windowSize.x * 0.5f,
                    viewport->GetCenter().y - windowSize.y * 0.5f
                ));
            }
        }
        float windowWidth = ImGui::GetContentRegionAvail().x;

        TextCenteredWrapped(m_errorMessage.c_str());

        ImGui::Spacing();
        ImGui::Spacing();

        const char* btnLabel = "OK";

        ImVec2 btnSize = ImGui::CalcTextSize(btnLabel);
        btnSize.x += ImGui::GetStyle().FramePadding.x * 2.0f;
        btnSize.y += ImGui::GetStyle().FramePadding.y * 2.0f;

        float posX = (windowWidth - btnSize.x) * 0.5f;
        ImGui::SetCursorPosX(posX);

        if(ImGui::Button(btnLabel, btnSize)) {
            m_showErrorWindow = false;
        }
    }
    ImGui::End();

    if(lastState && !m_showErrorWindow) m_showErrorWindow = false;
}
