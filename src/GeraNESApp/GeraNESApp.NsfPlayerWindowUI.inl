#pragma once

#ifdef ENABLE_NSF_PLAYER
inline void GeraNESApp::drawNsfPlayerWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(360.0f, 0.0f));

    if(ImGui::Begin("NSF Player", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        auto withIcon = [this](const char* icon, const char* label) {
            return m_fontAwesomeIconsLoaded ? std::string(icon) + " " + label : std::string(label);
        };

        const bool isPlaying = m_emu.nsfIsPlaying();
        const bool isPaused = m_emu.nsfIsPaused();
        const bool hasEnded = m_emu.nsfHasEnded();
        const int totalSongs = m_emu.nsfTotalSongs();
        const int currentSong = m_emu.nsfCurrentSong();
        const bool canPlay = !isPlaying || isPaused || hasEnded;
        const bool canPause = isPlaying && !isPaused && !hasEnded;
        const bool canStop = (isPlaying || isPaused) && !hasEnded;

        ImGui::BeginDisabled(!canPlay);
        if(ImGui::Button(withIcon(FontAwesomeIcons::kPlay, "Play").c_str(), ImVec2(100.0f, 0.0f))) {
            m_emu.nsfPlay();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!canPause);
        if(ImGui::Button(withIcon(FontAwesomeIcons::kPause, "Pause").c_str(), ImVec2(100.0f, 0.0f))) {
            m_emu.nsfPause();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!canStop);
        if(ImGui::Button(withIcon(FontAwesomeIcons::kStop, "Stop").c_str(), ImVec2(100.0f, 0.0f))) {
            m_emu.nsfStop();
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Text("Song %d / %d", currentSong, totalSongs);

        int selectedSong = currentSong;
        const bool hasSongs = totalSongs > 0;
        ImGui::BeginDisabled(!hasSongs);
        SetNextItemWidthScaledClamped(120.0f);
        if(ImGui::InputInt("Song", &selectedSong, 1, 1) && hasSongs) {
            if(selectedSong == currentSong + 1 || (currentSong == totalSongs && selectedSong > totalSongs)) {
                m_emu.nsfNextSong();
            }
            else if(selectedSong == currentSong - 1 || (currentSong == 1 && selectedSong < 1)) {
                m_emu.nsfPrevSong();
            }
            else {
                if(selectedSong < 1) selectedSong = totalSongs;
                if(selectedSong > totalSongs) selectedSong = 1;
                m_emu.nsfSetSong(selectedSong);
            }
        }
        ImGui::EndDisabled();
    }

    ImGui::End();
}
#endif
