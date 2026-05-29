#pragma once

namespace
{
const char* replayPortDeviceLabel(const std::optional<Settings::Device>& device)
{
    if(!device.has_value()) {
        return "Unassigned";
    }

    switch(*device) {
        case Settings::Device::CONTROLLER: return "Controller";
        case Settings::Device::ZAPPER: return "Zapper";
        case Settings::Device::ARKANOID_CONTROLLER: return "Arkanoid Controller";
        case Settings::Device::BANDAI_HYPERSHOT: return "Bandai Hyper Shot";
        case Settings::Device::SNES_MOUSE: return "SNES Mouse";
        case Settings::Device::SNES_CONTROLLER: return "SNES Controller";
        case Settings::Device::POWER_PAD_SIDE_A: return "Power Pad Side A";
        case Settings::Device::POWER_PAD_SIDE_B: return "Power Pad Side B";
        case Settings::Device::FAMICOM_CONTROLLER: return "Famicom Controller";
        case Settings::Device::SUBOR_MOUSE: return "Subor Mouse";
        case Settings::Device::NONE: return "None";
        case Settings::Device::VIRTUAL_BOY_CONTROLLER: return "Virtual Boy Controller";
    }

    return "Unknown";
}

const char* replayExpansionDeviceLabel(Settings::ExpansionDevice device)
{
    switch(device) {
        case Settings::ExpansionDevice::NONE: return "None";
        case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM: return "Standard Controller Famicom";
        case Settings::ExpansionDevice::BANDAI_HYPERSHOT: return "Bandai Hyper Shot";
        case Settings::ExpansionDevice::KONAMI_HYPERSHOT: return "Konami Hyper Shot";
        case Settings::ExpansionDevice::ARKANOID_CONTROLLER: return "Arkanoid Controller";
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A: return "Family Trainer Side A";
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B: return "Family Trainer Side B";
        case Settings::ExpansionDevice::SUBOR_KEYBOARD: return "Subor Keyboard";
        case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD: return "Family Basic Keyboard";
    }

    return "Unknown";
}

const char* replayNesMultitapLabel(Settings::NesMultitapDevice device)
{
    switch(device) {
        case Settings::NesMultitapDevice::NONE: return "None";
        case Settings::NesMultitapDevice::FOUR_SCORE: return "Four Score";
    }

    return "Unknown";
}

const char* replayFamicomMultitapLabel(Settings::FamicomMultitapDevice device)
{
    switch(device) {
        case Settings::FamicomMultitapDevice::NONE: return "None";
        case Settings::FamicomMultitapDevice::HORI_ADAPTER: return "Hori Adapter";
    }

    return "Unknown";
}

std::string replayFormatTimeHms(uint32_t frameCount, uint32_t fps)
{
    const uint32_t safeFps = std::max<uint32_t>(1u, fps);
    const uint32_t totalSeconds = frameCount / safeFps;
    const uint32_t hours = totalSeconds / 3600u;
    const uint32_t minutes = (totalSeconds / 60u) % 60u;
    const uint32_t seconds = totalSeconds % 60u;

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, seconds);
    return buffer;
}

constexpr const char* kReplayContinueRecordingPopupId = "Continue Replay Recording";
}

