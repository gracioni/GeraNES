#pragma once

inline void GeraNESApp::drawImprovementsWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(320.0f, 0.0f));

    if(ImGui::Begin("Improvements", &m_showImprovementsWindow, ImGuiWindowFlags_NoResize)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        bool disableSpritesLimit = m_emu.spriteLimitDisabled();
        if(ImGui::Checkbox("Disable Sprites Limit", &disableSpritesLimit)) {
            m_emu.disableSpriteLimit(disableSpritesLimit);
        }
        AppSettings::instance().data.improvements.disableSpritesLimit = m_emu.spriteLimitDisabled();

        bool overclock = m_emu.overclocked();
        if(ImGui::Checkbox("Overclock", &overclock)) {
            m_emu.enableOverclock(overclock);
        }
        AppSettings::instance().data.improvements.overclock = m_emu.overclocked();

        ImGui::SetNextItemWidth(100);

        const bool netplayRewindDisabled = shouldSuppressRewindForNetplay();
        int value = netplayRewindDisabled
            ? 0
            : AppSettings::instance().data.improvements.maxRewindTime;
        ImGui::BeginDisabled(netplayRewindDisabled);
        if(ImGui::InputInt("Max Rewind Time(s)", &value)) {
            value = std::max(0, value);
            AppSettings::instance().data.improvements.maxRewindTime = value;
            m_emu.setupRewindSystem(value > 0, value);
        }
        ImGui::EndDisabled();
        if(netplayRewindDisabled) {
            ImGui::TextDisabled("Rewind is disabled while netplay is active.");
        }
    }

    ImGui::End();
}
