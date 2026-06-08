#pragma once

inline void GeraNESApp::menuBar() {
    bool show_menu = true;
    const bool netplayClientRestricted = isNetplayClientRestricted();
    const bool netplayRomChangeRestricted = isNetplayRomChangeRestricted();
    const bool replayInteractionLocked = isReplaySessionInteractionLocked();
    const bool replayRecordingActive = isReplayRecordingActive();
    const bool usingCustomChrome = useCustomWindowChrome();
    const ImVec4 menuBarColor = ImGuiTheme::chromeMenuBar();
    bool menuBarVisible = false;
    bool menuHostBegun = false;

    if(show_menu) {
        if(usingCustomChrome) {
            ImGuiViewport* mainViewport = ImGui::GetMainViewport();
            const float menuHeight = ImGui::GetFrameHeight();
            ImGui::SetNextWindowViewport(mainViewport->ID);
            ImGui::SetNextWindowPos(ImVec2(mainViewport->Pos.x, mainViewport->Pos.y + customTitleBarHeight()));
            ImGui::SetNextWindowSize(ImVec2(mainViewport->Size.x, menuHeight));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(menuBarColor.x, menuBarColor.y, menuBarColor.z, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_MenuBarBg, menuBarColor);
            menuHostBegun = ImGui::Begin(
                "##GeraNESMainMenuHost",
                nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_MenuBar
            );
            if(menuHostBegun) {
                menuBarVisible = ImGui::BeginMenuBar();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_MenuBarBg, menuBarColor);
            menuBarVisible = ImGui::BeginMainMenuBar();
        }
    }

    if(menuBarVisible) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
        auto withMenuIcon = [this](const char* icon, const char* label) {
            return m_fontAwesomeIconsLoaded ? std::string(icon) + " " + label : std::string(label);
        };
        auto withMenuIconText = [this](const char* icon, const std::string& label) {
            return m_fontAwesomeIconsLoaded ? std::string(icon) + " " + label : label;
        };
        auto beginTopMenu = [](const char* label, bool enabled = true) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGuiTheme::textOnAccent());
            const bool open = ImGui::BeginMenu(label, enabled);
            ImGui::PopStyleColor();
            return open;
        };

        if (beginTopMenu(withMenuIcon(FontAwesomeIcons::kFile, "File").c_str()))
        {
            const bool hasRomLoaded = m_emu.valid();
            auto sc = m_shortcuts.get("openRom");
            if( sc != nullptr) {

                if (ImGui::MenuItem(withMenuIconText(FontAwesomeIcons::kFolderOpen, sc->label).c_str(), sc->shortcut.c_str(), false, !netplayRomChangeRestricted && !replayInteractionLocked))
                {
                    sc->action();
                }
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kXmark, "Close ROM").c_str(), nullptr, false, hasRomLoaded && !netplayRomChangeRestricted && !replayInteractionLocked)) {
                closeRomAction();
            }

            #ifdef __EMSCRIPTEN__
            if (ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kDatabase, "Session").c_str())) {

                if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kDownload, "Export").c_str())) {
                    AppSettings::instance().save();
                    emcriptenExportSession();
                }

                if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kFolderOpen, "Import").c_str(), nullptr, false, !netplayRomChangeRestricted && !replayInteractionLocked)) {
                    emcriptenImportSession(reinterpret_cast<intptr_t>(this));
                }

                ImGui::EndMenu();
            }
            #endif

            auto recentFiles = AppSettings::instance().data.getRecentFiles();
            if (ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kClockRotateLeft, "Recent Files").c_str(), recentFiles.size() > 0 && !netplayRomChangeRestricted && !replayInteractionLocked))
            {
                for(size_t i = 0; i < recentFiles.size(); ++i) {
                    const float recentFileLabelWidth = std::max(
                        80.0f,
                        ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f
                    );
#ifdef __EMSCRIPTEN__
                    const std::string displayName = fs::path(recentFiles[i]).filename().string();
                    const std::string menuLabel = BuildEllipsizedMenuLabel(
                        displayName.empty() ? recentFiles[i] : displayName,
                        recentFiles[i],
                        recentFileLabelWidth
                    );
                    if(ImGui::MenuItem(menuLabel.c_str())) {
#elif defined(__ANDROID__)
                    std::string androidRecentUri;
                    std::string androidRecentDisplayName;
                    if(parseAndroidRecentFileEntry(recentFiles[i], androidRecentUri, androidRecentDisplayName)) {
                        const std::string menuLabel = BuildEllipsizedMenuLabel(
                            androidRecentUri,
                            recentFiles[i],
                            recentFileLabelWidth
                        );
                        if(ImGui::MenuItem(menuLabel.c_str())) {
                            AndroidFileDialog::PickedFile pickedRom;
                            std::string error;
                            if(AndroidFileDialog::copyDocumentUriToCache(androidRecentUri, pickedRom, &error)) {
                                m_androidLastOpenedDocumentUri = pickedRom.uri;
                                m_androidLastOpenedDocumentDisplayName = pickedRom.displayName;
                                openFile(pickedRom.cachePath.c_str());
                            } else if(!error.empty()) {
                                Logger::instance().log("Failed to reopen recent Android document: " + error, Logger::Type::ERROR);
                            }
                        }
                    } else {
                        const std::string displayName = fs::path(recentFiles[i]).filename().string();
                        const std::string menuLabel = BuildEllipsizedMenuLabel(
                            displayName.empty() ? recentFiles[i] : displayName,
                            recentFiles[i],
                            recentFileLabelWidth
                        );
                        if(ImGui::MenuItem(menuLabel.c_str())) {
                            openFile(recentFiles[i].c_str());
                        }
                    }
#else
                    const std::string menuLabel = BuildEllipsizedMenuLabel(
                        recentFiles[i],
                        recentFiles[i],
                        recentFileLabelWidth
                    );
                    if(ImGui::MenuItem(menuLabel.c_str())) {
                        openFile(recentFiles[i].c_str());
                    }
#endif
                }
                ImGui::EndMenu();
            }

            sc = m_shortcuts.get("quit");
            if( sc != nullptr) {
                ImGui::Separator();

                if (ImGui::MenuItem(withMenuIconText(FontAwesomeIcons::kRightFromBracket, sc->label).c_str(), sc->shortcut.c_str()))
                {
                    sc->action();
                }
            }

            ImGui::EndMenu();
        }

        if (beginTopMenu(withMenuIcon(FontAwesomeIcons::kCalculator, "Emulator").c_str(), !netplayClientRestricted))
        {
            const bool hasRomLoaded = m_emu.valid();
            int& saveStateSlot = AppSettings::instance().data.saveStateSlot;
            auto sc = m_shortcuts.get("saveState");
            if( sc != nullptr) {

                if (ImGui::MenuItem(withMenuIconText(FontAwesomeIcons::kFloppyDisk, sc->label).c_str(), sc->shortcut.c_str(), false, hasRomLoaded && !replayInteractionLocked))
                {
                    sc->action();
                }
            }

            sc = m_shortcuts.get("loadState");
            if( sc != nullptr) {

                if (ImGui::MenuItem(withMenuIconText(FontAwesomeIcons::kFolderOpen, sc->label).c_str(), sc->shortcut.c_str(), false, hasRomLoaded && !replayRecordingActive))
                {
                    sc->action();
                }
            }

            if(ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kFloppyDisk, "Slot").c_str(), !replayInteractionLocked)) {
                for(int slot = 0; slot <= 9; ++slot) {
                    const std::string label = std::to_string(slot);
                    if(ImGui::MenuItem(label.c_str(), nullptr, saveStateSlot == slot)) {
                        saveStateSlot = slot;
                        Logger::instance().log(
                            "Save slot set to " + std::to_string(slot),
                            Logger::Type::USER
                        );
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kGlobe, "Region").c_str(), hasRomLoaded)) {

                if(ImGui::MenuItem("NTSC", nullptr, m_emu.region() == Settings::Region::NTSC)) {
                    m_emu.setRegion(Settings::Region::NTSC);
                }

                if(ImGui::MenuItem("PAL", nullptr, m_emu.region() == Settings::Region::PAL)) {
                    m_emu.setRegion(Settings::Region::PAL);
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            auto pauseShortcut = m_shortcuts.get("pause");
            const char* pauseKey = (pauseShortcut != nullptr) ? pauseShortcut->shortcut.c_str() : nullptr;
            const bool netplayPauseActive = canUseNetplaySessionPause();
            const bool pauseSelected = netplayPauseActive
                ? (m_netplayRuntime.uiSnapshot().room.state == ConsoleNetplay::SessionState::Paused)
                : m_emu.paused();
            const bool debugOwnsFlowControl = AppSettings::instance().data.debug.cpuDebuggerEnabled;
            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kPause, "Pause").c_str(), pauseKey, pauseSelected, hasRomLoaded && !isNetplayPauseRestricted() && !debugOwnsFlowControl)) {
                togglePauseAction();
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kRotateRight, "Reset").c_str(), nullptr, false, hasRomLoaded && !netplayClientRestricted && !isReplayResetRestricted())) {
                resetAction();
            }

            ImGui::Separator();

            if (ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kWandMagicSparkles, "Improvements").c_str())) {
                m_showImprovementsWindow = true;
            }

            if(ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kSliders, "Speed").c_str(), hasRomLoaded)) {
                const bool speedRestricted = isNetplaySpeedRestricted();
                const EmulationSpeed currentSpeed = effectiveEmulationSpeed(false);
                const auto& speeds = emulationSpeedValues();
                for(EmulationSpeed speed : speeds) {
                    const bool optionEnabled = !speedRestricted || speed == EmulationSpeed::Normal;
                    if(ImGui::MenuItem(emulationSpeedLabel(speed), nullptr, currentSpeed == speed, optionEnabled)) {
                        m_selectedEmulationSpeed = speed;
                        resetEmulationSpeedPacing();
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (beginTopMenu(withMenuIcon(FontAwesomeIcons::kSliders, "Options").c_str()))
        {
            if (ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kTv, "Video").c_str()))
            {
                if (ImGui::BeginMenu("VSync")) {

                    for(int i = OFF; i <= ADAPTATIVE ; i++) {
                        if(ImGui::MenuItem(VSYNC_TYPE_LABELS[i], nullptr, m_vsyncMode == i)) {
                            m_vsyncMode = (VSyncMode)i;
                            AppSettings::instance().data.video.vsyncMode = i;
                            updateVSyncConfig();
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Filter")) {

                    for(int i = NEAREST; i <= BILINEAR ; i++) {
                        if(ImGui::MenuItem(FILTER_TYPE_LABELS[i], nullptr, m_filterMode == i)) {
                            m_filterMode = (FilterMode)i;
                            AppSettings::instance().data.video.filterMode = i;
                            updateFilterConfig();
                        }
                    }
                    ImGui::EndMenu();
                }

                if(ImGui::MenuItem("Shader")) {
                    m_showShaderWindow = true;
                }

                if (ImGui::MenuItem("Palette")) {
                    m_showPaletteWindow = true;
                }

                ImGui::Separator();

                if(ImGui::MenuItem("Overscan")) {
                    m_showOverscanWindow = true;
                }

                if (ImGui::BeginMenu("Scale Mode")) {
                    auto setScaleMode = [this](VideoScaleMode mode) {
                        m_videoScaleMode = mode;
                        AppSettings::instance().data.video.scaleMode = static_cast<int>(mode);
                        AppSettings::instance().data.video.horizontalStretch = m_videoScaleMode == STRETCH_TO_FILL;
                        m_updateObjectsFlag = true;
                    };

                    if(ImGui::MenuItem(VIDEO_SCALE_MODE_LABELS[ASPECT_FIT], nullptr, m_videoScaleMode == ASPECT_FIT)) {
                        setScaleMode(ASPECT_FIT);
                    }
                    if(ImGui::MenuItem(VIDEO_SCALE_MODE_LABELS[STRETCH_TO_FILL], nullptr, m_videoScaleMode == STRETCH_TO_FILL)) {
                        setScaleMode(STRETCH_TO_FILL);
                    }
                    if (ImGui::BeginMenu("Pixel Perfect")) {
                        if(ImGui::MenuItem(VIDEO_SCALE_MODE_LABELS[PIXEL_PERFECT_BEST_FIT], nullptr, m_videoScaleMode == PIXEL_PERFECT_BEST_FIT)) {
                            setScaleMode(PIXEL_PERFECT_BEST_FIT);
                        }

                        ImGui::Separator();

                        ImGui::TextUnformatted("Scale");
                        ImGui::SameLine();
                        if(ImGui::Button("-")) {
                            m_pixelPerfectScale = std::max(1, m_pixelPerfectScale - 1);
                            AppSettings::instance().data.video.pixelPerfectScale = m_pixelPerfectScale;
                            setScaleMode(PIXEL_PERFECT);
                        }
                        ImGui::SameLine();
                        const std::string selectedScaleLabel = std::to_string(m_pixelPerfectScale) + "x";
                        if(ImGui::Button(selectedScaleLabel.c_str())) {
                            AppSettings::instance().data.video.pixelPerfectScale = m_pixelPerfectScale;
                            setScaleMode(PIXEL_PERFECT);
                        }
                        ImGui::SameLine();
                        if(ImGui::Button("+")) {
                            m_pixelPerfectScale = std::min(16, m_pixelPerfectScale + 1);
                            AppSettings::instance().data.video.pixelPerfectScale = m_pixelPerfectScale;
                            setScaleMode(PIXEL_PERFECT);
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                auto sc = m_shortcuts.get("fullscreen");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), isFullScreen()))
                    {
                        sc->action();
                    }
                }

                #ifndef __EMSCRIPTEN__
                if (ImGui::BeginMenu("Fullscreen Mode")) {
                    for(int i = 0; i < static_cast<int>(FULLSCREEN_MODE_LABELS.size()); ++i) {
                        if(ImGui::MenuItem(FULLSCREEN_MODE_LABELS[static_cast<size_t>(i)], nullptr, m_fullScreenMode == i)) {
                            m_fullScreenMode = i;
                            AppSettings::instance().data.video.fullScreenMode = i;
                            if(isFullScreen()) {
                                setFullScreen(false);
                                setFullScreen(true, m_fullScreenMode == 1);
                                updateVSyncConfig();
                                m_mainLoopLastCounter = SDL_GetPerformanceCounter();
                                m_mainLoopCounterFrequency = SDL_GetPerformanceFrequency();
                                m_mainLoopCounterRemainder = 0;
                                m_presenterFrameAccumScaled = 0;
                                m_presenterStepRemainder = 0;
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                #endif

                ImGui::Separator();
                if (ImGui::MenuItem("Show FPS", nullptr, &AppSettings::instance().data.debug.showFps))
                {
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kVolumeHigh, "Audio").c_str()))
            {
                if (ImGui::BeginMenu("Device")) {

                    for(size_t i = 0; i < m_audioDevices.size(); ++i) {

                        bool checked = m_emu.currentAudioDeviceName() == m_audioDevices[i];

                        if(ImGui::MenuItem(m_audioDevices[i].c_str(), nullptr, checked)) {
                            AppSettings::instance().data.audio.audioDevice = m_audioDevices[i];
                            m_emu.configAudioDevice(
                                m_audioDevices[i],
                                AppSettings::instance().data.audio.sampleRate,
                                AppSettings::instance().data.audio.sampleSize
                            );
                        }
                    }
                    ImGui::EndMenu();
                }

                const auto audioFormatOptions = m_emu.getAudioFormatOptions(m_emu.currentAudioDeviceName());
                const int currentSampleRate = m_emu.currentAudioSampleRate();
                const int currentSampleSize = m_emu.currentAudioSampleSize();

                if(ImGui::BeginMenu("Sample Rate", !audioFormatOptions.sampleRates.empty())) {
                    for(int rate : audioFormatOptions.sampleRates) {
                        const std::string label = std::to_string(rate) + " Hz";
                        const bool checked = currentSampleRate == rate;
                        if(ImGui::MenuItem(label.c_str(), nullptr, checked)) {
                            AppSettings::instance().data.audio.sampleRate = rate;
                            m_emu.configAudioDevice(
                                AppSettings::instance().data.audio.audioDevice,
                                AppSettings::instance().data.audio.sampleRate,
                                AppSettings::instance().data.audio.sampleSize
                            );
                        }
                    }
                    ImGui::EndMenu();
                }

                if(ImGui::BeginMenu("Sample Size", !audioFormatOptions.sampleSizes.empty())) {
                    for(int size : audioFormatOptions.sampleSizes) {
                        const std::string label = std::to_string(size) + "-bit";
                        const bool checked = currentSampleSize == size;
                        if(ImGui::MenuItem(label.c_str(), nullptr, checked)) {
                            AppSettings::instance().data.audio.sampleSize = size;
                            m_emu.configAudioDevice(
                                AppSettings::instance().data.audio.audioDevice,
                                AppSettings::instance().data.audio.sampleRate,
                                AppSettings::instance().data.audio.sampleSize
                            );
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::BeginMenu("Channels"))
                {
                    drawAudioChannelDebugControls();
                    ImGui::EndMenu();
                }

                float volumePercent = m_emu.getAudioVolume() * 100.0f;
                if(ImGui::SliderFloat("Volume", &volumePercent, 0.0f, 100.0f, "%.0f%%")) {
                    float volume = volumePercent / 100.0f;
                    m_emu.setAudioVolume(volume);
                    AppSettings::instance().data.audio.volume = volume;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kGamepad , "Input").c_str()))
            {
            const auto replayState = m_replaySession.snapshot();
            const bool replayInputLocked =
                replayState.mode == ReplaySession::ReplayMode::Recording ||
                replayState.loadedReplayActive;
            const auto replayTopology = replayState.data.inputTopology;
#ifndef __EMSCRIPTEN__
            const auto netplaySnapshot = GeraNESNetplay::menuSnapshot(m_netplayRuntime);
            const auto localInputTopology = m_inputTopology;
            const bool netplayInputManaged = netplaySnapshot.inputManaged;
            const auto& localNetplayAssignments = netplaySnapshot.localAssignments;
            const bool canChangeNetplayManagedInput = !netplayInputManaged;
            const Settings::Device effectivePort1Device = replayInputLocked
                ? replayTopology.port1Device
                : netplayInputManaged
                ? netplaySnapshot.port1Device.value_or(Settings::Device::NONE)
                : localInputTopology.port1Device;
            const Settings::Device effectivePort2Device = replayInputLocked
                ? replayTopology.port2Device
                : netplayInputManaged
                ? netplaySnapshot.port2Device.value_or(Settings::Device::NONE)
                : localInputTopology.port2Device;
            const auto effectiveExpansionDevice = replayInputLocked
                ? replayTopology.expansionDevice
                : netplayInputManaged
                ? netplaySnapshot.expansionDevice
                : localInputTopology.expansionDevice;
            const auto effectiveNesMultitapDevice = replayInputLocked
                ? replayTopology.nesMultitapDevice
                : netplayInputManaged
                ? netplaySnapshot.nesMultitapDevice
                : localInputTopology.nesMultitapDevice;
            const auto effectiveFamicomMultitapDevice = replayInputLocked
                ? replayTopology.famicomMultitapDevice
                : netplayInputManaged
                ? netplaySnapshot.famicomMultitapDevice
                : localInputTopology.famicomMultitapDevice;
#else
            const bool netplayInputManaged = false;
            const std::vector<ConsoleNetplay::PlayerSlot> localNetplayAssignments;
            const bool canChangeNetplayManagedInput = true;
            const auto localInputTopology = m_inputTopology;
            const auto effectivePort1Device = replayInputLocked ? replayTopology.port1Device : localInputTopology.port1Device;
            const auto effectivePort2Device = replayInputLocked ? replayTopology.port2Device : localInputTopology.port2Device;
            const auto effectiveExpansionDevice = replayInputLocked ? replayTopology.expansionDevice : localInputTopology.expansionDevice;
            const auto effectiveNesMultitapDevice = replayInputLocked ? replayTopology.nesMultitapDevice : localInputTopology.nesMultitapDevice;
            const auto effectiveFamicomMultitapDevice = replayInputLocked ? replayTopology.famicomMultitapDevice : localInputTopology.famicomMultitapDevice;
#endif
            const bool nesFourScoreEnabled = effectiveNesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE;
            const bool famicomHoriEnabled = effectiveFamicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER;
            const bool anyMultitapActive = nesFourScoreEnabled || famicomHoriEnabled;
            const bool topologyEditingEnabled = canChangeNetplayManagedInput && !replayInputLocked;

            auto effectivePortDeviceFor = [&](Settings::Port port)
            {
                return port == Settings::Port::P_1 ? effectivePort1Device : effectivePort2Device;
            };

            auto applyMenuTopology = [this](const InputTopology& topology)
            {
                applyInputTopology(topology);
            };

            auto drawControllerPortMenuItem =
                [this,
                 &effectivePortDeviceFor,
                 effectiveExpansionDevice,
                 effectiveNesMultitapDevice,
                 effectiveFamicomMultitapDevice,
                 applyMenuTopology,
                 topologyEditingEnabled](const char* label, Settings::Port port, Settings::Device device)
            {
                const bool selected = effectivePortDeviceFor(port) == device;
                if(ImGui::MenuItem(label, nullptr, selected, topologyEditingEnabled)) {
                    InputTopology topology;
                    topology.port1Device =
                        port == Settings::Port::P_1 ? device : effectivePortDeviceFor(Settings::Port::P_1);
                    topology.port2Device =
                        port == Settings::Port::P_2 ? device : effectivePortDeviceFor(Settings::Port::P_2);
                    topology.expansionDevice = effectiveExpansionDevice;
                    topology.nesMultitapDevice = effectiveNesMultitapDevice;
                    topology.famicomMultitapDevice = effectiveFamicomMultitapDevice;
                    applyMenuTopology(topology);
                }
            };

            auto drawControllerPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                const auto device = effectivePortDeviceFor(port);
                if(device != Settings::Device::CONTROLLER &&
                   device != Settings::Device::FAMICOM_CONTROLLER) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    const bool famicomController = device == Settings::Device::FAMICOM_CONTROLLER;
                    if(port == Settings::Port::P_1) m_inputBindingConfigWindow.show(famicomController ? "Famicom Controller 1" : "Standard Controller 1", m_controller1);
                    else m_inputBindingConfigWindow.show(famicomController ? "Famicom Controller 2" : "Standard Controller 2", m_controller2);
                }
            };

            auto drawArkanoidPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                if(effectivePortDeviceFor(port) != Settings::Device::ARKANOID_CONTROLLER) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    m_showArkanoidNesConfigWindow = true;
                }
            };

            auto drawSnesMousePortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                const auto device = effectivePortDeviceFor(port);
                if(device != Settings::Device::SNES_MOUSE &&
                   device != Settings::Device::SUBOR_MOUSE) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    m_showSnesMouseConfigWindow = true;
                }
            };

            auto drawPowerPadPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                const auto device = effectivePortDeviceFor(port);
                if(device != Settings::Device::POWER_PAD_SIDE_A &&
                   device != Settings::Device::POWER_PAD_SIDE_B) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    m_powerPadConfigWindow.show("Power Pad Config", m_powerPadInfo);
                }
            };

            auto drawSnesControllerPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                if(effectivePortDeviceFor(port) != Settings::Device::SNES_CONTROLLER) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    if(port == Settings::Port::P_1) m_inputBindingConfigWindow.show("SNES Controller 1", m_snesController1);
                    else m_inputBindingConfigWindow.show("SNES Controller 2", m_snesController2);
                }
            };

            auto drawVirtualBoyControllerPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                if(effectivePortDeviceFor(port) != Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    if(port == Settings::Port::P_1) m_inputBindingConfigWindow.show("Virtual Boy Controller 1", m_virtualBoyController1);
                    else m_inputBindingConfigWindow.show("Virtual Boy Controller 2", m_virtualBoyController2);
                }
            };

            auto drawKonamiHyperShotConfigItem = [this, effectiveExpansionDevice](bool enabled)
            {
                if(effectiveExpansionDevice != Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    m_showKonamiHyperShotConfigWindow = true;
                }
            };

            auto drawMultitapControllerConfigItem = [this](const char* label, ControllerInfo& info, bool enabled)
            {
                if(ImGui::MenuItem(label, nullptr, false, enabled)) {
                    m_inputBindingConfigWindow.show(label, info);
                }
            };

            auto localHasNetplayAssignment = [&](ConsoleNetplay::PlayerSlot slot) {
                return std::find(localNetplayAssignments.begin(), localNetplayAssignments.end(), slot) != localNetplayAssignments.end();
            };
            const bool allowPort1Config = !netplayInputManaged || localHasNetplayAssignment(GeraNESNetplay::kPort1PlayerSlot);
            const bool allowPort2Config = !netplayInputManaged || localHasNetplayAssignment(GeraNESNetplay::kPort2PlayerSlot);
            const bool allowExpansionConfig = !netplayInputManaged || localHasNetplayAssignment(GeraNESNetplay::kExpansionPlayerSlot);
            const bool allowMultitapP1Config = !netplayInputManaged || localHasNetplayAssignment(GeraNESNetplay::kMultitapP1PlayerSlot);
            const bool allowMultitapP2Config = !netplayInputManaged || localHasNetplayAssignment(GeraNESNetplay::kMultitapP2PlayerSlot);
            const bool allowMultitapP3Config = !netplayInputManaged || localHasNetplayAssignment(GeraNESNetplay::kMultitapP3PlayerSlot);
            const bool allowMultitapP4Config = !netplayInputManaged || localHasNetplayAssignment(GeraNESNetplay::kMultitapP4PlayerSlot);

            bool automaticOnRomLoad = AppSettings::instance().data.input.automaticOnRomLoad;
            if(ImGui::MenuItem("Automatic on ROM load", nullptr, automaticOnRomLoad)) {
                AppSettings::instance().data.input.automaticOnRomLoad = !automaticOnRomLoad;
            }

            bool showMiniatures = AppSettings::instance().data.input.showMiniatures;
            if(ImGui::MenuItem("Show Miniatures", nullptr, showMiniatures)) {
                AppSettings::instance().data.input.showMiniatures = !showMiniatures;
            }

            ImGui::BeginDisabled(replayInputLocked);

            ImGui::Separator();

            if (ImGui::BeginMenu("Port 1", !anyMultitapActive && (!netplayInputManaged || allowPort1Config))) {
                drawControllerPortMenuItem("None", Settings::Port::P_1, Settings::Device::NONE);
                drawControllerPortMenuItem("Standard Controller", Settings::Port::P_1, Settings::Device::CONTROLLER);
                drawControllerPortMenuItem("Famicom Controller", Settings::Port::P_1, Settings::Device::FAMICOM_CONTROLLER);
                drawControllerPortMenuItem("Zapper", Settings::Port::P_1, Settings::Device::ZAPPER);
                drawControllerPortMenuItem("Power Pad (Side A)", Settings::Port::P_1, Settings::Device::POWER_PAD_SIDE_A);
                drawControllerPortMenuItem("Power Pad (Side B)", Settings::Port::P_1, Settings::Device::POWER_PAD_SIDE_B);
                drawControllerPortMenuItem("SNES Mouse", Settings::Port::P_1, Settings::Device::SNES_MOUSE);
                drawControllerPortMenuItem("Subor Mouse", Settings::Port::P_1, Settings::Device::SUBOR_MOUSE);
                drawControllerPortMenuItem("SNES Controller", Settings::Port::P_1, Settings::Device::SNES_CONTROLLER);
                drawControllerPortMenuItem("Virtual Boy Controller", Settings::Port::P_1, Settings::Device::VIRTUAL_BOY_CONTROLLER);
                drawControllerPortMenuItem("Arkanoid Controller", Settings::Port::P_1, Settings::Device::ARKANOID_CONTROLLER);
                drawControllerPortConfigItem(Settings::Port::P_1, allowPort1Config);
                drawPowerPadPortConfigItem(Settings::Port::P_1, allowPort1Config);
                drawSnesMousePortConfigItem(Settings::Port::P_1, allowPort1Config);
                drawSnesControllerPortConfigItem(Settings::Port::P_1, allowPort1Config);
                drawVirtualBoyControllerPortConfigItem(Settings::Port::P_1, allowPort1Config);
                drawArkanoidPortConfigItem(Settings::Port::P_1, allowPort1Config);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Port 2", !anyMultitapActive && (!netplayInputManaged || allowPort2Config))) {
                drawControllerPortMenuItem("None", Settings::Port::P_2, Settings::Device::NONE);
                drawControllerPortMenuItem("Standard Controller", Settings::Port::P_2, Settings::Device::CONTROLLER);
                drawControllerPortMenuItem("Famicom Controller", Settings::Port::P_2, Settings::Device::FAMICOM_CONTROLLER);
                drawControllerPortMenuItem("Zapper", Settings::Port::P_2, Settings::Device::ZAPPER);
                drawControllerPortMenuItem("Power Pad (Side A)", Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_A);
                drawControllerPortMenuItem("Power Pad (Side B)", Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_B);
                drawControllerPortMenuItem("SNES Mouse", Settings::Port::P_2, Settings::Device::SNES_MOUSE);
                drawControllerPortMenuItem("Subor Mouse", Settings::Port::P_2, Settings::Device::SUBOR_MOUSE);
                drawControllerPortMenuItem("SNES Controller", Settings::Port::P_2, Settings::Device::SNES_CONTROLLER);
                drawControllerPortMenuItem("Virtual Boy Controller", Settings::Port::P_2, Settings::Device::VIRTUAL_BOY_CONTROLLER);
                drawControllerPortMenuItem("Arkanoid Controller", Settings::Port::P_2, Settings::Device::ARKANOID_CONTROLLER);
                drawControllerPortConfigItem(Settings::Port::P_2, allowPort2Config);
                drawPowerPadPortConfigItem(Settings::Port::P_2, allowPort2Config);
                drawSnesMousePortConfigItem(Settings::Port::P_2, allowPort2Config);
                drawSnesControllerPortConfigItem(Settings::Port::P_2, allowPort2Config);
                drawVirtualBoyControllerPortConfigItem(Settings::Port::P_2, allowPort2Config);
                drawArkanoidPortConfigItem(Settings::Port::P_2, allowPort2Config);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Expansion", !anyMultitapActive && (!netplayInputManaged || allowExpansionConfig))) {
                if (ImGui::MenuItem("None", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::NONE, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::NONE,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Standard Controller (Famicom)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Bandai Hyper Shot", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::BANDAI_HYPERSHOT,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Konami Hyper Shot", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::KONAMI_HYPERSHOT,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Family Trainer (Side A)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Family Trainer (Side B)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Subor Keyboard", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::SUBOR_KEYBOARD,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Family Basic Keyboard", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if (ImGui::MenuItem("Arkanoid Controller (Famicom)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER, topologyEditingEnabled))
                {
                    applyMenuTopology({
                        effectivePort1Device,
                        effectivePort2Device,
                        Settings::ExpansionDevice::ARKANOID_CONTROLLER,
                        effectiveNesMultitapDevice,
                        effectiveFamicomMultitapDevice
                    });
                }
                if(effectiveExpansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                    ImGui::Separator();
                    if(ImGui::MenuItem("Config...", nullptr, false, allowExpansionConfig)) {
                        m_showArkanoidFamicomConfigWindow = true;
                    }
                }
                if(effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                   effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                    ImGui::Separator();
                    if(ImGui::MenuItem("Config...", nullptr, false, allowExpansionConfig)) {
                        m_powerPadConfigWindow.show("Family Trainer Config", m_powerPadInfo);
                    }
                }
                if(effectiveExpansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
                    ImGui::Separator();
                    if(ImGui::MenuItem("Config...", nullptr, false, allowExpansionConfig)) {
                        m_inputBindingConfigWindow.show("Standard Controller 3", m_controller3);
                    }
                }
                drawKonamiHyperShotConfigItem(allowExpansionConfig);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Multitap", !netplayInputManaged || anyMultitapActive)) {
                if (ImGui::BeginMenu("NES")) {
                    if (ImGui::BeginMenu("Four Score")) {
                        if(ImGui::MenuItem("Enabled", nullptr, nesFourScoreEnabled, topologyEditingEnabled)) {
                            const Settings::NesMultitapDevice nextNesMultitap =
                                nesFourScoreEnabled ? Settings::NesMultitapDevice::NONE : Settings::NesMultitapDevice::FOUR_SCORE;
                            applyMenuTopology({
                                effectivePort1Device,
                                effectivePort2Device,
                                effectiveExpansionDevice,
                                nextNesMultitap,
                                nextNesMultitap == Settings::NesMultitapDevice::NONE
                                    ? effectiveFamicomMultitapDevice
                                    : Settings::FamicomMultitapDevice::NONE
                            });
                        }

                        if(nesFourScoreEnabled) {
                            ImGui::Separator();
                            drawMultitapControllerConfigItem("Standard Controller 1", m_controller1, allowMultitapP1Config);
                            drawMultitapControllerConfigItem("Standard Controller 2", m_controller2, allowMultitapP2Config);
                            drawMultitapControllerConfigItem("Standard Controller 3", m_controller3, allowMultitapP3Config);
                            drawMultitapControllerConfigItem("Standard Controller 4", m_controller4, allowMultitapP4Config);
                        }

                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Famicom")) {
                    if (ImGui::BeginMenu("Hori Adapter")) {
                        if(ImGui::MenuItem("Enabled", nullptr, famicomHoriEnabled, topologyEditingEnabled)) {
                            const Settings::FamicomMultitapDevice nextFamicomMultitap =
                                famicomHoriEnabled ? Settings::FamicomMultitapDevice::NONE : Settings::FamicomMultitapDevice::HORI_ADAPTER;
                            applyMenuTopology({
                                effectivePort1Device,
                                effectivePort2Device,
                                effectiveExpansionDevice,
                                nextFamicomMultitap == Settings::FamicomMultitapDevice::NONE
                                    ? effectiveNesMultitapDevice
                                    : Settings::NesMultitapDevice::NONE,
                                nextFamicomMultitap
                            });
                        }

                        if(famicomHoriEnabled) {
                            ImGui::Separator();
                            drawMultitapControllerConfigItem("Standard Controller 1", m_controller1, allowMultitapP1Config);
                            drawMultitapControllerConfigItem("Standard Controller 2", m_controller2, allowMultitapP2Config);
                            drawMultitapControllerConfigItem("Standard Controller 3", m_controller3, allowMultitapP3Config);
                            drawMultitapControllerConfigItem("Standard Controller 4", m_controller4, allowMultitapP4Config);
                        }

                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

            ImGui::EndMenu();
        }
        if(ImGui::MenuItem("System")) {
            m_inputBindingConfigWindow.show("System", m_systemInput);
        }

        if(replayInputLocked) {
            ImGui::Separator();
            ImGui::TextUnformatted("Replay locks the input topology for the full session.");
        }

        ImGui::EndDisabled();

        const GameDatabase::System cartridgeSystem = m_emu.currentCartridgeSystem();
        const bool isFdsRom = cartridgeSystem == GameDatabase::System::FDS;
        const bool isVsRom = cartridgeSystem == GameDatabase::System::VsSystem;
        const bool hasHardwareActions = m_emu.valid() && (isFdsRom || isVsRom);

            if (ImGui::BeginMenu("Hardware", hasHardwareActions && !netplayClientRestricted)) {
                if (ImGui::MenuItem("FDS - Switch Disk Side", nullptr, false, isFdsRom && !netplayClientRestricted)) {
                    m_emu.fdsSwitchDiskSide();
                }
                if (ImGui::MenuItem("FDS - Eject Disk", nullptr, false, isFdsRom && !netplayClientRestricted)) {
                    m_emu.fdsEjectDisk();
                }
                if (ImGui::MenuItem("FDS - Insert Next Disk", nullptr, false, isFdsRom && !netplayClientRestricted)) {
                    m_emu.fdsInsertNextDisk();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("VS - Insert Coin 1", nullptr, false, isVsRom && !netplayClientRestricted)) {
                    m_emu.vsInsertCoin(1);
                }
                if (ImGui::MenuItem("VS - Insert Coin 2", nullptr, false, isVsRom && !netplayClientRestricted)) {
                    m_emu.vsInsertCoin(2);
                }
                if (ImGui::MenuItem("VS - Insert Coin 3 (DualSystem)", nullptr, false, isVsRom && !netplayClientRestricted)) {
                    m_emu.vsInsertCoin(3);
                }
                if (ImGui::MenuItem("VS - Insert Coin 4 (DualSystem)", nullptr, false, isVsRom && !netplayClientRestricted)) {
                    m_emu.vsInsertCoin(4);
                }
                if (ImGui::MenuItem("VS - Service Button", nullptr, false, isVsRom && !netplayClientRestricted)) {
                    m_emu.vsServiceButton(1);
                }
                if (ImGui::MenuItem("VS - Service Button 2 (DualSystem)", nullptr, false, isVsRom && !netplayClientRestricted)) {
                    m_emu.vsServiceButton(2);
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Touch controls")) {

                bool enabled = AppSettings::instance().data.input.touchControls.enabled;
                if(ImGui::MenuItem("Enabled", nullptr, enabled)) {
                    AppSettings::instance().data.input.touchControls.enabled = !enabled;
                }

                if (ImGui::BeginMenu("Target")) {
                    auto& configuredTouchTarget = AppSettings::instance().data.input.touchControls.target;
                    const auto effectiveTouchTarget = effectiveTouchControlsTarget();

                    if(ImGui::MenuItem(
                            touchTargetMenuLabel(AppSettings::TouchControlsTarget::Port1Controller).c_str(),
                            nullptr,
                            effectiveTouchTarget == AppSettings::TouchControlsTarget::Port1Controller)) {
                        configuredTouchTarget = AppSettings::TouchControlsTarget::Port1Controller;
                    }

                    if(ImGui::MenuItem(
                            touchTargetMenuLabel(AppSettings::TouchControlsTarget::Port2Controller).c_str(),
                            nullptr,
                            effectiveTouchTarget == AppSettings::TouchControlsTarget::Port2Controller)) {
                        configuredTouchTarget = AppSettings::TouchControlsTarget::Port2Controller;
                    }

                    if(ImGui::MenuItem(
                            touchTargetMenuLabel(AppSettings::TouchControlsTarget::Expansion).c_str(),
                            nullptr,
                            effectiveTouchTarget == AppSettings::TouchControlsTarget::Expansion)) {
                        configuredTouchTarget = AppSettings::TouchControlsTarget::Expansion;
                    }

                    if(ImGui::BeginMenu("Multitap")) {
                        if(ImGui::MenuItem(
                                touchTargetMenuLabel(AppSettings::TouchControlsTarget::MultitapP1).c_str(),
                                nullptr,
                                effectiveTouchTarget == AppSettings::TouchControlsTarget::MultitapP1)) {
                            configuredTouchTarget = AppSettings::TouchControlsTarget::MultitapP1;
                        }
                        if(ImGui::MenuItem(
                                touchTargetMenuLabel(AppSettings::TouchControlsTarget::MultitapP2).c_str(),
                                nullptr,
                                effectiveTouchTarget == AppSettings::TouchControlsTarget::MultitapP2)) {
                            configuredTouchTarget = AppSettings::TouchControlsTarget::MultitapP2;
                        }
                        if(ImGui::MenuItem(
                                touchTargetMenuLabel(AppSettings::TouchControlsTarget::MultitapP3).c_str(),
                                nullptr,
                                effectiveTouchTarget == AppSettings::TouchControlsTarget::MultitapP3)) {
                            configuredTouchTarget = AppSettings::TouchControlsTarget::MultitapP3;
                        }
                        if(ImGui::MenuItem(
                                touchTargetMenuLabel(AppSettings::TouchControlsTarget::MultitapP4).c_str(),
                                nullptr,
                                effectiveTouchTarget == AppSettings::TouchControlsTarget::MultitapP4)) {
                            configuredTouchTarget = AppSettings::TouchControlsTarget::MultitapP4;
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }

                float transparencyPercent = AppSettings::instance().data.input.touchControls.transparency * 100.0f;
                if(ImGui::SliderFloat("Transparency", &transparencyPercent, 0.0f, 100.0f, "%.0f%%")) {
                    AppSettings::instance().data.input.touchControls.transparency = transparencyPercent / 100.0f;
                }

                ImGui::EndMenu();
            }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (beginTopMenu(withMenuIcon(FontAwesomeIcons::kWrench, "Tools").c_str()))
        {
            if (ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kClipboard, "Log").c_str()))
            {
                m_showLogWindow = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kWifi, "Netplay").c_str(), nullptr, false, !replayInteractionLocked))
            {
                m_showNetplayWindow = true;
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kVideo, "Replay").c_str(), nullptr, false, !isReplayRestricted())) {
                m_showReplayWindow = true;
            }

            if(ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kPuzzlePiece, "Mod").c_str(), !replayInteractionLocked)) {
                if(ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kFolderOpen, "Load").c_str(), !netplayRomChangeRestricted && !replayInteractionLocked)) {
                    if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kFileZipper, "ZIP File").c_str())) {
                        loadModArchive();
                    }
                    if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kFolder, "Folder").c_str())) {
                        loadModFolder();
                    }
                    ImGui::EndMenu();
                }

                const bool hasSelectedMod = m_modManager.hasSelectedSource();
                if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kXmark, "Clear").c_str(), nullptr, false, hasSelectedMod && !netplayRomChangeRestricted && !replayInteractionLocked)) {
                    clearSelectedMod();
                }

                ImGui::Separator();

                auto originalGraphicsShortcut = m_shortcuts.get("showOriginalModGraphics");
                const char* originalGraphicsKey =
                    (originalGraphicsShortcut != nullptr) ? originalGraphicsShortcut->shortcut.c_str() : nullptr;
                if(ImGui::MenuItem(
                        "Show original graphics",
                        originalGraphicsKey,
                        m_showOriginalGraphicsInsteadOfModFramebuffer,
                        m_modManager.active() && !replayInteractionLocked)) {
                    m_showOriginalGraphicsInsteadOfModFramebuffer = !m_showOriginalGraphicsInsteadOfModFramebuffer;
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            const bool hasLoadedRom = m_emu.valid();
            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kMemory, "Memory Viewer").c_str(), nullptr, false, hasLoadedRom)) {
                m_showMemoryViewerWindow = true;
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kMemory, "Memory Compare").c_str(), nullptr, false, hasLoadedRom)) {
                m_showMemoryCompareWindow = true;
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kMicrochip, "PPU Viewer").c_str(), nullptr, false, hasLoadedRom)) {
                m_showPpuViewerWindow = true;
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kPalette, "Screen Pixel Inspector").c_str(), nullptr, false, hasLoadedRom)) {
                m_showModPixelInspectorWindow = true;
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kClipboard, "Event Viewer").c_str(), nullptr, false, hasLoadedRom)) {
                m_showEventViewerWindow = true;
            }

            auto debugShortcut = m_shortcuts.get("cpuDebugger");
            const char* debugKey = (debugShortcut != nullptr) ? debugShortcut->shortcut.c_str() : nullptr;
            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kBug, "CPU Debugger").c_str(), debugKey, false, hasLoadedRom && !replayInteractionLocked)) {
                m_showCpuDebuggerWindow = true;
                AppSettings::instance().data.debug.showCpuDebugger = true;
                requestEnableCpuDebugger();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu(withMenuIcon(FontAwesomeIcons::kGear, "Advanced").c_str()))
            {
                if (ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kDatabase, "Rom Database").c_str(), nullptr, false, m_emu.valid()))
                {
                    loadRomDatabaseEditorFromCurrentRom();
                    m_showRomDatabaseWindow = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (beginTopMenu(withMenuIcon(FontAwesomeIcons::kCircleQuestion, "Help").c_str()))
        {
            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kBookOpen, "User Guide").c_str())) {
                openDocumentation();
            }

            if(ImGui::MenuItem(withMenuIcon(FontAwesomeIcons::kCircleInfo, "About").c_str())) {
                m_showAboutWindow = true;
            }

            ImGui::EndMenu();
        }

        ImGui::PopStyleVar();

        if(usingCustomChrome) {
            ImGui::EndMenuBar();
            m_menuBarHeight = static_cast<int>(std::round(ImGui::GetFrameHeight()));
        } else {
            ImGui::EndMainMenuBar();
            m_menuBarHeight = static_cast<int>(std::round(ImGui::GetFrameHeight()));
        }
    }
    else {
        m_menuBarHeight = 0;
    }

    if(usingCustomChrome) {
        if(menuHostBegun) {
            ImGui::End();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    } else if(show_menu) {
        ImGui::PopStyleColor();
    }
}

