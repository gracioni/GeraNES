#pragma once

inline void GeraNESApp::drawArkanoidFamicomConfigWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(360.0f, 0.0f));

    if(ImGui::Begin("Arkanoid Controller Config (Famicom)", &m_showArkanoidFamicomConfigWindow, ImGuiWindowFlags_NoResize)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ImGui::TextWrapped("The Famicom Arkanoid paddle uses grabbed relative mouse movement. Press Escape to release the mouse.");
        ImGui::Separator();

        float sensitivity = AppSettings::instance().data.input.arkanoid.sensitivity;
        ImGui::SetNextItemWidth(180.0f);
        if(ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 4.0f, "%.2fx")) {
            AppSettings::instance().data.input.arkanoid.sensitivity = std::clamp(sensitivity, 0.05f, 4.0f);
        }
    }

    ImGui::End();
}
