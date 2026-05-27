#pragma once

inline void GeraNESApp::drawAboutWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(320.0f, 0.0f));

    if(ImGui::Begin("About", &m_showAboutWindow, ImGuiWindowFlags_NoResize)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        std::string txt = std::string(GERANES_NAME) + " " + GERANES_VERSION;

        TextCenteredWrapped(txt);

        txt = std::string("Racionisoft 2015 - ") + std::to_string(compileTimeYear());

        ImGui::NewLine();
        TextCenteredWrapped(txt);

        ImGui::NewLine();
        ImGui::NewLine();

        txt = "geraldoracioni@gmail.com";
        TextCenteredWrapped(txt);
    }

    ImGui::End();
}
