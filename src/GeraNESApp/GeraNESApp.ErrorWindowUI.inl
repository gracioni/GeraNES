#pragma once

inline void GeraNESApp::drawErrorWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(320.0f, 0.0f));

    bool lastState = m_showErrorWindow;

    if(ImGui::Begin("Error", &m_showErrorWindow)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
