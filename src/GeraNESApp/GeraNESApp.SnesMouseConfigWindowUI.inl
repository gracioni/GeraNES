#pragma once

inline void GeraNESApp::drawSnesMouseConfigWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(360.0f, 0.0f));

    if(ImGui::Begin("Mouse Config", &m_showSnesMouseConfigWindow, ImGuiWindowFlags_NoResize)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ImGui::TextWrapped("The selected mouse device uses grabbed relative movement. Press Escape to release the mouse.");
        ImGui::Separator();

        float sensitivity = AppSettings::instance().data.input.snesMouse.sensitivity;
        ImGui::SetNextItemWidth(180.0f);
        if(ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 2.0f, "%.2fx")) {
            AppSettings::instance().data.input.snesMouse.sensitivity = std::clamp(sensitivity, 0.05f, 2.0f);
        }
    }

    ImGui::End();
}
