#pragma once

inline void GeraNESApp::drawOverscanWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(320.0f, 0.0f));
    if(!ImGui::Begin("Overscan", &m_showOverscanWindow, ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }

    m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    auto overscan = m_overscanConfig;
    bool changed = false;
    changed |= ImGui::InputInt("Top", &overscan.top);
    changed |= ImGui::InputInt("Right", &overscan.right);
    changed |= ImGui::InputInt("Bottom", &overscan.bottom);
    changed |= ImGui::InputInt("Left", &overscan.left);

    if(ImGui::Button("Reset Defaults")) {
        overscan = { true, 8, 0, 8, 0 };
        changed = true;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Visible area is cropped on all sides.");

    if(changed) {
        overscan.enabled = true;
        overscan.top = std::clamp(overscan.top, 0, PPU::SCREEN_HEIGHT - 1);
        overscan.right = std::clamp(overscan.right, 0, PPU::SCREEN_WIDTH - 1);
        overscan.bottom = std::clamp(overscan.bottom, 0, PPU::SCREEN_HEIGHT - 1);
        overscan.left = std::clamp(overscan.left, 0, PPU::SCREEN_WIDTH - 1);
        if(overscan.top + overscan.bottom >= PPU::SCREEN_HEIGHT) {
            overscan.bottom = PPU::SCREEN_HEIGHT - overscan.top - 1;
        }
        if(overscan.left + overscan.right >= PPU::SCREEN_WIDTH) {
            overscan.right = PPU::SCREEN_WIDTH - overscan.left - 1;
        }

        m_overscanConfig = overscan;

        auto& video = AppSettings::instance().data.video;
        video.overscanTop = m_overscanConfig.top;
        video.overscanRight = m_overscanConfig.right;
        video.overscanBottom = m_overscanConfig.bottom;
        video.overscanLeft = m_overscanConfig.left;

        refreshModFrameCaptureHook();
        updateBuffers();
    }

    ImGui::End();
}
