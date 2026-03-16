#pragma once

inline void GeraNESApp::showGui()
{
    const ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();

    float lastMenuBarHeight = m_menuBarHeight;

    if(m_showMenuBar) menuBar();
    else m_menuBarHeight = 0;

    if(lastMenuBarHeight != m_menuBarHeight) updateBuffers();

    if(m_emu.isNsfLoaded()) drawNsfPlayerVisualizer();

    m_controllerConfigWindow.update();

    if(m_showImprovementsWindow) {
        ImGui::SetNextWindowSize(ImVec2(320, 0));
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

            int value = AppSettings::instance().data.improvements.maxRewindTime;
            if(ImGui::InputInt("Max Rewind Time(s)", &value)) {
                value = std::max(0, value);
                AppSettings::instance().data.improvements.maxRewindTime = value;
                m_emu.setupRewindSystem(value > 0, value);
            }
        }

        ImGui::End();
    }

    if(m_showAboutWindow) {
        ImGui::SetNextWindowSize(ImVec2(320, 0));
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

    if(m_showRomDatabaseWindow) {
        ImGui::SetNextWindowSize(ImVec2(720, 0), ImGuiCond_Appearing);
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
        ImGui::SetNextWindowSize(ImVec2(320, 0));
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
        ImGui::SetNextWindowSize(ImVec2(600, 0), ImGuiCond_Once);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Log", &m_showLogWindow)) {
            constexpr const char* kLogTextId = "##LogMultilineInput";
            ImGui::InputTextMultiline(kLogTextId, m_logBuf.data(), m_logBuf.size(),
                    ImVec2(-1, 400), ImGuiInputTextFlags_ReadOnly);

            if(!(ImGui::IsItemActive() || ImGui::IsItemEdited())) {
                ImGuiContext& g = *GImGui;
                const char* child_window_name = NULL;
                ImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g.CurrentWindow->Name, kLogTextId, ImGui::GetID(kLogTextId));
                ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);

                if(child_window) {
                    ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
                }
            }

            ImGui::Spacing();

            const char* btnLabel = "Clear";
            ImVec2 btnTextSize = ImGui::CalcTextSize(btnLabel);
            ImVec2 btnSize = ImVec2(btnTextSize.x + ImGui::GetStyle().FramePadding.x * 2.0f,
                                    btnTextSize.y + ImGui::GetStyle().FramePadding.y * 2.0f);

            float windowWidth = ImGui::GetContentRegionAvail().x;
            float posX = (windowWidth - btnSize.x) * 0.5f;
            ImGui::SetCursorPosX(posX);

            if(ImGui::Button(btnLabel, btnSize)) {
                m_log.clear();
                m_logBuf.clear();
                m_logBuf.push_back('\0');
            }
        }

        ImGui::End();
    }
}

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
        m_fontNsfTitle,
        m_fontNsfSubtitle
    );
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
