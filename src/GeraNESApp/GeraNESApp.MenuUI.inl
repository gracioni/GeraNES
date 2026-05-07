#pragma once

inline void GeraNESApp::menuBar() {

    bool show_menu = true;
    const bool netplayClientRestricted = isNetplayClientRestricted();
    const bool netplayRomChangeRestricted = isNetplayRomChangeRestricted();
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
        auto beginTopMenu = [](const char* label, bool enabled = true) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGuiTheme::textOnAccent());
            const bool open = ImGui::BeginMenu(label, enabled);
            ImGui::PopStyleColor();
            return open;
        };

        if (beginTopMenu("File"))
        {
            const bool hasRomLoaded = m_emu.valid();
            auto sc = m_shortcuts.get("openRom");
            if( sc != nullptr) {

                if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), false, !netplayRomChangeRestricted))
                {
                    sc->action();
                }
            }

            if(ImGui::MenuItem("Close ROM", nullptr, false, hasRomLoaded && !netplayRomChangeRestricted)) {
                closeRomAction();
            }

            #ifdef __EMSCRIPTEN__
            if (ImGui::BeginMenu("Session")) {

                if(ImGui::MenuItem("Export")) {
                    AppSettings::instance().save();
                    emcriptenExportSession();
                }

                if(ImGui::MenuItem("Import", nullptr, false, !netplayRomChangeRestricted)) {
                    emcriptenImportSession(reinterpret_cast<intptr_t>(this));
                }

                ImGui::EndMenu();
            }
            #endif

            auto recentFiles = AppSettings::instance().data.getRecentFiles();
            if (ImGui::BeginMenu("Recent Files", recentFiles.size() > 0 && !netplayRomChangeRestricted))
            {
                for(size_t i = 0; i < recentFiles.size(); ++i) {
#ifdef __EMSCRIPTEN__
                    const std::string displayName = fs::path(recentFiles[i]).filename().string();
                    const std::string menuLabel = (displayName.empty() ? recentFiles[i] : displayName) + "##" + recentFiles[i];
                    if(ImGui::MenuItem(menuLabel.c_str())) {
#else
                    if(ImGui::MenuItem(recentFiles[i].c_str())) {
#endif
                        openFile(recentFiles[i].c_str());
                    }
                }
                ImGui::EndMenu();
            }

            sc = m_shortcuts.get("quit");
            if( sc != nullptr) {
                ImGui::Separator();

                if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str()))
                {
                    sc->action();
                }
            }

            ImGui::EndMenu();
        }

        if (beginTopMenu("Emulator", !netplayClientRestricted))
        {
            const bool hasRomLoaded = m_emu.valid();
            int& saveStateSlot = AppSettings::instance().data.saveStateSlot;
            auto sc = m_shortcuts.get("saveState");
            if( sc != nullptr) {

                if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), false, hasRomLoaded))
                {
                    sc->action();
                }
            }

            sc = m_shortcuts.get("loadState");
            if( sc != nullptr) {

                if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), false, hasRomLoaded))
                {
                    sc->action();
                }
            }

            if(ImGui::BeginMenu("Slot")) {
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

            if (ImGui::MenuItem("Improvements")) {
                m_showImprovementsWindow = true;
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Region", hasRomLoaded)) {

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
            if(ImGui::MenuItem("Pause", pauseKey, pauseSelected, hasRomLoaded && !isNetplayPauseRestricted())) {
                togglePauseAction();
            }

            ImGui::Separator();

            if(ImGui::MenuItem("Reset", nullptr, false, hasRomLoaded && !netplayClientRestricted)) {
                resetAction();
            }

            ImGui::EndMenu();
        }

        if (beginTopMenu("Options"))
        {
            if (ImGui::BeginMenu("Video"))
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

                if (ImGui::BeginMenu("Shader")) {
                    const bool usingDefaultShader = AppSettings::instance().data.video.shaderStack.empty();
                    if(ImGui::MenuItem("Use default", nullptr, usingDefaultShader)) {
                        AppSettings::instance().data.video.shaderStack.clear();
                        AppSettings::instance().data.video.shaderName.clear();
                        m_selectedShaderStackIndex = -1;
                        updateShaderConfig();
                    }

                    if(ImGui::MenuItem("Stack editor...")) {
                        m_showShaderStackWindow = true;
                    }

                    if(!AppSettings::instance().data.video.shaderStack.empty()) {
                        ImGui::Separator();
                        for(size_t i = 0; i < AppSettings::instance().data.video.shaderStack.size(); ++i) {
                            const std::string label =
                                std::to_string(i + 1) + ". " + AppSettings::instance().data.video.shaderStack[i].label +
                                (AppSettings::instance().data.video.shaderStack[i].enabled ? "" : " (off)");
                            ImGui::TextUnformatted(label.c_str());
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Palette")) {
                    m_showPaletteWindow = true;
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

                auto sc = m_shortcuts.get("fullscreen");
                if( sc != nullptr) {

                    if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), isFullScreen()))
                    {
                        sc->action();
                    }
                }

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

                ImGui::Separator();
                if (ImGui::MenuItem("Show FPS", nullptr, &AppSettings::instance().data.debug.showFps))
                {
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Audio"))
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

            if (ImGui::BeginMenu("Input"))
            {
#ifndef __EMSCRIPTEN__
            const auto netplaySnapshot = GeraNESNetplay::menuSnapshot(m_netplayRuntime);
            const auto localInputTopology = m_emu.getInputTopologySnapshot();
            const bool netplayInputManaged = netplaySnapshot.inputManaged;
            const auto& localNetplayAssignments = netplaySnapshot.localAssignments;
            const bool canChangeNetplayManagedInput = !netplayInputManaged;
            const auto effectivePort1Device = netplayInputManaged
                ? netplaySnapshot.port1Device
                : localInputTopology.port1Device;
            const auto effectivePort2Device = netplayInputManaged
                ? netplaySnapshot.port2Device
                : localInputTopology.port2Device;
            const auto effectiveExpansionDevice = netplayInputManaged
                ? netplaySnapshot.expansionDevice
                : localInputTopology.expansionDevice;
            const auto effectiveNesMultitapDevice = netplayInputManaged
                ? netplaySnapshot.nesMultitapDevice
                : localInputTopology.nesMultitapDevice;
            const auto effectiveFamicomMultitapDevice = netplayInputManaged
                ? netplaySnapshot.famicomMultitapDevice
                : localInputTopology.famicomMultitapDevice;
#else
            const bool netplayInputManaged = false;
            const std::vector<ConsoleNetplay::PlayerSlot> localNetplayAssignments;
            const bool canChangeNetplayManagedInput = true;
            const auto localInputTopology = m_emu.getInputTopologySnapshot();
            const auto effectivePort1Device = localInputTopology.port1Device;
            const auto effectivePort2Device = localInputTopology.port2Device;
            const auto effectiveExpansionDevice = localInputTopology.expansionDevice;
            const auto effectiveNesMultitapDevice = localInputTopology.nesMultitapDevice;
            const auto effectiveFamicomMultitapDevice = localInputTopology.famicomMultitapDevice;
#endif
            const bool nesFourScoreEnabled = effectiveNesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE;
            const bool famicomHoriEnabled = effectiveFamicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER;
            const bool anyMultitapActive = nesFourScoreEnabled || famicomHoriEnabled;

            auto effectivePortDeviceFor = [&](Settings::Port port)
            {
                return port == Settings::Port::P_1 ? effectivePort1Device : effectivePort2Device;
            };

            auto drawControllerPortMenuItem = [this, &effectivePortDeviceFor, canChangeNetplayManagedInput](const char* label, Settings::Port port, Settings::Device device)
            {
                const bool selected = effectivePortDeviceFor(port) == std::optional<Settings::Device>(device);
                if(ImGui::MenuItem(label, nullptr, selected, canChangeNetplayManagedInput)) {
                    m_emu.setPortDevice(port, device);
                }
            };

            auto drawControllerPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                const auto device = effectivePortDeviceFor(port);
                if(device != std::optional<Settings::Device>(Settings::Device::CONTROLLER) &&
                   device != std::optional<Settings::Device>(Settings::Device::FAMICOM_CONTROLLER)) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    const bool famicomController = device == std::optional<Settings::Device>(Settings::Device::FAMICOM_CONTROLLER);
                    if(port == Settings::Port::P_1) m_inputBindingConfigWindow.show(famicomController ? "Famicom Controller 1" : "Standard Controller 1", m_controller1);
                    else m_inputBindingConfigWindow.show(famicomController ? "Famicom Controller 2" : "Standard Controller 2", m_controller2);
                }
            };

            auto drawArkanoidPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                if(effectivePortDeviceFor(port) != std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER)) {
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
                if(device != std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) &&
                   device != std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE)) {
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
                if(device != std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A) &&
                   device != std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B)) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...", nullptr, false, enabled)) {
                    m_powerPadConfigWindow.show("Power Pad Config", m_powerPadInfo);
                }
            };

            auto drawSnesControllerPortConfigItem = [this, &effectivePortDeviceFor](Settings::Port port, bool enabled)
            {
                if(effectivePortDeviceFor(port) != std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER)) {
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
                if(effectivePortDeviceFor(port) != std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER)) {
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
                if (ImGui::MenuItem("None", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::NONE, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::NONE);
                }
                if (ImGui::MenuItem("Standard Controller (Famicom)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM);
                }
                if (ImGui::MenuItem("Bandai Hyper Shot", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::BANDAI_HYPERSHOT);
                }
                if (ImGui::MenuItem("Konami Hyper Shot", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::KONAMI_HYPERSHOT);
                }
                if (ImGui::MenuItem("Family Trainer (Side A)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A);
                }
                if (ImGui::MenuItem("Family Trainer (Side B)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B);
                }
                if (ImGui::MenuItem("Subor Keyboard", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::SUBOR_KEYBOARD);
                }
                if (ImGui::MenuItem("Family Basic Keyboard", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD);
                }
                if (ImGui::MenuItem("Arkanoid Controller (Famicom)", nullptr, effectiveExpansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER, canChangeNetplayManagedInput))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::ARKANOID_CONTROLLER);
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
                        if(ImGui::MenuItem("Enabled", nullptr, nesFourScoreEnabled, canChangeNetplayManagedInput)) {
                            m_emu.setNesMultitapDevice(
                                nesFourScoreEnabled ? Settings::NesMultitapDevice::NONE : Settings::NesMultitapDevice::FOUR_SCORE
                            );
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
                        if(ImGui::MenuItem("Enabled", nullptr, famicomHoriEnabled, canChangeNetplayManagedInput)) {
                            m_emu.setFamicomMultitapDevice(
                                famicomHoriEnabled ? Settings::FamicomMultitapDevice::NONE : Settings::FamicomMultitapDevice::HORI_ADAPTER
                            );
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

#ifdef ENABLE_NSF_PLAYER
        if (beginTopMenu("NSF", m_emu.isNsfLoaded()))
        {
            const bool isPlaying = m_emu.nsfIsPlaying();
            const bool isPaused = m_emu.nsfIsPaused();
            const bool hasEnded = m_emu.nsfHasEnded();
            const int totalSongs = m_emu.nsfTotalSongs();
            const int currentSong = m_emu.nsfCurrentSong();
            const bool canPlay = !isPlaying || isPaused || hasEnded;
            const bool canPause = isPlaying && !isPaused && !hasEnded;
            const bool canStop = (isPlaying || isPaused) && !hasEnded;

            if(ImGui::MenuItem("Play", nullptr, false, canPlay)) {
                m_emu.nsfPlay();
            }
            if(ImGui::MenuItem("Pause", nullptr, false, canPause)) {
                m_emu.nsfPause();
            }
            if(ImGui::MenuItem("Stop", nullptr, false, canStop)) {
                m_emu.nsfStop();
            }

            ImGui::Separator();
            ImGui::Text("Song %d / %d", currentSong, totalSongs);
            int selectedSong = currentSong;
            if(ImGui::InputInt("Song", &selectedSong, 1, 1)) {
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

            ImGui::EndMenu();
        }
#endif

        if (beginTopMenu("Tools"))
        {
            if (ImGui::MenuItem("Log"))
            {
                m_showLogWindow = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Netplay"))
            {
                m_showNetplayWindow = true;
            }

            ImGui::Separator();

            if(ImGui::MenuItem("PPU Viewer", nullptr, m_showPpuViewerWindow)) {
                m_showPpuViewerWindow = !m_showPpuViewerWindow;
            }

            if(ImGui::MenuItem("Event Viewer", nullptr, m_showEventViewerWindow)) {
                m_showEventViewerWindow = !m_showEventViewerWindow;
            }

            auto debugShortcut = m_shortcuts.get("cpuDebugger");
            const char* debugKey = (debugShortcut != nullptr) ? debugShortcut->shortcut.c_str() : nullptr;
            const bool netplayBlocksCpuDebug = isNetplayBlockingCpuDebug();
            if(ImGui::MenuItem("CPU Debugger", debugKey, m_showCpuDebuggerWindow, !netplayBlocksCpuDebug)) {
                m_showCpuDebuggerWindow = !m_showCpuDebuggerWindow;
                AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;
            }

            auto breakpointShortcut = m_shortcuts.get("cpuBreakpoints");
            const char* breakpointKey = (breakpointShortcut != nullptr) ? breakpointShortcut->shortcut.c_str() : nullptr;
            if(ImGui::MenuItem("CPU Breakpoints", breakpointKey, m_showCpuBreakpointsWindow, !netplayBlocksCpuDebug)) {
                m_showCpuBreakpointsWindow = !m_showCpuBreakpointsWindow;
                AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Advanced"))
            {
                if (ImGui::MenuItem("Rom Database", nullptr, false, m_emu.valid()))
                {
                    loadRomDatabaseEditorFromCurrentRom();
                    m_showRomDatabaseWindow = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiTheme::textOnAccent());
        if (ImGui::MenuItem("About"))
        {
            m_showAboutWindow = true;
        }
        ImGui::PopStyleColor();

        ImGui::PopStyleVar();

        if(usingCustomChrome) {
            ImGui::EndMenuBar();
            m_menuBarHeight = static_cast<int>(std::round(ImGui::GetWindowHeight()));
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
    std::vector<AudioChannelControl> nesChannels;
    std::vector<AudioChannelControl> mapperChannels;
    collectAudioChannelsFromJson(m_emu.getAudioChannelsJson(), "nes", nesChannels);
    m_emu.withExclusiveAccess([&](auto& emu) {
        collectAudioChannelsFromJson(emu.getConsole().cartridge().getMapperAudioChannelsJson(), "mapper", mapperChannels);
    });

    if(nesChannels.empty() && mapperChannels.empty()) {
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
                applyAudioChannelVolume(c, valuePercent / 100.0f);
            }
        }
    };

    drawChannelSliders(nesChannels);

    if(!nesChannels.empty() && !mapperChannels.empty()) {
        ImGui::Separator();
    }

    drawChannelSliders(mapperChannels);
}

inline void GeraNESApp::drawPaletteWindow()
{
    ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_Appearing);

    if(!ImGui::Begin("Palette", &m_showPaletteWindow)) {
        ImGui::End();
        return;
    }

    if(ImGui::BeginChild("PaletteList", ImVec2(180.0f, 0.0f), true)) {
        if(ImGui::Selectable("Default", m_selectedPaletteName == "Default")) {
            applyPalette(m_paletteList.front().colors, "Default");
        }

        for(const PaletteItem& item : m_paletteList) {
            if(item.builtIn) continue;
            if(ImGui::Selectable(item.name.c_str(), m_selectedPaletteName == item.name)) {
                applyPalette(item.colors, item.name);
            }
        }

        ImGui::Separator();
        if(ImGui::Button("New")) {
            createNewPalette();
        }
        ImGui::SameLine();
        if(ImGui::Button("Reload")) {
            loadPaletteList();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if(ImGui::BeginChild("PaletteEditor", ImVec2(0.0f, 0.0f), true)) {
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##PaletteName", &m_paletteNameInput);

        ImGui::SameLine();
        if(ImGui::Button("Save")) {
            saveCurrentPalette();
        }

        ImGui::SameLine();
        const bool canDeletePalette = m_selectedPaletteName != "Default";
        if(!canDeletePalette) ImGui::BeginDisabled();
        if(ImGui::Button("Delete")) {
            deleteCurrentPalette();
        }
        if(!canDeletePalette) ImGui::EndDisabled();

        ImGui::Separator();

        if(ImGui::BeginChild("PaletteGrid", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            for(size_t i = 0; i < m_editPalette.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));

                float color[3] = {
                    static_cast<float>(m_editPalette[i] & 0xFF) / 255.0f,
                    static_cast<float>((m_editPalette[i] >> 8) & 0xFF) / 255.0f,
                    static_cast<float>((m_editPalette[i] >> 16) & 0xFF) / 255.0f
                };

                if(ImGui::ColorEdit3("##color", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                    const uint32_t r = static_cast<uint32_t>(std::clamp(color[0], 0.0f, 1.0f) * 255.0f + 0.5f);
                    const uint32_t g = static_cast<uint32_t>(std::clamp(color[1], 0.0f, 1.0f) * 255.0f + 0.5f);
                    const uint32_t b = static_cast<uint32_t>(std::clamp(color[2], 0.0f, 1.0f) * 255.0f + 0.5f);
                    m_editPalette[i] = 0xFF000000u | r | (g << 8) | (b << 16);
                    m_emu.setColorPalette(m_editPalette);
                }

                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%02X  %s", static_cast<unsigned int>(i), paletteColorToHex(m_editPalette[i]).c_str());
                }

                if((i % 16) != 15) ImGui::SameLine(0.0f, 6.0f);
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
}

inline void GeraNESApp::drawShaderStackWindow()
{
    ImGui::SetNextWindowSize(ImVec2(700.0f, 440.0f), ImGuiCond_Appearing);

    if(!ImGui::Begin("Shader Stack", &m_showShaderStackWindow)) {
        ImGui::End();
        return;
    }

    auto& shaderStack = AppSettings::instance().data.video.shaderStack;
    if(shaderList.empty()) {
        ImGui::TextDisabled("No .glsl shader files found in shaders/.");
        ImGui::TextDisabled("Drop .glsl files there and reopen this window.");
        ImGui::End();
        return;
    }

    m_selectedAvailableShaderIndex = std::clamp(m_selectedAvailableShaderIndex, 0, static_cast<int>(shaderList.size()) - 1);
    if(shaderStack.empty()) {
        m_selectedShaderStackIndex = -1;
    } else {
        if(m_selectedShaderStackIndex < 0 || m_selectedShaderStackIndex >= static_cast<int>(shaderStack.size())) {
            m_selectedShaderStackIndex = static_cast<int>(shaderStack.size()) - 1;
        }
    }

    ImGui::BeginChild("AvailableShaders", ImVec2(280.0f, 0.0f), true);
    ImGui::TextUnformatted("Available");
    ImGui::Separator();
    for(size_t i = 0; i < shaderList.size(); ++i) {
        const bool selected = m_selectedAvailableShaderIndex == static_cast<int>(i);
        if(ImGui::Selectable(shaderList[i].label.c_str(), selected)) {
            m_selectedAvailableShaderIndex = static_cast<int>(i);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ShaderStackEditor", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Passes");
    ImGui::Separator();

    const bool canAdd = !shaderList.empty() && m_selectedAvailableShaderIndex >= 0 &&
        m_selectedAvailableShaderIndex < static_cast<int>(shaderList.size());
    if(ImGui::Button("Add Pass") && canAdd) {
        AppSettings::Video::ShaderPass pass;
        pass.label = shaderList[static_cast<size_t>(m_selectedAvailableShaderIndex)].label;
        pass.enabled = true;
        shaderStack.push_back(std::move(pass));
        m_selectedShaderStackIndex = static_cast<int>(shaderStack.size()) - 1;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Clear Stack")) {
        shaderStack.clear();
        m_selectedShaderStackIndex = -1;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload")) {
        loadShaderList();
        updateShaderConfig();
    }

    ImGui::Separator();

    if(ImGui::BeginChild("ShaderStackPasses", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 8.0f), true)) {
        if(shaderStack.empty()) {
            ImGui::TextDisabled("Default shader only.");
        } else {
            for(size_t i = 0; i < shaderStack.size(); ++i) {
                auto& pass = shaderStack[i];
                ImGui::PushID(static_cast<int>(i));
                bool enabled = pass.enabled;
                if(ImGui::Checkbox("##Enabled", &enabled)) {
                    pass.enabled = enabled;
                    updateShaderConfig();
                }
                ImGui::SameLine();
                const std::string rowLabel = std::to_string(i + 1) + ". " + pass.label;
                if(ImGui::Selectable(rowLabel.c_str(), m_selectedShaderStackIndex == static_cast<int>(i), ImGuiSelectableFlags_SpanAvailWidth)) {
                    m_selectedShaderStackIndex = static_cast<int>(i);
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    const bool hasEntries = !shaderStack.empty();
    const int activeIndex = hasEntries
        ? std::clamp(m_selectedShaderStackIndex, 0, static_cast<int>(shaderStack.size()) - 1)
        : -1;

    if(!hasEntries) ImGui::BeginDisabled();
    if(ImGui::Button("Up") && activeIndex > 0) {
        std::swap(shaderStack[static_cast<size_t>(m_selectedShaderStackIndex)], shaderStack[static_cast<size_t>(m_selectedShaderStackIndex - 1)]);
        --m_selectedShaderStackIndex;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Down") && activeIndex >= 0 && activeIndex + 1 < static_cast<int>(shaderStack.size())) {
        std::swap(shaderStack[static_cast<size_t>(m_selectedShaderStackIndex)], shaderStack[static_cast<size_t>(m_selectedShaderStackIndex + 1)]);
        ++m_selectedShaderStackIndex;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Remove") && activeIndex >= 0) {
        shaderStack.erase(shaderStack.begin() + activeIndex);
        if(shaderStack.empty()) m_selectedShaderStackIndex = -1;
        else if(activeIndex >= static_cast<int>(shaderStack.size())) m_selectedShaderStackIndex = static_cast<int>(shaderStack.size()) - 1;
        else m_selectedShaderStackIndex = activeIndex;
        updateShaderConfig();
    }
    if(!hasEntries) ImGui::EndDisabled();

    if(activeIndex >= 0 && activeIndex < static_cast<int>(shaderStack.size())) {
        auto& configuredPass = shaderStack[static_cast<size_t>(activeIndex)];
        const ShaderItem* configuredItem = findShaderByLabel(configuredPass.label);
        ImGui::Separator();
        ImGui::Text("Parameters: %s", configuredPass.label.c_str());

        if(configuredItem == nullptr) {
            ImGui::TextDisabled("Shader file not found.");
        } else {
            std::ifstream shaderFile(configuredItem->path);
            std::string shaderText((std::istreambuf_iterator<char>(shaderFile)), std::istreambuf_iterator<char>());
            std::vector<ShaderPass::Parameter> parameters = parseShaderParameters(shaderText);
            for(ShaderPass::Parameter& parameter : parameters) {
                auto it = configuredPass.parameters.find(parameter.name);
                if(it != configuredPass.parameters.end()) {
                    parameter.value = std::clamp(it->second, parameter.minValue, parameter.maxValue);
                }
            }

            if(parameters.empty()) {
                ImGui::TextDisabled("This shader exposes no #pragma parameter entries.");
            } else {
                if(ImGui::Button("Reset To Default")) {
                    configuredPass.parameters.clear();
                    updateShaderConfig();
                }
                ImGui::Separator();

                for(const ShaderPass::Parameter& parameter : parameters) {
                    float value = parameter.value;
                    const std::string sliderId = parameter.label + "##" + parameter.name;
                    if(ImGui::SliderFloat(sliderId.c_str(), &value, parameter.minValue, parameter.maxValue, "%.3f")) {
                        configuredPass.parameters[parameter.name] = value;
                        updateShaderConfig();
                    }
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Passes run top to bottom. Remove every pass to go back to the default shader.");
    ImGui::EndChild();

    ImGui::End();
}

inline void GeraNESApp::drawPpuViewerWindow()
{
    constexpr int kNametableWidth = 512;
    constexpr int kNametableHeight = 480;
    constexpr int kChrWidth = 256;
    constexpr int kChrHeight = 128;
    constexpr float kPaletteSwatchSize = 18.0f;
    constexpr float kPaletteSwatchSpacing = 4.0f;

    ImGui::SetNextWindowSize(ImVec2(1080.0f, 650.0f), ImGuiCond_Appearing);

    if(!ImGui::Begin("PPU Viewer", &m_showPpuViewerWindow)) {
        ImGui::End();
        return;
    }

    if(!m_emu.valid()) {
        ImGui::TextDisabled("Load a ROM to inspect PPU data.");
        ImGui::End();
        return;
    }

    if(m_ppuNametableTexture == 0) {
        glGenTextures(1, &m_ppuNametableTexture);
        glBindTexture(GL_TEXTURE_2D, m_ppuNametableTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kNametableWidth, kNametableHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    if(m_ppuChrTexture == 0) {
        glGenTextures(1, &m_ppuChrTexture);
        glBindTexture(GL_TEXTURE_2D, m_ppuChrTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kChrWidth, kChrHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    if(m_ppuNametableBuffer.size() != static_cast<size_t>(kNametableWidth * kNametableHeight)) {
        m_ppuNametableBuffer.resize(static_cast<size_t>(kNametableWidth * kNametableHeight));
    }

    if(m_ppuChrBuffer.size() != static_cast<size_t>(kChrWidth * kChrHeight)) {
        m_ppuChrBuffer.resize(static_cast<size_t>(kChrWidth * kChrHeight));
    }

    std::array<uint8_t, 0x2000> chrData = {};
    std::array<uint8_t, 0x1000> nametableData = {};
    std::array<uint8_t, 0x20> paletteData = {};
    std::array<uint32_t, 64> rgbPalette = {};
    int scrollX = 0;
    int scrollY = 0;
    int backgroundPatternTableAddress = 0x0000;

    m_emu.withExclusiveAccess([&](auto& emu) {
        const PPU& ppu = emu.getConsole().ppu();

        rgbPalette = ppu.colorPalette();
        scrollX = ppu.getCursorX();
        scrollY = ppu.getCursorY();
        backgroundPatternTableAddress = ppu.debugBackgroundPatternTableAddress();

        for(size_t i = 0; i < chrData.size(); ++i) {
            chrData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(i));
        }

        for(size_t i = 0; i < nametableData.size(); ++i) {
            nametableData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x2000 + i));
        }

        for(size_t i = 0; i < paletteData.size(); ++i) {
            paletteData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x3F00 + i));
        }
    });

    const auto colorForPaletteEntry = [&](uint8_t paletteEntry) -> uint32_t {
        return rgbPalette[paletteEntry & 0x3F];
    };

    const uint8_t universalBackground = static_cast<uint8_t>(paletteData[0] & 0x3F);

    for(int y = 0; y < kNametableHeight; ++y) {
        const int nameTableRow = y >= 240 ? 2 : 0;
        const int localY = y % 240;
        const int tileY = localY >> 3;
        const int fineY = localY & 0x07;

        for(int x = 0; x < kNametableWidth; ++x) {
            const int nameTableIndex = nameTableRow + (x >= 256 ? 1 : 0);
            const int localX = x & 0xFF;
            const int tileX = localX >> 3;
            const int fineX = localX & 0x07;
            const int nameTableBase = nameTableIndex * 0x400;
            const uint8_t tileIndex = nametableData[static_cast<size_t>(nameTableBase + (tileY * 32) + tileX)];
            const uint8_t attrByte = nametableData[static_cast<size_t>(nameTableBase + 0x3C0 + ((tileY >> 2) * 8) + (tileX >> 2))];
            const int attrShift = ((tileY & 0x02) << 1) | (tileX & 0x02);
            const uint8_t paletteIndex = static_cast<uint8_t>((attrByte >> attrShift) & 0x03);
            const int patternAddr = backgroundPatternTableAddress + (tileIndex * 16) + fineY;
            const uint8_t lowPlane = chrData[static_cast<size_t>(patternAddr)];
            const uint8_t highPlane = chrData[static_cast<size_t>(patternAddr + 8)];
            const int bit = 7 - fineX;
            const uint8_t colorIndex = static_cast<uint8_t>(((lowPlane >> bit) & 0x01) | (((highPlane >> bit) & 0x01) << 1));

            uint8_t paletteEntry = universalBackground;
            if(colorIndex != 0) {
                paletteEntry = static_cast<uint8_t>(paletteData[static_cast<size_t>((paletteIndex * 4) + colorIndex)] & 0x3F);
            }

            m_ppuNametableBuffer[static_cast<size_t>((y * kNametableWidth) + x)] = colorForPaletteEntry(paletteEntry);
        }
    }

    for(int table = 0; table < 2; ++table) {
        const int tableBase = table * 0x1000;
        const int xOffset = table * 128;

        for(int tileY = 0; tileY < 16; ++tileY) {
            for(int tileX = 0; tileX < 16; ++tileX) {
                const int tileIndex = (tileY * 16) + tileX;

                for(int fineY = 0; fineY < 8; ++fineY) {
                    const uint8_t lowPlane = chrData[static_cast<size_t>(tableBase + (tileIndex * 16) + fineY)];
                    const uint8_t highPlane = chrData[static_cast<size_t>(tableBase + (tileIndex * 16) + fineY + 8)];

                    for(int fineX = 0; fineX < 8; ++fineX) {
                        const int bit = 7 - fineX;
                        const uint8_t colorIndex = static_cast<uint8_t>(((lowPlane >> bit) & 0x01) | (((highPlane >> bit) & 0x01) << 1));
                        uint8_t paletteEntry = universalBackground;
                        if(colorIndex != 0) {
                            paletteEntry = static_cast<uint8_t>(paletteData[static_cast<size_t>(colorIndex)] & 0x3F);
                        }

                        const int dstX = xOffset + (tileX * 8) + fineX;
                        const int dstY = (tileY * 8) + fineY;
                        m_ppuChrBuffer[static_cast<size_t>((dstY * kChrWidth) + dstX)] = colorForPaletteEntry(paletteEntry);
                    }
                }
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_ppuNametableTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kNametableWidth, kNametableHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_ppuNametableBuffer.data());
    glBindTexture(GL_TEXTURE_2D, m_ppuChrTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kChrWidth, kChrHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_ppuChrBuffer.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    ImGui::Text("Scroll: X=%d  Y=%d", scrollX, scrollY);
    ImGui::SameLine();
    ImGui::TextDisabled("Background pattern table: $%04X", backgroundPatternTableAddress);
    ImGui::Spacing();

    auto drawPaletteStrip = [&](const char* label, int paletteBaseIndex, int displayBaseAddress) {
        ImGui::BeginGroup();
        ImGui::Text("%s $%04X", label, displayBaseAddress);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 start = ImGui::GetCursorScreenPos();

        for(int i = 0; i < 4; ++i) {
            const uint8_t paletteEntry = static_cast<uint8_t>(paletteData[static_cast<size_t>(paletteBaseIndex + i)] & 0x3F);
            const ImVec2 swatchMin(start.x + i * (kPaletteSwatchSize + kPaletteSwatchSpacing), start.y);
            const ImVec2 swatchMax(swatchMin.x + kPaletteSwatchSize, swatchMin.y + kPaletteSwatchSize);
            drawList->AddRectFilled(swatchMin, swatchMax, colorForPaletteEntry(paletteEntry), 3.0f);
            drawList->AddRect(swatchMin, swatchMax, ImGuiTheme::toU32(ImGuiTheme::textDisabled()), 3.0f);
        }

        ImGui::Dummy(ImVec2((kPaletteSwatchSize * 4.0f) + (kPaletteSwatchSpacing * 3.0f), kPaletteSwatchSize));
        ImGui::EndGroup();
    };

    ImGui::TextUnformatted("PPU Palettes");
    for(int paletteIndex = 0; paletteIndex < 4; ++paletteIndex) {
        if(paletteIndex > 0) {
            ImGui::SameLine();
        }
        drawPaletteStrip(("BG " + std::to_string(paletteIndex)).c_str(), paletteIndex * 4, 0x3F00 + paletteIndex * 4);
    }
    ImGui::NewLine();
    for(int paletteIndex = 0; paletteIndex < 4; ++paletteIndex) {
        if(paletteIndex > 0) {
            ImGui::SameLine();
        }
        drawPaletteStrip(("SPR " + std::to_string(paletteIndex)).c_str(), 0x10 + paletteIndex * 4, 0x3F10 + paletteIndex * 4);
    }
    ImGui::Separator();

    if(ImGui::BeginChild("PpuViewerScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Nametables");
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(m_ppuNametableTexture)),
            ImVec2(static_cast<float>(kNametableWidth), static_cast<float>(kNametableHeight))
        );

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float clampedScrollY = static_cast<float>(std::clamp(scrollY, 0, kNametableHeight - 1));
        const float clampedScrollX = static_cast<float>(std::clamp(scrollX, 0, kNametableWidth - 1));

        drawList->AddLine(
            ImVec2(imageMin.x, imageMin.y + clampedScrollY),
            ImVec2(imageMax.x, imageMin.y + clampedScrollY),
            ImGuiTheme::toU32(ImGuiTheme::eventWrite()),
            1.0f
        );
        drawList->AddLine(
            ImVec2(imageMin.x + clampedScrollX, imageMin.y),
            ImVec2(imageMin.x + clampedScrollX, imageMax.y),
            ImGuiTheme::toU32(ImGuiTheme::eventWrite()),
            1.0f
        );

        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        if(ImGui::IsItemHovered()) {
            const int hoveredX = std::clamp(static_cast<int>(mousePos.x - imageMin.x), 0, kNametableWidth - 1);
            const int hoveredY = std::clamp(static_cast<int>(mousePos.y - imageMin.y), 0, kNametableHeight - 1);
            const int tileX = hoveredX / 8;
            const int tileY = hoveredY / 8;
            const int nameTableIndex = (tileY >= 30 ? 2 : 0) + (tileX >= 32 ? 1 : 0);
            const int localTileX = tileX % 32;
            const int localTileY = tileY % 30;
            const uint16_t tileAddress = static_cast<uint16_t>(0x2000 + nameTableIndex * 0x400 + localTileY * 32 + localTileX);
            const int nameTableBase = nameTableIndex * 0x400;
            const uint8_t attrByte = nametableData[static_cast<size_t>(nameTableBase + 0x3C0 + ((localTileY >> 2) * 8) + (localTileX >> 2))];
            const int attrShift = ((localTileY & 0x02) << 1) | (localTileX & 0x02);
            const uint8_t paletteIndex = static_cast<uint8_t>((attrByte >> attrShift) & 0x03);

            const ImVec2 tileMin(imageMin.x + tileX * 8.0f, imageMin.y + tileY * 8.0f);
            const ImVec2 tileMax(tileMin.x + 8.0f, tileMin.y + 8.0f);
            drawList->AddRect(tileMin, tileMax, ImGuiTheme::toU32(ImGuiTheme::eventWrite()), 0.0f, 0, 2.0f);

            ImGui::BeginTooltip();
            ImGui::Text("Nametable tile");
            ImGui::Text("Tile: (%d, %d)", tileX, tileY);
            ImGui::Text("Nametable addr: $%04X", static_cast<unsigned int>(tileAddress));
            ImGui::Text("Tile index: $%02X", static_cast<unsigned int>(nametableData[static_cast<size_t>(tileAddress - 0x2000)]));
            ImGui::Text("BG palette: %u ($3F%02X-$3F%02X)",
                        static_cast<unsigned int>(paletteIndex),
                        static_cast<unsigned int>(paletteIndex * 4),
                        static_cast<unsigned int>(paletteIndex * 4 + 3));
            ImGui::Separator();
            for(int i = 0; i < 4; ++i) {
                if(i > 0) {
                    ImGui::SameLine();
                }
                const int paletteOffset = static_cast<int>(paletteIndex) * 4 + i;
                const uint8_t paletteEntry = static_cast<uint8_t>(paletteData[static_cast<size_t>(paletteOffset)] & 0x3F);
                const ImVec2 swatchMin = ImGui::GetCursorScreenPos();
                const ImVec2 swatchMax(swatchMin.x + 16.0f, swatchMin.y + 16.0f);
                ImGui::Dummy(ImVec2(16.0f, 16.0f));
                ImGui::GetWindowDrawList()->AddRectFilled(swatchMin, swatchMax, colorForPaletteEntry(paletteEntry), 2.0f);
                ImGui::GetWindowDrawList()->AddRect(swatchMin, swatchMax, ImGuiTheme::toU32(ImGuiTheme::textDisabled()), 2.0f);
            }
            ImGui::EndTooltip();
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextUnformatted("CHR / Pattern Tables");
        ImGui::TextDisabled("Left: $0000   Right: $1000");
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(m_ppuChrTexture)),
            ImVec2(static_cast<float>(kChrWidth * 2), static_cast<float>(kChrHeight * 2))
        );

        const ImVec2 chrImageMin = ImGui::GetItemRectMin();
        if(ImGui::IsItemHovered()) {
            const int hoveredX = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.x - chrImageMin.x) / 2.0f), 0, kChrWidth - 1);
            const int hoveredY = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.y - chrImageMin.y) / 2.0f), 0, kChrHeight - 1);
            const int tileX = hoveredX / 8;
            const int tileY = hoveredY / 8;
            const int tableIndex = tileX >= 16 ? 1 : 0;
            const int localTileX = tileX % 16;
            const int tileIndex = tileY * 16 + localTileX;
            const uint16_t tileAddress = static_cast<uint16_t>(tableIndex * 0x1000 + tileIndex * 16);

            const ImVec2 tileMin(chrImageMin.x + tileX * 16.0f, chrImageMin.y + tileY * 16.0f);
            const ImVec2 tileMax(tileMin.x + 16.0f, tileMin.y + 16.0f);
            drawList->AddRect(tileMin, tileMax, ImGuiTheme::toU32(ImGuiTheme::eventWrite()), 0.0f, 0, 2.0f);

            ImGui::BeginTooltip();
            ImGui::Text("CHR tile");
            ImGui::Text("Tile: (%d, %d)", tileX, tileY);
            ImGui::Text("Tile index: $%02X", static_cast<unsigned int>(tileIndex));
            ImGui::Text("Pattern addr: $%04X", static_cast<unsigned int>(tileAddress));
            ImGui::EndTooltip();
        }
        ImGui::EndGroup();
    }
    ImGui::EndChild();

    ImGui::End();
}

inline void GeraNESApp::drawEventViewerWindow()
{
    constexpr int kEventWidth = 341;
    constexpr int kEventHeight = 312;
    constexpr int kVisibleFrameWidth = 256;
    constexpr int kVisibleFrameHeight = 240;
    constexpr int kVisibleFrameXOffset = 1;
    constexpr float kScale = 2.0f;
    constexpr float kEventDotRadius = 2.0f;
    constexpr float kEventHitRadius = 6.0f;

    ImGui::SetNextWindowSize(ImVec2(860.0f, 720.0f), ImGuiCond_Appearing);

    if(!ImGui::Begin("Event Viewer", &m_showEventViewerWindow)) {
        ImGui::End();
        return;
    }

    if(!m_emu.valid()) {
        ImGui::TextDisabled("Load a ROM to inspect PPU events.");
        ImGui::End();
        return;
    }

    bool traceEnabled = m_ppuEventViewerEnabled;
    if(ImGui::Checkbox("Enable Event Viewer", &traceEnabled)) {
        m_ppuEventViewerEnabled = traceEnabled;
        m_emu.withExclusiveAccess([&](auto& emu) {
            emu.enablePpuEventTrace(traceEnabled);
        });
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Reads are blue, writes are red. Full PPU frame: 341x312.");
    ImGui::Separator();

    if(m_ppuEventTexture == 0) {
        glGenTextures(1, &m_ppuEventTexture);
        glBindTexture(GL_TEXTURE_2D, m_ppuEventTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kEventWidth, kEventHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    if(m_ppuEventBuffer.size() != static_cast<size_t>(kEventWidth * kEventHeight)) {
        m_ppuEventBuffer.resize(static_cast<size_t>(kEventWidth * kEventHeight));
    }

    std::vector<GeraNESEmu::PpuRegisterAccessEvent> ppuEvents;
    std::vector<uint32_t> eventFramebuffer;

    m_emu.withExclusiveAccess([&](auto& emu) {
        if(emu.ppuEventTraceEnabled() != m_ppuEventViewerEnabled) {
            emu.enablePpuEventTrace(m_ppuEventViewerEnabled);
        }

        ppuEvents = emu.ppuRegisterAccessEvents();
        const PPU& ppu = emu.getConsole().ppu();
        const uint32_t* framebuffer = ppu.getFramebuffer();
        eventFramebuffer.assign(framebuffer, framebuffer + (kVisibleFrameWidth * kVisibleFrameHeight));
    });

    std::fill(m_ppuEventBuffer.begin(), m_ppuEventBuffer.end(), 0xFF000000u);
    if(!eventFramebuffer.empty()) {
        for(int y = 0; y < kVisibleFrameHeight; ++y) {
            const size_t srcOffset = static_cast<size_t>(y * kVisibleFrameWidth);
            const size_t dstOffset = static_cast<size_t>((y * kEventWidth) + kVisibleFrameXOffset);
            std::copy_n(eventFramebuffer.begin() + static_cast<std::ptrdiff_t>(srcOffset),
                        kVisibleFrameWidth,
                        m_ppuEventBuffer.begin() + static_cast<std::ptrdiff_t>(dstOffset));
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_ppuEventTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kEventWidth, kEventHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_ppuEventBuffer.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    const bool paused = m_emu.paused();
    const uint32_t currentFrame = ppuEvents.empty() ? 0u : ppuEvents.front().frame;
    if(ppuEvents.empty() || m_selectedPpuEventFrame != currentFrame ||
       m_selectedPpuEventIndex < 0 ||
       m_selectedPpuEventIndex >= static_cast<int>(ppuEvents.size())) {
        m_selectedPpuEventIndex = -1;
        m_selectedPpuEventFrame = currentFrame;
    }

    const auto registerName = [](uint16_t address) -> const char* {
        switch(address) {
            case 0x2000: return "PPUCTRL";
            case 0x2001: return "PPUMASK";
            case 0x2002: return "PPUSTATUS";
            case 0x2003: return "OAMADDR";
            case 0x2004: return "OAMDATA";
            case 0x2005: return "PPUSCROLL";
            case 0x2006: return "PPUADDR";
            case 0x2007: return "PPUDATA";
            default: return "PPU?";
        }
    };

    ImGui::Text("Events in current frame: %d", static_cast<int>(ppuEvents.size()));
    ImGui::SameLine();
    ImGui::TextDisabled("Frame %u", currentFrame);
    if(!paused) {
        ImGui::SameLine();
        ImGui::TextDisabled("Pause emulation to inspect event tooltips.");
    }

    if(m_selectedPpuEventIndex >= 0) {
        const auto& selectedEvent = ppuEvents[static_cast<size_t>(m_selectedPpuEventIndex)];
        ImGui::Text(
            "Selected: #%d %s %s ($%04X) value $%02X at scanline %u cycle %u",
            m_selectedPpuEventIndex,
            selectedEvent.isWrite ? "Write" : "Read",
            registerName(selectedEvent.address),
            static_cast<unsigned int>(selectedEvent.address),
            static_cast<unsigned int>(selectedEvent.value),
            static_cast<unsigned int>(selectedEvent.scanline),
            static_cast<unsigned int>(selectedEvent.cycle)
        );
    } else {
        ImGui::TextDisabled("Selected: none");
    }
    ImGui::Separator();

    int hoveredEventIndex = -1;
    bool scrollTableToSelection = false;

    if(ImGui::BeginChild("EventViewerImageScroll", ImVec2(0.0f, 320.0f), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(m_ppuEventTexture)),
            ImVec2(static_cast<float>(kEventWidth) * kScale, static_cast<float>(kEventHeight) * kScale)
        );

        const bool imageHovered = ImGui::IsItemHovered();
        if(m_ppuEventViewerEnabled) {
            const ImVec2 imageMin = ImGui::GetItemRectMin();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float bestDistanceSq = kEventHitRadius * kEventHitRadius;
            const ImVec2 mousePos = ImGui::GetIO().MousePos;

            for(size_t i = 0; i < ppuEvents.size(); ++i) {
                const auto& event = ppuEvents[i];
                if(event.scanline >= kEventHeight) {
                    continue;
                }

                const int x = std::clamp(static_cast<int>(event.cycle), 0, kEventWidth - 1);
                const int y = std::clamp(static_cast<int>(event.scanline), 0, kEventHeight - 1);
                const ImU32 color = event.isWrite
                    ? ImGuiTheme::toU32(ImGuiTheme::eventWrite())
                    : ImGuiTheme::toU32(ImGuiTheme::info());
                const ImVec2 center(
                    imageMin.x + (static_cast<float>(x) + 0.5f) * kScale,
                    imageMin.y + (static_cast<float>(y) + 0.5f) * kScale
                );
                drawList->AddCircleFilled(center, kEventDotRadius, color);

                if(paused && imageHovered) {
                    const float dx = mousePos.x - center.x;
                    const float dy = mousePos.y - center.y;
                    const float distanceSq = (dx * dx) + (dy * dy);
                    if(distanceSq <= bestDistanceSq) {
                        bestDistanceSq = distanceSq;
                        hoveredEventIndex = static_cast<int>(i);
                    }
                }
            }

            if(m_selectedPpuEventIndex >= 0) {
                const auto& selectedEvent = ppuEvents[static_cast<size_t>(m_selectedPpuEventIndex)];
                if(selectedEvent.scanline < kEventHeight) {
                    const int x = std::clamp(static_cast<int>(selectedEvent.cycle), 0, kEventWidth - 1);
                    const int y = std::clamp(static_cast<int>(selectedEvent.scanline), 0, kEventHeight - 1);
                    const ImVec2 center(
                        imageMin.x + (static_cast<float>(x) + 0.5f) * kScale,
                        imageMin.y + (static_cast<float>(y) + 0.5f) * kScale
                    );
                    drawList->AddCircle(center, kEventHitRadius, ImGuiTheme::toU32(ImGuiTheme::warning()), 0, 2.0f);
                }
            }

            if(hoveredEventIndex >= 0) {
                const auto& hoveredEvent = ppuEvents[static_cast<size_t>(hoveredEventIndex)];
                ImGui::BeginTooltip();
                ImGui::Text("#%d  %s", hoveredEventIndex, hoveredEvent.isWrite ? "Write" : "Read");
                ImGui::Text("%s ($%04X)", registerName(hoveredEvent.address), static_cast<unsigned int>(hoveredEvent.address));
                ImGui::Text("Value: $%02X", static_cast<unsigned int>(hoveredEvent.value));
                ImGui::Text("Scanline: %u", static_cast<unsigned int>(hoveredEvent.scanline));
                ImGui::Text("Cycle: %u", static_cast<unsigned int>(hoveredEvent.cycle));
                ImGui::Text("Frame: %u", hoveredEvent.frame);
                ImGui::EndTooltip();
            }

            if(paused && hoveredEventIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_selectedPpuEventIndex = hoveredEventIndex;
                m_selectedPpuEventFrame = currentFrame;
                scrollTableToSelection = true;
            }
        }
    }
    ImGui::EndChild();

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp;

    if(ImGui::BeginTable("EventViewerTable", 6, tableFlags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Scanline", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Cycle", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiTheme::textOnAccent());
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for(size_t i = 0; i < ppuEvents.size(); ++i) {
            const auto& event = ppuEvents[i];
            const bool selected = m_selectedPpuEventIndex == static_cast<int>(i);

            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableSetColumnIndex(0);
            if(ImGui::Selectable("##event-row", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                m_selectedPpuEventIndex = static_cast<int>(i);
                m_selectedPpuEventFrame = currentFrame;
            }
            if(selected && scrollTableToSelection) {
                ImGui::SetScrollHereY(0.5f);
            }
            ImGui::SameLine();
            ImGui::Text("%d", static_cast<int>(i));

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(event.isWrite ? "Write" : "Read");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", registerName(event.address));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("$%02X", static_cast<unsigned int>(event.value));
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", static_cast<unsigned int>(event.scanline));
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%u", static_cast<unsigned int>(event.cycle));

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
