#pragma once

namespace
{
const char* replayPortDeviceLabel(Settings::Device device)
{
    switch(device) {
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

    SetNextWindowCenteredOnMainViewport(ImVec2(660.0f, 380.0f));
    if(ImGui::Begin("Replay", &m_showReplayWindow, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoSavedSettings)) {
        const auto replayState = m_replaySession.snapshot();
        const auto hostReplayStatus = m_emu.replayPlaybackStatus();
        const bool hasRomLoaded = m_emu.valid();
        const bool recording = replayState.mode == ReplaySession::ReplayMode::Recording;
        const bool recordingPaused = recording && m_emu.paused();
        const bool replayLoaded = replayState.loadedReplayActive;
        const bool playbackReady = replayState.mode == ReplaySession::ReplayMode::Playback && replayLoaded;
        const bool replaySeeking = playbackReady && hostReplayStatus.seeking;
        const bool replayPlaying = playbackReady ? hostReplayStatus.playing : replayState.playing;
        const bool netplayRestricted = isReplayRestricted();
        const bool canContinueRecordingFromReplay =
            playbackReady && !replayPlaying && !replaySeeking;
        const bool recordEnabled =
            hasRomLoaded && !netplayRestricted && (!replayLoaded || canContinueRecordingFromReplay);
        const uint32_t loadedReplayFrameCount =
            playbackReady ? hostReplayStatus.loadedFrameCount : replayState.loadedFrameCount;
        const bool playEnabled = playbackReady && loadedReplayFrameCount > 0 && !replayPlaying && !replaySeeking;
        const bool pauseEnabled = (recording || replayPlaying) && !replaySeeking;
        const bool closeEnabled = replayLoaded && !replaySeeking;
        const uint32_t replayFrameCount =
            recording ? replayState.loadedFrameCount : loadedReplayFrameCount;
        const uint32_t replayCursorFrame =
            std::min(playbackReady ? hostReplayStatus.cursorFrame : replayState.cursorFrame, replayFrameCount);
        const uint32_t replayFps = hasRomLoaded ? std::max<uint32_t>(1u, m_emu.getRegionFPS()) : 60u;
        const bool speedRestricted = isNetplaySpeedRestricted();
        const EmulationSpeed currentSpeed = effectiveEmulationSpeed(m_maxSpeedRequested);
        const ImVec2 playStopButtonSize = ScaleUiSize(ImVec2(90.0f, 0.0f));
        const ImVec2 pauseButtonSize = ScaleUiSize(ImVec2(100.0f, 0.0f));
        const ImVec2 recordButtonSize = ScaleUiSize(ImVec2(130.0f, 0.0f));
        const ImVec2 speedArrowButtonSize = ScaleUiSize(ImVec2(28.0f, 0.0f));
        const ImVec2 speedLabelButtonSize = ScaleUiSize(ImVec2(70.0f, 0.0f));
        const ImVec2 confirmationButtonSize = ScaleUiSize(ImVec2(120.0f, 0.0f));

        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem((std::string(FontAwesomeIcons::kFolderOpen) + " Load Replay").c_str(), nullptr, false, !replaySeeking)) {
                    openReplayDialog();
                }
                if(ImGui::MenuItem((std::string(FontAwesomeIcons::kXmark) + " Close Replay").c_str(), nullptr, false, closeEnabled)) {
                    clearReplaySession(true);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::BeginDisabled(!playEnabled);
        if(ImGui::Button((std::string(FontAwesomeIcons::kPlay) + " Play").c_str(), playStopButtonSize)) {
            startReplayPlayback();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!playbackReady);
        if(ImGui::Button((std::string(FontAwesomeIcons::kStop) + " Stop").c_str(), playStopButtonSize)) {
            (void)stopReplayToStart();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!pauseEnabled);
        const std::string pauseLabel =
            recording
                ? std::string(FontAwesomeIcons::kPause) + (recordingPaused ? " Resume" : " Pause")
                : std::string(FontAwesomeIcons::kPause) + " Pause";
        if(ImGui::Button(pauseLabel.c_str(), pauseButtonSize)) {
            if(recording) {
                m_emu.togglePaused();
                if(recordingPaused) {
                    m_replaySession.beginPlayback();
                } else {
                    m_replaySession.stopPlayback();
                }
            } else {
                stopReplayPlayback(true);
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!recordEnabled);
        const std::string recordLabel =
            std::string(FontAwesomeIcons::kRecordVinyl) + (recording ? " Recording..." : " Record");
        if(ImGui::Button(recordLabel.c_str(), recordButtonSize)) {
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
        if(ImGui::Button("<", speedArrowButtonSize)) {
            cycleEmulationSpeedSelection(-1);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button(emulationSpeedLabel(currentSpeed), speedLabelButtonSize);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(speedRestricted);
        if(ImGui::Button(">", speedArrowButtonSize)) {
            cycleEmulationSpeedSelection(1);
        }
        ImGui::EndDisabled();

        uint32_t sliderMax = replayFrameCount;
        if(!m_replaySliderDragging && !replaySeeking) {
            m_replaySliderValue = static_cast<int>(std::min(replayCursorFrame, sliderMax));
        }
        m_replaySliderValue = std::clamp(m_replaySliderValue, 0, static_cast<int>(sliderMax));
        const bool canSeek = playbackReady && !replayPlaying && !replaySeeking;
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
        ImGui::Text("Cursor: %u/%u", replayCursorFrame, replayFrameCount);
        ImGui::Text("File: %s", replayState.filePath.empty() ? "-" : replayState.filePath.string().c_str());
        if(replaySeeking) {
            ImGui::Text("Seeking to frame: %d", m_replaySliderValue);
        }
        if(ImGui::BeginPopupModal(kReplayContinueRecordingPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Close the loaded replay and continue recording from the current replay position?");
            ImGui::Separator();
            if(ImGui::Button("Yes", confirmationButtonSize)) {
                (void)continueReplayRecordingFromCurrentCursor();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("No", confirmationButtonSize)) {
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
                replaySeeking
                    ? "Replay seek is in progress."
                    : replayPlaying
                    ? "Pause the replay to continue recording from the current position."
                    : "Pause the replay to continue recording from the current position."
            );
        } else if(canContinueRecordingFromReplay) {
            ImGui::Separator();
            ImGui::TextUnformatted("Paused replay can continue as a new recording from the current position.");
        }
    }
    ImGui::End();

    if(m_emu.replayPlaybackStatus().seeking) {
        m_showReplayWindow = true;
        return;
    }

    const auto replayState = m_replaySession.snapshot();
    if(!m_showReplayWindow && replayState.mode == ReplaySession::ReplayMode::Recording) {
        m_selectedEmulationSpeed = EmulationSpeed::Normal;
        m_maxSpeedRequested = false;
        resetEmulationSpeedPacing();
        stopReplayRecording();
    } else if(!m_showReplayWindow && replayState.mode == ReplaySession::ReplayMode::Playback) {
        m_selectedEmulationSpeed = EmulationSpeed::Normal;
        m_maxSpeedRequested = false;
        resetEmulationSpeedPacing();
        clearReplaySession(false);
    }
}