inline void GeraNESApp::drawReplayWindow()
{
    if(!m_showReplayWindow) {
        return;
    }

    if(ImGui::Begin("Replay", &m_showReplayWindow, ImGuiWindowFlags_MenuBar)) {
        const auto replayState = m_replayManager.snapshot();
        const bool hasRomLoaded = m_emu.valid();
        const bool recording = replayState.mode == ReplayManager::ReplayMode::Recording;
        const bool replayLoaded = replayState.loadedReplayActive;
        const bool playbackReady = replayState.mode == ReplayManager::ReplayMode::Playback && replayLoaded;
        const bool seekInProgress = m_replaySeekInProgress;
        const bool netplayRestricted = isReplayRestricted();
        const bool canContinueRecordingFromReplay =
            playbackReady && !replayState.playing && !seekInProgress;
        const bool recordEnabled =
            hasRomLoaded && !netplayRestricted && (!replayLoaded || canContinueRecordingFromReplay);
        const bool playEnabled = playbackReady && replayState.loadedFrameCount > 0 && !seekInProgress && !replayState.playing;
        const bool pauseEnabled = (recording || replayState.playing) && !seekInProgress;
        const bool closeEnabled = replayLoaded && !seekInProgress;
        const uint32_t replayFrameCount =
            recording ? replayState.loadedFrameCount : static_cast<uint32_t>(replayState.data.frames.size());
        const uint32_t replayCursorFrame = std::min(replayState.cursorFrame, replayFrameCount);
        const uint32_t replayFps = hasRomLoaded ? std::max<uint32_t>(1u, m_emu.getRegionFPS()) : 60u;
        const bool speedRestricted = isNetplaySpeedRestricted();
        const EmulationSpeed currentSpeed = effectiveEmulationSpeed(m_maxSpeedRequested);

        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem((std::string(FontAwesomeIcons::kFolderOpen) + " Load").c_str(), nullptr, false, !seekInProgress)) {
                    openReplayDialog();
                }
                if(ImGui::MenuItem((std::string(FontAwesomeIcons::kXmark) + " Close").c_str(), nullptr, false, closeEnabled)) {
                    clearReplaySession(true);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::BeginDisabled(!playEnabled);
        if(ImGui::Button((std::string(FontAwesomeIcons::kPlay) + " Play").c_str(), ImVec2(90.0f, 0.0f))) {
            startReplayPlayback();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!playbackReady);
        if(ImGui::Button((std::string(FontAwesomeIcons::kStop) + " Stop").c_str(), ImVec2(90.0f, 0.0f))) {
            (void)stopReplayToStart();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!pauseEnabled);
        if(ImGui::Button((std::string(FontAwesomeIcons::kPause) + " Pause").c_str(), ImVec2(100.0f, 0.0f))) {
            stopReplayPlayback(true);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!recordEnabled);
        const std::string recordLabel =
            std::string(FontAwesomeIcons::kRecordVinyl) + (recording ? " Recording..." : " Record");
        if(ImGui::Button(recordLabel.c_str(), ImVec2(130.0f, 0.0f))) {
            if(recording) {
                stopReplayRecording();
            } else if(canContinueRecordingFromReplay) {
                ImGui::OpenPopup(kReplayContinueRecordingPopupId);
            } else {
                startReplayRecording();
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextUnformatted("Speed");
        ImGui::SameLine();
        ImGui::BeginDisabled(speedRestricted);
        if(ImGui::Button("<", ImVec2(28.0f, 0.0f))) {
            cycleEmulationSpeedSelection(-1);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button(emulationSpeedLabel(currentSpeed), ImVec2(70.0f, 0.0f));
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(speedRestricted);
        if(ImGui::Button(">", ImVec2(28.0f, 0.0f))) {
            cycleEmulationSpeedSelection(1);
        }
        ImGui::EndDisabled();

        uint32_t sliderMax = replayFrameCount;
        if(!m_replaySliderDragging) {
            m_replaySliderValue = static_cast<int>(std::min(replayState.cursorFrame, sliderMax));
        }
        m_replaySliderValue = std::clamp(m_replaySliderValue, 0, static_cast<int>(sliderMax));
        const bool canSeek = playbackReady && !replayState.playing && !seekInProgress;
        ImGui::BeginDisabled(!canSeek || sliderMax == 0);
        ImGui::SliderInt("Position", &m_replaySliderValue, 0, static_cast<int>(sliderMax), "%d");
        if(ImGui::IsItemActive()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(
                replayFormatTimeHms(static_cast<uint32_t>(m_replaySliderValue), replayFps).c_str());
            ImGui::EndTooltip();
        }
        if(ImGui::IsItemActivated()) {
            m_replaySliderDragging = true;
        }
        if(ImGui::IsItemDeactivatedAfterEdit()) {
            m_replaySliderDragging = false;
            (void)seekReplayToFrame(static_cast<uint32_t>(m_replaySliderValue));
        }
        ImGui::EndDisabled();

        ImGui::Separator();

        const char* modeLabel = "Idle";
        if(recording) modeLabel = "Recording";
        else if(playbackReady) modeLabel = seekInProgress ? "Seeking" : (replayState.playing ? "Playback" : "Replay Loaded");

        ImGui::Text("Mode: %s", modeLabel);
        ImGui::Text("ROM: %s", replayState.data.romName.empty() ? "-" : replayState.data.romName.c_str());
        ImGui::Text("CRC: %s", replayState.data.romCrc.empty() ? "-" : replayState.data.romCrc.c_str());
        ImGui::Text("Port 1: %s", replayPortDeviceLabel(replayState.data.inputTopology.port1Device));
        ImGui::Text("Port 2: %s", replayPortDeviceLabel(replayState.data.inputTopology.port2Device));
        ImGui::Text("Expansion: %s", replayExpansionDeviceLabel(replayState.data.inputTopology.expansionDevice));
        ImGui::Text("NES Multitap: %s", replayNesMultitapLabel(replayState.data.inputTopology.nesMultitapDevice));
        ImGui::Text("Famicom Multitap: %s", replayFamicomMultitapLabel(replayState.data.inputTopology.famicomMultitapDevice));
        ImGui::Text("Time: %s/%s",
                    replayFormatTimeHms(replayCursorFrame, replayFps).c_str(),
                    replayFormatTimeHms(replayFrameCount, replayFps).c_str());
        ImGui::Text("Frames: %u", replayFrameCount);
        ImGui::Text("Cursor: %u", replayState.cursorFrame);
        ImGui::Text("File: %s", replayState.filePath.empty() ? "-" : replayState.filePath.string().c_str());
        if(seekInProgress) {
            ImGui::Text("Seeking to frame: %d", m_replaySliderValue);
        }

        if(ImGui::BeginPopupModal(kReplayContinueRecordingPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Close the loaded replay and continue recording from the current replay position?");
            ImGui::Separator();
            if(ImGui::Button("Yes", ImVec2(120.0f, 0.0f))) {
                (void)continueReplayRecordingFromCurrentCursor();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("No", ImVec2(120.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if(netplayRestricted) {
            ImGui::Separator();
            ImGui::TextUnformatted("Replay is unavailable while netplay is active.");
        } else if(!hasRomLoaded) {
            ImGui::Separator();
            ImGui::TextUnformatted("Load a ROM to record or play a replay.");
        } else if(replayLoaded && !canContinueRecordingFromReplay) {
            ImGui::Separator();
            ImGui::TextUnformatted(
                replayState.playing
                    ? "Pause the replay to continue recording from the current position."
                    : "Recording is unavailable while the replay is seeking."
            );
        } else if(canContinueRecordingFromReplay) {
            ImGui::Separator();
            ImGui::TextUnformatted("Paused replay can continue as a new recording from the current position.");
        }
    }
    ImGui::End();

    const auto replayState = m_replayManager.snapshot();
    if(m_replaySeekInProgress) {
        m_showReplayWindow = true;
        return;
    }

    if(!m_showReplayWindow && replayState.mode == ReplayManager::ReplayMode::Recording) {
        stopReplayRecording();
    } else if(!m_showReplayWindow && replayState.mode == ReplayManager::ReplayMode::Playback) {
        clearReplaySession(false);
    }
}
