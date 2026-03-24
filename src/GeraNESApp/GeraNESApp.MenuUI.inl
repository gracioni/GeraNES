#pragma once

inline void GeraNESApp::menuBar() {

    bool show_menu = true;

    if (show_menu && ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            auto sc = m_shortcuts.get("openRom");
            if( sc != nullptr) {

                if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str()))
                {
                    sc->action();
                }
            }

            #ifdef __EMSCRIPTEN__
            if (ImGui::BeginMenu("Session")) {

                if(ImGui::MenuItem("Export")) {
                    AppSettings::instance().save();
                    emcriptenExportSession();
                }

                if(ImGui::MenuItem("Import")) {
                    emcriptenImportSession(reinterpret_cast<intptr_t>(this));
                }

                ImGui::EndMenu();
            }
            #endif

            auto recentFiles = AppSettings::instance().data.getRecentFiles();
            if (ImGui::BeginMenu("Recent Files", recentFiles.size() > 0))
            {
                for(int i = 0; i < recentFiles.size(); i++) {
                    if(ImGui::MenuItem(recentFiles[i].c_str())) {
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

        if (ImGui::BeginMenu("Emulator"))
        {
            const bool hasRomLoaded = m_emu.valid();
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
            if(ImGui::MenuItem("Pause", pauseKey, m_emu.paused(), hasRomLoaded)) {
                m_emu.togglePaused();
            }

            ImGui::Separator();

            if(ImGui::MenuItem("Reset", nullptr, false, hasRomLoaded)) {
                m_emu.reset();
            }

            ImGui::EndMenu();
        }

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

                if(ImGui::MenuItem("default", nullptr, AppSettings::instance().data.video.shaderName == "")) {
                    AppSettings::instance().data.video.shaderName = "";
                    updateShaderConfig();
                }

                if(shaderList.size() > 0) ImGui::Separator();

                for(const ShaderItem& item: shaderList) {
                    if(ImGui::MenuItem(item.label.c_str(), nullptr, item.label == AppSettings::instance().data.video.shaderName)) {
                        AppSettings::instance().data.video.shaderName = item.label;
                        updateShaderConfig();
                    }
                }
                ImGui::EndMenu();
            }

            auto sc = m_shortcuts.get("horizontalStretch");
            if( sc != nullptr) {

                if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), m_horizontalStretch))
                {
                    sc->action();
                }
            }

            sc = m_shortcuts.get("fullscreen");
            if( sc != nullptr) {

                if (ImGui::MenuItem(sc->label.c_str(), sc->shortcut.c_str(), isFullScreen()))
                {
                    sc->action();
                }
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

                for(int i = 0; i < m_audioDevices.size(); i++) {

                    bool checked = m_emu.currentAudioDeviceName() == m_audioDevices[i];

                    if(ImGui::MenuItem(m_audioDevices[i].c_str(), nullptr, checked)) {
                        m_emu.configAudioDevice(m_audioDevices[i]);
                        AppSettings::instance().data.audio.audioDevice = m_audioDevices[i];
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

#ifdef ENABLE_NSF_PLAYER
        if (ImGui::BeginMenu("NSF", m_emu.isNsfLoaded()))
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

        if (ImGui::BeginMenu("Input"))
        {
            auto drawControllerPortMenuItem = [this](const char* label, Settings::Port port, Settings::Device device)
            {
                const bool selected = m_emu.getPortDevice(port) == std::optional<Settings::Device>(device);
                if(ImGui::MenuItem(label, nullptr, selected)) {
                    m_emu.setPortDevice(port, device);
                }
            };

            auto drawControllerPortConfigItem = [this](Settings::Port port)
            {
                if(m_emu.getPortDevice(port) != std::optional<Settings::Device>(Settings::Device::CONTROLLER)) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...")) {
                    if(port == Settings::Port::P_1) m_inputBindingConfigWindow.show("Controller 1", m_controller1);
                    else m_inputBindingConfigWindow.show("Controller 2", m_controller2);
                }
            };

            auto drawArkanoidPortConfigItem = [this](Settings::Port port)
            {
                if(m_emu.getPortDevice(port) != std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER)) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...")) {
                    m_showArkanoidNesConfigWindow = true;
                }
            };

            auto drawSnesMousePortConfigItem = [this](Settings::Port port)
            {
                if(m_emu.getPortDevice(port) != std::optional<Settings::Device>(Settings::Device::SNES_MOUSE)) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...")) {
                    m_showSnesMouseConfigWindow = true;
                }
            };

            auto drawKonamiHyperShotConfigItem = [this]()
            {
                if(m_emu.getExpansionDevice() != Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                    return;
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Config...")) {
                    m_showKonamiHyperShotConfigWindow = true;
                }
            };

            if (ImGui::BeginMenu("Port 1")) {
                drawControllerPortMenuItem("Controller", Settings::Port::P_1, Settings::Device::CONTROLLER);
                drawControllerPortMenuItem("Zapper", Settings::Port::P_1, Settings::Device::ZAPPER);
                drawControllerPortMenuItem("Power Pad (Side A)", Settings::Port::P_1, Settings::Device::POWER_PAD_SIDE_A);
                drawControllerPortMenuItem("Power Pad (Side B)", Settings::Port::P_1, Settings::Device::POWER_PAD_SIDE_B);
                drawControllerPortMenuItem("SNES Mouse", Settings::Port::P_1, Settings::Device::SNES_MOUSE);
                drawControllerPortMenuItem("Arkanoid Controller", Settings::Port::P_1, Settings::Device::ARKANOID_CONTROLLER);
                drawControllerPortConfigItem(Settings::Port::P_1);
                drawSnesMousePortConfigItem(Settings::Port::P_1);
                drawArkanoidPortConfigItem(Settings::Port::P_1);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Port 2")) {
                drawControllerPortMenuItem("Controller", Settings::Port::P_2, Settings::Device::CONTROLLER);
                drawControllerPortMenuItem("Zapper", Settings::Port::P_2, Settings::Device::ZAPPER);
                drawControllerPortMenuItem("Power Pad (Side A)", Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_A);
                drawControllerPortMenuItem("Power Pad (Side B)", Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_B);
                drawControllerPortMenuItem("SNES Mouse", Settings::Port::P_2, Settings::Device::SNES_MOUSE);
                drawControllerPortMenuItem("Arkanoid Controller", Settings::Port::P_2, Settings::Device::ARKANOID_CONTROLLER);
                drawControllerPortConfigItem(Settings::Port::P_2);
                drawSnesMousePortConfigItem(Settings::Port::P_2);
                drawArkanoidPortConfigItem(Settings::Port::P_2);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Expansion")) {
                if (ImGui::MenuItem("None", nullptr, m_emu.getExpansionDevice() == Settings::ExpansionDevice::NONE))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::NONE);
                }
                if (ImGui::MenuItem("Bandai Hyper Shot", nullptr, m_emu.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::BANDAI_HYPERSHOT);
                }
                if (ImGui::MenuItem("Konami Hyper Shot", nullptr, m_emu.getExpansionDevice() == Settings::ExpansionDevice::KONAMI_HYPERSHOT))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::KONAMI_HYPERSHOT);
                }
                if (ImGui::MenuItem("Arkanoid Controller (Famicom)", nullptr, m_emu.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER))
                {
                    m_emu.setExpansionDevice(Settings::ExpansionDevice::ARKANOID_CONTROLLER);
                }
                if(m_emu.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                    ImGui::Separator();
                    if(ImGui::MenuItem("Config...")) {
                        m_showArkanoidFamicomConfigWindow = true;
                    }
                }
                drawKonamiHyperShotConfigItem();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Hardware")) {
                if (ImGui::MenuItem("FDS - Switch Disk Side")) {
                    m_emu.fdsSwitchDiskSide();
                }
                if (ImGui::MenuItem("FDS - Eject Disk")) {
                    m_emu.fdsEjectDisk();
                }
                if (ImGui::MenuItem("FDS - Insert Next Disk")) {
                    m_emu.fdsInsertNextDisk();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("VS - Insert Coin 1")) {
                    m_emu.vsInsertCoin(1);
                }
                if (ImGui::MenuItem("VS - Insert Coin 2")) {
                    m_emu.vsInsertCoin(2);
                }
                if (ImGui::MenuItem("VS - Insert Coin 3 (DualSystem)")) {
                    m_emu.vsInsertCoin(3);
                }
                if (ImGui::MenuItem("VS - Insert Coin 4 (DualSystem)")) {
                    m_emu.vsInsertCoin(4);
                }
                if (ImGui::MenuItem("VS - Service Button")) {
                    m_emu.vsServiceButton(1);
                }
                if (ImGui::MenuItem("VS - Service Button 2 (DualSystem)")) {
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

                if (ImGui::BeginMenu("Digital pad mode")) {
                    int digitalPadMode = (int)AppSettings::instance().data.input.touchControls.digitalPadMode;
                    for(int i = (int)DigitaPadMode::Absolute; i <= (int)DigitaPadMode::Relative ; i++) {
                        if(ImGui::MenuItem(DigitaPadModeLabels[i], nullptr, digitalPadMode == i)) {
                            AppSettings::instance().data.input.touchControls.digitalPadMode = (DigitaPadMode)i;
                        }
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Buttons mode")) {
                    int buttonsMode = (int)AppSettings::instance().data.input.touchControls.buttonsMode;
                    for(int i = (int)ButtonsMode::Absolute; i <= (int)ButtonsMode::Column ; i++) {
                        if(ImGui::MenuItem(ButtonsModeLabels[i], nullptr, buttonsMode == i)) {
                            AppSettings::instance().data.input.touchControls.buttonsMode = (ButtonsMode)i;
                        }
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

        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Log"))
            {
                m_showLogWindow = true;
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

        if (ImGui::MenuItem("About"))
        {
            m_showAboutWindow = true;
        }

        ImGui::EndMainMenuBar();
    }

    m_menuBarHeight = ImGui::GetFrameHeight();
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