inline void GeraNESApp::collectAudioChannelsFromJson(const std::string& jsonStr, const char* source, std::vector<AudioChannelControl>& out)
{
    if(jsonStr.empty()) return;

    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        if(!j.is_object() || !j.contains("channels") || !j["channels"].is_array()) return;

        for(const auto& item : j["channels"]) {
            if(!item.is_object() || !item.contains("id")) continue;

            AudioChannelControl c;
            c.source = source;
            c.id = item.value("id", "");
            if(c.id.empty()) continue;

            c.label = item.value("label", c.id);
            c.volume = item.value("volume", 1.0f);
            c.min = item.value("min", 0.0f);
            c.max = item.value("max", 1.0f);

            if(c.min > c.max) std::swap(c.min, c.max);
            if(c.volume < c.min) c.volume = c.min;
            if(c.volume > c.max) c.volume = c.max;

            out.push_back(std::move(c));
        }
    }
    catch(...) {
    }
}

inline void GeraNESApp::applyAudioChannelVolume(const AudioChannelControl& c, float value)
{
    if(c.source == "nes") {
        m_emu.setAudioChannelVolumeById(c.id, value);
        return;
    }

    if(c.source == "mapper") {
        m_emu.withExclusiveAccess([&](auto& emu) {
            emu.getConsole().cartridge().setMapperAudioChannelVolumeById(c.id, value);
        });
    }
}

