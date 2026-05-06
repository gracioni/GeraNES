#pragma once

inline void GeraNESApp::showGui()
{
    const ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();

    float lastMenuBarHeight = m_menuBarHeight;

    if(m_showMenuBar) menuBar();
    else m_menuBarHeight = 0;

    if(lastMenuBarHeight != m_menuBarHeight) updateBuffers();

#ifdef ENABLE_NSF_PLAYER
    if(m_emu.isNsfLoaded()) drawNsfPlayerVisualizer();
#endif

    m_inputBindingConfigWindow.update();
    m_powerPadConfigWindow.update();

    if(m_showImprovementsWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Improvements", &m_showImprovementsWindow, ImGuiWindowFlags_NoResize)) {
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

    if(m_showPaletteWindow) {
        drawPaletteWindow();
    }

    if(m_showShaderStackWindow) {
        drawShaderStackWindow();
    }

    if(m_showCpuDebuggerWindow) {
        drawCpuDebuggerWindow();
    }

    if(m_showCpuBreakpointsWindow) {
        drawCpuBreakpointsWindow();
    }

    if(m_showAboutWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("About", &m_showAboutWindow, ImGuiWindowFlags_NoResize)) {
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

    if(m_showArkanoidNesConfigWindow) {
        SetNextWindowSizeClamped(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Arkanoid Controller Config (NES)", &m_showArkanoidNesConfigWindow, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("The NES Arkanoid paddle uses grabbed relative mouse movement. Press Escape to release the mouse.");
            ImGui::Separator();

            float sensitivity = AppSettings::instance().data.input.arkanoid.sensitivity;
            ImGui::SetNextItemWidth(180.0f);
            if(ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 4.0f, "%.2fx")) {
                AppSettings::instance().data.input.arkanoid.sensitivity = std::clamp(sensitivity, 0.05f, 4.0f);
            }

        }

        ImGui::End();
    }

    if(m_showArkanoidFamicomConfigWindow) {
        SetNextWindowSizeClamped(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Arkanoid Controller Config (Famicom)", &m_showArkanoidFamicomConfigWindow, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("The Famicom Arkanoid paddle uses grabbed relative mouse movement. Press Escape to release the mouse.");
            ImGui::Separator();

            float sensitivity = AppSettings::instance().data.input.arkanoid.sensitivity;
            ImGui::SetNextItemWidth(180.0f);
            if(ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 4.0f, "%.2fx")) {
                AppSettings::instance().data.input.arkanoid.sensitivity = std::clamp(sensitivity, 0.05f, 4.0f);
            }

        }

        ImGui::End();
    }

    if(m_showSnesMouseConfigWindow) {
        SetNextWindowSizeClamped(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Mouse Config", &m_showSnesMouseConfigWindow, ImGuiWindowFlags_NoResize)) {
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

    if(m_showKonamiHyperShotConfigWindow) {
        m_inputBindingConfigWindow.show("Konami Hyper Shot Config", m_konamiHyperShot);
        m_showKonamiHyperShotConfigWindow = false;
    }

    if(m_showNetplayWindow) {
        GeraNESNetplay::drawNetplayWindow(m_showNetplayWindow, m_netplayRuntime, viewportCenter);
    }

    if(m_showRomDatabaseWindow) {
        SetNextWindowSizeClamped(ImVec2(720.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Rom Database");
    }

    if(ImGui::BeginPopupModal("Rom Database", &m_showRomDatabaseWindow)) {
        if(!m_romDbEditor.loaded) {
            ImGui::TextWrapped("%s", m_romDbEditor.statusMessage.c_str());
        }
        else {
                ImVec4 color = m_romDbEditor.foundInDatabase ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
                ImGui::TextColored(color, "%s", m_romDbEditor.statusMessage.c_str());
                ImGui::Separator();

                const bool compareWithSaved = m_romDbEditor.foundInDatabase;
                auto isChanged = [&](const std::string& a, const std::string& b) {
                    return compareWithSaved && a != b;
                };
                auto pushChangedStyle = [](bool changed) {
                    if(!changed) return;
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.08f, 0.08f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.55f, 0.16f, 0.16f, 1.0f));
                };
                auto popChangedStyle = [](bool changed) {
                    if(changed) ImGui::PopStyleColor(3);
                };

                auto drawStringEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<const char*, const char*>>& options, bool changed) {
                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(value == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

                    pushChangedStyle(changed);
                    if(ImGui::BeginCombo(id, options[current].second)) {
                        for(size_t i = 0; i < options.size(); ++i) {
                            const bool selected = static_cast<int>(i) == current;
                            if(ImGui::Selectable(options[i].second, selected)) {
                                value = options[i].first;
                            }
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    popChangedStyle(changed);
                };

                auto drawIntEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<int, const char*>>& options, bool changed) {
                    int parsed = 0;
                    try { parsed = value.empty() ? options.front().first : std::stoi(value); } catch(...) { parsed = options.front().first; }

                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(parsed == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

                    pushChangedStyle(changed);
                    if(ImGui::BeginCombo(id, options[current].second)) {
                        for(size_t i = 0; i < options.size(); ++i) {
                            const bool selected = static_cast<int>(i) == current;
                            if(ImGui::Selectable(options[i].second, selected)) {
                                value = std::to_string(options[i].first);
                            }
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    popChangedStyle(changed);
                };

                bool ch = false;
                ch = isChanged(m_romDbEditor.PrgChrCrc32, m_romDbSaved.PrgChrCrc32);
                pushChangedStyle(ch);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("PrgChrCrc32");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText("##PrgChrCrc32", &m_romDbEditor.PrgChrCrc32, ImGuiInputTextFlags_ReadOnly);
                popChangedStyle(ch);

                auto drawLabelCell = [&](const char* label) {
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                };
                auto drawInputTextCell = [&](const char* id, std::string& value, const std::string& savedValue) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    bool changed = isChanged(value, savedValue);
                    pushChangedStyle(changed);
                    ImGui::InputText(id, &value);
                    popChangedStyle(changed);
                };
                auto drawStringEnumCell = [&](const char* id, std::string& value, const std::string& savedValue, const std::vector<std::pair<const char*, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawStringEnumCombo(id, value, options, isChanged(value, savedValue));
                };
                auto drawIntEnumCell = [&](const char* id, std::string& value, const std::string& savedValue, const std::vector<std::pair<int, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawIntEnumCombo(id, value, options, isChanged(value, savedValue));
                };
                auto drawEmptyPair = [&]() {
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                };

                const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;
                if(ImGui::BeginTable("RomDbFieldsTable", 4, tableFlags)) {
                    ImGui::TableSetupColumn("LabelA", ImGuiTableColumnFlags_WidthFixed, 115.0f);
                    ImGui::TableSetupColumn("ValueA", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("LabelB", ImGuiTableColumnFlags_WidthFixed, 115.0f);
                    ImGui::TableSetupColumn("ValueB", ImGuiTableColumnFlags_WidthStretch, 1.0f);

                    ImGui::TableNextRow();
                    drawLabelCell("System");
                    drawStringEnumCell("##System", m_romDbEditor.System, m_romDbSaved.System, {
                        {"", "Default"},
                        {"NesNtsc", "NesNtsc"},
                        {"NesPal", "NesPal"},
                        {"Famicom", "Famicom"},
                        {"Dendy", "Dendy"},
                        {"VsSystem", "VsSystem"},
                        {"Playchoice", "Playchoice"},
                        {"FDS", "FDS"},
                        {"FamicomNetworkSystem", "FamicomNetworkSystem"},
                    });
                    drawLabelCell("Mapper");
                    drawInputTextCell("##Mapper", m_romDbEditor.Mapper, m_romDbSaved.Mapper);

                    ImGui::TableNextRow();
                    drawLabelCell("Board");
                    drawInputTextCell("##Board", m_romDbEditor.Board, m_romDbSaved.Board);
                    drawLabelCell("SubMapperId");
                    drawInputTextCell("##SubMapperId", m_romDbEditor.SubMapperId, m_romDbSaved.SubMapperId);

                    ImGui::TableNextRow();
                    drawLabelCell("PCB");
                    drawInputTextCell("##PCB", m_romDbEditor.PCB, m_romDbSaved.PCB);
                    drawLabelCell("Mirroring");
                    drawStringEnumCell("##Mirroring", m_romDbEditor.Mirroring, m_romDbSaved.Mirroring, {
                        {"", "Default"},
                        {"h", "Horizontal"},
                        {"v", "Vertical"},
                        {"4", "Four Screen"},
                        {"0", "Single Screen A"},
                        {"1", "Single Screen B"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("Chip");
                    drawInputTextCell("##Chip", m_romDbEditor.Chip, m_romDbSaved.Chip);
                    drawLabelCell("InputType");
                    drawIntEnumCell("##InputType", m_romDbEditor.InputType, m_romDbSaved.InputType, {
                        {0, "Unspecified"}, {1, "StandardControllers"}, {2, "FourScore"}, {3, "FourPlayerAdapter"},
                        {4, "VsSystem"}, {5, "VsSystemSwapped"}, {6, "VsSystemSwapAB"}, {7, "VsZapper"},
                        {8, "Zapper"}, {9, "TwoZappers"}, {10, "BandaiHypershot"}, {11, "PowerPadSideA"},
                        {12, "PowerPadSideB"}, {13, "FamilyTrainerSideA"}, {14, "FamilyTrainerSideB"},
                        {15, "ArkanoidControllerNes"}, {16, "ArkanoidControllerFamicom"}, {17, "DoubleArkanoidController"},
                        {18, "KonamiHyperShot"}, {19, "PachinkoController"}, {20, "ExcitingBoxing"},
                        {21, "JissenMahjong"}, {22, "PartyTap"}, {23, "OekaKidsTablet"}, {24, "BarcodeBattler"},
                        {25, "MiraclePiano"}, {26, "PokkunMoguraa"}, {27, "TopRider"}, {28, "DoubleFisted"},
                        {29, "Famicom3dSystem"}, {30, "DoremikkoKeyboard"}, {31, "ROB"}, {32, "FamicomDataRecorder"},
                        {33, "TurboFile"}, {34, "BattleBox"}, {35, "FamilyBasicKeyboard"}, {36, "Pec586Keyboard"},
                        {37, "Bit79Keyboard"}, {38, "SuborKeyboard"}, {39, "SuborKeyboardMouse1"},
                        {40, "SuborKeyboardMouse2"}, {41, "SnesMouse"}, {42, "GenericMulticart"},
                        {43, "SnesControllers"}, {44, "RacermateBicycle"}, {45, "UForce"}
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("PrgRomSize");
                    drawInputTextCell("##PrgRomSize", m_romDbEditor.PrgRomSize, m_romDbSaved.PrgRomSize);
                    drawLabelCell("VsSystemType");
                    drawIntEnumCell("##VsSystemType", m_romDbEditor.VsSystemType, m_romDbSaved.VsSystemType, {
                        {0, "Default"},
                        {1, "RbiBaseballProtection"},
                        {2, "TkoBoxingProtection"},
                        {3, "SuperXeviousProtection"},
                        {4, "IceClimberProtection"},
                        {5, "VsDualSystem"},
                        {6, "RaidOnBungelingBayProtection"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRomSize");
                    drawInputTextCell("##ChrRomSize", m_romDbEditor.ChrRomSize, m_romDbSaved.ChrRomSize);
                    drawLabelCell("VsPpuModel");
                    drawIntEnumCell("##VsPpuModel", m_romDbEditor.VsPpuModel, m_romDbSaved.VsPpuModel, {
                        {0, "Ppu2C02"},
                        {1, "Ppu2C03"},
                        {2, "Ppu2C04A"},
                        {3, "Ppu2C04B"},
                        {4, "Ppu2C04C"},
                        {5, "Ppu2C04D"},
                        {6, "Ppu2C05A"},
                        {7, "Ppu2C05B"},
                        {8, "Ppu2C05C"},
                        {9, "Ppu2C05D"},
                        {10, "Ppu2C05E"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("SaveRamSize");
                    drawInputTextCell("##SaveRamSize", m_romDbEditor.SaveRamSize, m_romDbSaved.SaveRamSize);
                    drawLabelCell("BusConflicts");
                    drawStringEnumCell("##BusConflicts", m_romDbEditor.BusConflicts, m_romDbSaved.BusConflicts, {
                        {"", "Default"},
                        {"Y", "Yes"},
                        {"N", "No"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRamSize");
                    drawInputTextCell("##ChrRamSize", m_romDbEditor.ChrRamSize, m_romDbSaved.ChrRamSize);
                    drawEmptyPair();

                    ImGui::TableNextRow();
                    drawLabelCell("WorkRamSize");
                    drawInputTextCell("##WorkRamSize", m_romDbEditor.WorkRamSize, m_romDbSaved.WorkRamSize);
                    drawLabelCell("HasBattery");
                    ImGui::TableNextColumn();
                    {
                        bool hasBattery = m_romDbEditor.HasBattery == "1";
                        const bool changed = isChanged(m_romDbEditor.HasBattery, m_romDbSaved.HasBattery);
                        pushChangedStyle(changed);
                        if(ImGui::Checkbox("##HasBattery", &hasBattery)) {
                            m_romDbEditor.HasBattery = hasBattery ? "1" : "0";
                        }
                        popChangedStyle(changed);
                    }

                    ImGui::EndTable();
                }

                const bool hasChanges = !compareWithSaved
                    || isChanged(m_romDbEditor.PrgChrCrc32, m_romDbSaved.PrgChrCrc32)
                    || isChanged(m_romDbEditor.System, m_romDbSaved.System)
                    || isChanged(m_romDbEditor.Board, m_romDbSaved.Board)
                    || isChanged(m_romDbEditor.PCB, m_romDbSaved.PCB)
                    || isChanged(m_romDbEditor.Chip, m_romDbSaved.Chip)
                    || isChanged(m_romDbEditor.Mapper, m_romDbSaved.Mapper)
                    || isChanged(m_romDbEditor.PrgRomSize, m_romDbSaved.PrgRomSize)
                    || isChanged(m_romDbEditor.ChrRomSize, m_romDbSaved.ChrRomSize)
                    || isChanged(m_romDbEditor.ChrRamSize, m_romDbSaved.ChrRamSize)
                    || isChanged(m_romDbEditor.WorkRamSize, m_romDbSaved.WorkRamSize)
                    || isChanged(m_romDbEditor.SaveRamSize, m_romDbSaved.SaveRamSize)
                    || isChanged(m_romDbEditor.HasBattery, m_romDbSaved.HasBattery)
                    || isChanged(m_romDbEditor.Mirroring, m_romDbSaved.Mirroring)
                    || isChanged(m_romDbEditor.InputType, m_romDbSaved.InputType)
                    || isChanged(m_romDbEditor.BusConflicts, m_romDbSaved.BusConflicts)
                    || isChanged(m_romDbEditor.SubMapperId, m_romDbSaved.SubMapperId)
                    || isChanged(m_romDbEditor.VsSystemType, m_romDbSaved.VsSystemType)
                    || isChanged(m_romDbEditor.VsPpuModel, m_romDbSaved.VsPpuModel);

                ImGui::Separator();
                const float saveButtonWidth = 120.0f;
                const float removeButtonWidth = 120.0f;

                ImGui::BeginDisabled(compareWithSaved && !hasChanges);
                if(ImGui::Button("Save", ImVec2(saveButtonWidth, 0))) {
                    ImGui::OpenPopup("Confirm Save ROM Database Entry");
                }
                ImGui::EndDisabled();

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - removeButtonWidth);
                ImGui::BeginDisabled(!m_romDbEditor.foundInDatabase);
                if(ImGui::Button("Remove", ImVec2(removeButtonWidth, 0))) {
                    ImGui::OpenPopup("Confirm Remove ROM Database Entry");
                }
                ImGui::EndDisabled();

                ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if(ImGui::BeginPopupModal("Confirm Save ROM Database Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped("Are you sure you want to save this entry to db.txt?");
                    ImGui::Spacing();
                    if(ImGui::Button("Yes", ImVec2(120, 0))) {
                        saveRomDatabaseEditor();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("No", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if(ImGui::BeginPopupModal("Confirm Remove ROM Database Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped("Are you sure you want to remove this entry from db.txt?");
                    ImGui::Spacing();
                    if(ImGui::Button("Yes", ImVec2(120, 0))) {
                        removeRomDatabaseEditor();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("No", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
        }
        ImGui::EndPopup();
    }
    if(!ImGui::IsPopupOpen("Rom Database")) {
        m_showRomDatabaseWindow = false;
    }

    if(m_showErrorWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        bool lastState = m_showErrorWindow;

        if(ImGui::Begin("Error", &m_showErrorWindow)) {
            float windowWidth = ImGui::GetContentRegionAvail().x;

            TextCenteredWrapped(m_errorMessage.c_str());

            ImGui::Spacing();
            ImGui::Spacing();

            const char* btnLabel = "OK";

            ImVec2 btnSize = ImGui::CalcTextSize(btnLabel);
            btnSize.x += ImGui::GetStyle().FramePadding.x * 2.0f;
            btnSize.y += ImGui::GetStyle().FramePadding.y * 2.0f;

            float posX = (windowWidth - btnSize.x) * 0.5f;
            ImGui::SetCursorPosX(posX);

            if(ImGui::Button(btnLabel, btnSize)) {
                m_showErrorWindow = false;
            }
        }
        ImGui::End();

        if(lastState && !m_showErrorWindow) m_showErrorWindow = false;
    }

    if(m_showLogWindow) {
        SetNextWindowSizeClamped(ImVec2(600.0f, 0.0f), ImGuiCond_Once);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Log", &m_showLogWindow)) {
            InputTextMultilineLog("##LogOuterChild", "##LogMultilineInput", m_logBuf.data(), m_logBuf.size(), ImVec2(-1, 400));

            ImGui::Spacing();

            const char* copyBtnLabel = "Copy";
            const char* clearBtnLabel = "Clear";
            const ImGuiStyle& style = ImGui::GetStyle();
            const ImVec2 copyBtnTextSize = ImGui::CalcTextSize(copyBtnLabel);
            const ImVec2 clearBtnTextSize = ImGui::CalcTextSize(clearBtnLabel);
            const ImVec2 copyBtnSize = ImVec2(copyBtnTextSize.x + style.FramePadding.x * 2.0f,
                                              copyBtnTextSize.y + style.FramePadding.y * 2.0f);
            const ImVec2 clearBtnSize = ImVec2(clearBtnTextSize.x + style.FramePadding.x * 2.0f,
                                               clearBtnTextSize.y + style.FramePadding.y * 2.0f);

            const float buttonRowWidth = copyBtnSize.x + style.ItemSpacing.x + clearBtnSize.x;
            const float windowWidth = ImGui::GetContentRegionAvail().x;
            const float posX = (windowWidth - buttonRowWidth) * 0.5f;
            ImGui::SetCursorPosX(posX);

            if(ImGui::Button(copyBtnLabel, copyBtnSize)) {
#ifdef __EMSCRIPTEN__
                emcriptenCopyTextToClipboardExact(m_log.c_str());
#else
                ImGui::SetClipboardText(m_log.c_str());
#endif
            }
            ImGui::SameLine();
            if(ImGui::Button(clearBtnLabel, clearBtnSize)) {
                m_log.clear();
                m_logBuf.clear();
                m_logBuf.push_back('\0');
            }
        }

        ImGui::End();
    }
}

#ifdef ENABLE_NSF_PLAYER
inline void GeraNESApp::drawNsfPlayerVisualizer()
{
    m_nsfVisualizer.draw(
        m_audioOutput.getRecentMixedSamples(),
        m_audioOutput.outputSampleRate(),
        m_menuBarHeight,
        width(),
        height(),
        m_emu.nsfCurrentSong(),
        m_emu.nsfTotalSongs(),
        m_emu.nsfIsPlaying(),
        m_emu.nsfIsPaused(),
        m_emu.nsfHasEnded(),
        m_lastMainLoopDtMs,
        m_fontNsfTitle,
        m_fontNsfSubtitle
    );
}
#endif

inline void DrawCpuBreakpointHitSummary(const GeraNESEmu::DebugBreakpointHit& hit)
{
    if(!hit.valid) {
        ImGui::TextDisabled("No breakpoint hit yet.");
        return;
    }

    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.25f, 1.0f), "%s", hit.reason.c_str());
    if(hit.hasAddress) {
        ImGui::Text(
            "%s $%04X = %02X  Frame %u  CPU %u  PPU %d,%d",
            hit.isWrite ? "Write" : "Read",
            hit.address,
            hit.value,
            hit.frame,
            hit.cpuCycle,
            hit.ppuScanline,
            hit.ppuCycle
        );
    } else {
        ImGui::Text(
            "Frame %u  CPU %u  PPU %d,%d",
            hit.frame,
            hit.cpuCycle,
            hit.ppuScanline,
            hit.ppuCycle
        );
    }
}

inline void GeraNESApp::drawCpuDebuggerWindow()
{
    SetNextWindowSizeClamped(ImVec2(860.0f, 620.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(!ImGui::Begin("CPU Debugger", &m_showCpuDebuggerWindow)) {
        AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;
        ImGui::End();
        return;
    }

    AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;

    const bool hasRomLoaded = m_emu.valid();
    const auto netplaySnapshot = m_netplayRuntime.uiSnapshot();
    const bool debugBlockedByNetplay =
        netplaySnapshot.active ||
        netplaySnapshot.hosting ||
        netplaySnapshot.connected ||
        netplaySnapshot.reconnecting;

    bool debugEnabled = AppSettings::instance().data.debug.cpuDebuggerEnabled;
    ImGui::BeginDisabled(!hasRomLoaded || debugBlockedByNetplay);
    if(ImGui::Checkbox("Enable debugger", &debugEnabled)) {
        AppSettings::instance().data.debug.cpuDebuggerEnabled = debugEnabled;
        if(debugEnabled && !m_emu.paused()) {
            m_emu.togglePaused();
        } else if(!debugEnabled) {
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.clearDebugBreakpointHit();
            });
            if(m_emu.paused()) {
                m_emu.togglePaused();
            }
        }
    }
    ImGui::EndDisabled();

    if(!hasRomLoaded) {
        ImGui::TextDisabled("Load a ROM to inspect CPU state.");
        ImGui::End();
        return;
    }

    if(debugBlockedByNetplay) {
        ImGui::TextDisabled("CPU debugging is disabled while netplay is active.");
        ImGui::End();
        return;
    }

    if(!debugEnabled) {
        ImGui::TextDisabled("Enable the debugger to pause, step, and inspect CPU state.");
        ImGui::End();
        return;
    }

    const bool paused = m_emu.paused();
    if(paused) {
        if(ImGui::Button("Resume")) {
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.clearDebugBreakpointHit();
            });
            m_emu.togglePaused();
        }
    } else {
        if(ImGui::Button("Pause")) {
            m_emu.togglePaused();
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    if(ImGui::Button("Step")) {
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.clearDebugBreakpointHit();
            emu.debugStepInstruction();
        });
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled(paused ? "Stopped" : "Running");

    ImGui::SameLine();
    if(ImGui::Button("Breakpoints")) {
        m_showCpuBreakpointsWindow = true;
        AppSettings::instance().data.debug.showCpuBreakpoints = true;
    }

    CPU2A03::DebugState cpuState;
    GeraNESEmu::ExecutionPoint execPoint;
    GeraNESEmu::DebugBreakpointHit breakpointHit;
    std::vector<CPU2A03DebugLine> disassembly;
    m_emu.withExclusiveAccess([&](auto& emu) {
        cpuState = emu.getConsole().cpu().debugState();
        execPoint = emu.executionPoint();
        breakpointHit = emu.debugBreakpointHit();
        disassembly = CPU2A03Debug::disassembleAround(
            cpuState.pc,
            8,
            24,
            [&](uint16_t addr) {
                return emu.debugPeekCpuMemory(addr);
            }
        );
    });

    ImGui::Separator();
    DrawCpuBreakpointHitSummary(breakpointHit);

    if(breakpointHit.valid) {
        ImGui::SameLine();
        if(ImGui::Button("Clear Hit")) {
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.clearDebugBreakpointHit();
            });
        }
    }

    ImGui::Separator();

    if(ImGui::BeginTable("CpuRegisters", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("PC  %04X", cpuState.pc);
        ImGui::TableNextColumn(); ImGui::Text("A   %02X", cpuState.a);
        ImGui::TableNextColumn(); ImGui::Text("X   %02X", cpuState.x);
        ImGui::TableNextColumn(); ImGui::Text("Y   %02X", cpuState.y);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("SP  %02X", cpuState.sp);
        ImGui::TableNextColumn(); ImGui::Text("P   %02X", cpuState.status);
        ImGui::TableNextColumn(); ImGui::Text("Flags  %s", CPU2A03Debug::formatStatus(cpuState.status).c_str());
        ImGui::TableNextColumn(); ImGui::Text("Frame  %u", execPoint.frame);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("CPU Cycles  %u", cpuState.cycleCounter);
        ImGui::TableNextColumn(); ImGui::Text("Tick  %llu", static_cast<unsigned long long>(execPoint.emulationTick));
        ImGui::TableNextColumn(); ImGui::Text("Pending Cycles  %u", execPoint.cpuCyclesRemaining);
        ImGui::TableNextColumn(); ImGui::Text("Last Opcode  %02X", cpuState.opcode);
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Disassembly");

    if(ImGui::BeginChild("CpuDisassembly", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        for(const CPU2A03DebugLine& line : disassembly) {
            if(line.isCurrent) {
                ImGui::TextColored(
                    ImVec4(0.95f, 0.85f, 0.25f, 1.0f),
                    "> %04X  %-8s  %s",
                    line.address,
                    line.bytes.c_str(),
                    line.mnemonic.c_str()
                );
            } else {
                ImGui::Text(
                    "  %04X  %-8s  %s",
                    line.address,
                    line.bytes.c_str(),
                    line.mnemonic.c_str()
                );
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

inline void GeraNESApp::drawCpuBreakpointsWindow()
{
    SetNextWindowSizeClamped(ImVec2(620.0f, 620.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(!ImGui::Begin("CPU Breakpoints", &m_showCpuBreakpointsWindow)) {
        AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;
        ImGui::End();
        return;
    }

    AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;

    const bool hasRomLoaded = m_emu.valid();
    const auto netplaySnapshot = m_netplayRuntime.uiSnapshot();
    const bool debugBlockedByNetplay =
        netplaySnapshot.active ||
        netplaySnapshot.hosting ||
        netplaySnapshot.connected ||
        netplaySnapshot.reconnecting;
    const bool debugEnabled = AppSettings::instance().data.debug.cpuDebuggerEnabled;

    GeraNESEmu::DebugBreakpointConfig config;
    GeraNESEmu::DebugBreakpointHit hit;
    m_emu.withExclusiveAccess([&](auto& emu) {
        config = emu.debugBreakpointConfig();
        hit = emu.debugBreakpointHit();
    });

    if(!hasRomLoaded) {
        ImGui::TextDisabled("Load a ROM to configure CPU breakpoints.");
        ImGui::End();
        return;
    }

    if(debugBlockedByNetplay) {
        ImGui::TextDisabled("CPU breakpoints are disabled while netplay is active.");
        ImGui::End();
        return;
    }

    if(!debugEnabled) {
        ImGui::TextDisabled("Enable the CPU debugger first. Breakpoints only arm while debug mode is enabled.");
        ImGui::End();
        return;
    }

    bool configChanged = false;

    configChanged |= ImGui::Checkbox("Enable event breakpoints", &config.enabled);
    ImGui::Separator();
    ImGui::TextUnformatted("Events");
    configChanged |= ImGui::Checkbox("PPU NMI start", &config.breakOnNmiStart);
    configChanged |= ImGui::Checkbox("PPU NMI end", &config.breakOnNmiEnd);
    configChanged |= ImGui::Checkbox("IRQ start", &config.breakOnIrqStart);
    configChanged |= ImGui::Checkbox("IRQ end", &config.breakOnIrqEnd);
    configChanged |= ImGui::Checkbox("Sprite zero hit", &config.breakOnSpriteZeroHit);
    configChanged |= ImGui::Checkbox("OAM DMA start ($4014)", &config.breakOnOamDmaStart);
    configChanged |= ImGui::Checkbox("DMC DMA start", &config.breakOnDmcDmaStart);

    ImGui::Separator();
    ImGui::TextUnformatted("Register Access");
    configChanged |= ImGui::Checkbox("PPU register writes", &config.breakOnPpuRegisterWrite);
    configChanged |= ImGui::Checkbox("PPU register reads", &config.breakOnPpuRegisterRead);
    configChanged |= ImGui::Checkbox("APU register writes", &config.breakOnApuRegisterWrite);
    configChanged |= ImGui::Checkbox("APU register reads", &config.breakOnApuRegisterRead);
    configChanged |= ImGui::Checkbox("Controller reads", &config.breakOnControllerRead);
    configChanged |= ImGui::Checkbox("Controller writes", &config.breakOnControllerWrite);
    configChanged |= ImGui::Checkbox("Mapper register writes", &config.breakOnMapperRegisterWrite);
    configChanged |= ImGui::Checkbox("Mapper register reads", &config.breakOnMapperRegisterRead);

    ImGui::Separator();
    ImGui::TextUnformatted("Exact CPU Watch");
    configChanged |= ImGui::Checkbox("Break on CPU read", &config.breakOnExactCpuRead);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    configChanged |= ImGui::InputScalar("##ExactCpuReadAddress", ImGuiDataType_U16, &config.exactCpuReadAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
    configChanged |= ImGui::Checkbox("Break on CPU write", &config.breakOnExactCpuWrite);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    configChanged |= ImGui::InputScalar("##ExactCpuWriteAddress", ImGuiDataType_U16, &config.exactCpuWriteAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);

    if(configChanged) {
        m_emu.withExclusiveAccess([&](auto& emu) {
            emu.setDebugBreakpointConfig(config);
        });
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Last Hit");
    DrawCpuBreakpointHitSummary(hit);
    if(hit.valid && ImGui::Button("Clear Breakpoint Hit")) {
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.clearDebugBreakpointHit();
        });
    }

    ImGui::End();
}

inline void GeraNESApp::showOverlay()
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if(AppSettings::instance().data.debug.showFps) {
        const int fontSize = 32;
        ImFont* fpsFont = m_fontFps != nullptr ? m_fontFps : ImGui::GetFont();

        std::string fpsText = std::to_string(m_fps);
        ImVec2 fpsTextSize = fpsFont->CalcTextSizeA(fontSize, FLT_MAX, 0, fpsText.c_str());

        const ImVec2 pos = ImVec2(width() - fpsTextSize.x - 32, 40);

        DrawTextOutlined(drawList, fpsFont, fontSize, pos, 0xFFFFFFFF, 0xFF000000, fpsText.c_str());
    }
    m_userToast.draw(drawList, static_cast<float>(width()), static_cast<float>(height()), m_fontToast);

    m_touch->draw(drawList);
}