inline void GeraNESApp::drawAudioChannelDebugControls()
{
    constexpr double kAudioChannelRefreshIntervalSeconds = 0.5;
    const double now = ImGui::GetTime();
    const bool emuValid = m_emu.valid();
    const GameDatabase::System cartridgeSystem = m_emu.currentCartridgeSystem();
    const bool shouldRefresh =
        !m_audioChannelControlCache.valid ||
        m_audioChannelControlCache.emuValid != emuValid ||
        m_audioChannelControlCache.cartridgeSystem != cartridgeSystem ||
        (now - m_audioChannelControlCache.lastRefreshTime) >= kAudioChannelRefreshIntervalSeconds;

    if(shouldRefresh) {
        m_audioChannelControlCache.nesChannels.clear();
        m_audioChannelControlCache.mapperChannels.clear();

        collectAudioChannelsFromJson(
            m_emu.getAudioChannelsJson(),
            "nes",
            m_audioChannelControlCache.nesChannels
        );

        if(emuValid) {
            m_emu.withExclusiveAccess([&](auto& emu) {
                collectAudioChannelsFromJson(
                    emu.getConsole().cartridge().getMapperAudioChannelsJson(),
                    "mapper",
                    m_audioChannelControlCache.mapperChannels
                );
            });
        }

        m_audioChannelControlCache.lastRefreshTime = now;
        m_audioChannelControlCache.valid = true;
        m_audioChannelControlCache.emuValid = emuValid;
        m_audioChannelControlCache.cartridgeSystem = cartridgeSystem;
    }

    if(m_audioChannelControlCache.nesChannels.empty() && m_audioChannelControlCache.mapperChannels.empty()) {
        ImGui::TextDisabled("No channel controls available.");
        return;
    }

    auto drawChannelSliders = [&](std::vector<AudioChannelControl>& list) {
        for(auto& c : list) {
            float valuePercent = c.volume * 100.0f;
            float minPercent = c.min * 100.0f;
            float maxPercent = c.max * 100.0f;
            std::string label = c.label + "##" + c.source + "." + c.id;
            if(ImGui::SliderFloat(label.c_str(), &valuePercent, minPercent, maxPercent, "%.0f%%")) {
                c.volume = valuePercent / 100.0f;
                applyAudioChannelVolume(c, c.volume);
            }
        }
    };

    drawChannelSliders(m_audioChannelControlCache.nesChannels);

    if(!m_audioChannelControlCache.nesChannels.empty() && !m_audioChannelControlCache.mapperChannels.empty()) {
        ImGui::Separator();
    }

    drawChannelSliders(m_audioChannelControlCache.mapperChannels);
}

#include "GeraNESApp/GeraNESApp.PaletteWindowUI.inl"
#include "GeraNESApp/GeraNESApp.OverscanWindowUI.inl"
#include "GeraNESApp/GeraNESApp.ShaderWindowUI.inl"
#include "GeraNESApp/GeraNESApp.PpuViewerWindowUI.inl"
#include "GeraNESApp/GeraNESApp.ModPixelInspectorWindowUI.inl"
#include "GeraNESApp/GeraNESApp.EventViewerWindowUI.inl"
