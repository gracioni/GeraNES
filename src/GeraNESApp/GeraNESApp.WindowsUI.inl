#pragma once

#include "GeraNESApp/GeraNESApp.CustomWindowChromeUI.inl"
#include "GeraNESApp/GeraNESApp.NsfPlayerVisualizerUI.inl"
#include "GeraNESApp/GeraNESApp.CpuDebugSharedUI.inl"
#include "GeraNESApp/GeraNESApp.MemoryViewerWindowUI.inl"
#include "GeraNESApp/GeraNESApp.MemoryCompareWindowUI.inl"
#include "GeraNESApp/GeraNESApp.CpuDebuggerWindowUI.inl"
#include "GeraNESApp/GeraNESApp.CpuBreakpointsWindowUI.inl"

inline void GeraNESApp::showGui()
{
    const ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();

    float lastMenuBarHeight = m_menuBarHeight;

    drawCustomWindowChrome();
    if(m_showMenuBar) menuBar();
    else m_menuBarHeight = 0;

    if(lastMenuBarHeight != m_menuBarHeight) updateBuffers();
    m_imGuiWindowFocusBlocksEmulator = false;

#ifdef ENABLE_NSF_PLAYER
    if(m_emu.isNsfLoaded()) drawNsfPlayerVisualizer();
#endif

    m_inputBindingConfigWindow.update();
    m_powerPadConfigWindow.update();

    if(m_showImprovementsWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

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

    if(m_showPaletteWindow) {
        drawPaletteWindow();
    }

    if(m_showShaderStackWindow) {
        drawShaderStackWindow();
    }

    if(m_showPpuViewerWindow) {
        drawPpuViewerWindow();
    } else {
        m_emu.setPpuViewerCaptureEnabled(false, false);
        if(m_ppuViewerScanlineTraceActive) {
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.enablePpuViewerScanlineTrace(false);
            });
            m_ppuViewerScanlineTraceActive = false;
            m_ppuViewerScanlineStates.clear();
            m_ppuViewerScanlineSnapshots.clear();
        }
    }

    if(m_showModPixelInspectorWindow) {
        drawModPixelInspectorWindow();
    }

    if(m_showEventViewerWindow) {
        drawEventViewerWindow();
    } else {
        m_ppuEventViewerEnabled = false;
        m_emu.setPpuEventViewerCaptureEnabled(false);
    }

    if(m_showMemoryViewerWindow) {
        drawMemoryViewerWindow();
    }

    if(m_showMemoryCompareWindow) {
        drawMemoryCompareWindow();
    }

    if(m_showCpuDebuggerWindow) {
        drawCpuDebuggerWindow();
    }

    if(m_showCpuBreakpointsWindow) {
        drawCpuBreakpointsWindow();
    } else {
        m_cpuBreakpointsFocused = false;
    }

    if(m_showAboutWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("About", &m_showAboutWindow, ImGuiWindowFlags_NoResize)) {
            m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
            m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
            m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
            m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
                const ImVec4 statusColor = m_romDbEditor.foundInDatabase
                    ? ImGuiTheme::success()
                    : ImGuiTheme::accentActive();
                ImGui::TextColored(statusColor, "%s", m_romDbEditor.statusMessage.c_str());
                ImGui::Separator();

                const bool compareWithSaved = m_romDbEditor.foundInDatabase;
                auto isChanged = [&](const std::string& a, const std::string& b) {
                    return compareWithSaved && a != b;
                };

                auto drawStringEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<const char*, const char*>>& options) {
                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(value == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

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
                };

                auto drawIntEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<int, const char*>>& options) {
                    int parsed = 0;
                    try { parsed = value.empty() ? options.front().first : std::stoi(value); } catch(...) { parsed = options.front().first; }

                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(parsed == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

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
                };

                bool ch = false;
                ch = isChanged(m_romDbEditor.PrgChrCrc32, m_romDbSaved.PrgChrCrc32);
                if(ch) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                }
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("PrgChrCrc32");
                if(ch) {
                    ImGui::PopStyleColor();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText("##PrgChrCrc32", &m_romDbEditor.PrgChrCrc32, ImGuiInputTextFlags_ReadOnly);

                auto drawLabelCell = [&](const char* label, bool changed = false) {
                    ImGui::TableNextColumn();
                    if(changed) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    }
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    if(changed) {
                        ImGui::PopStyleColor();
                    }
                };
                auto drawInputTextCell = [&](const char* id, std::string& value) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText(id, &value);
                };
                auto drawStringEnumCell = [&](const char* id, std::string& value, const std::vector<std::pair<const char*, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawStringEnumCombo(id, value, options);
                };
                auto drawIntEnumCell = [&](const char* id, std::string& value, const std::vector<std::pair<int, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawIntEnumCombo(id, value, options);
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
                    drawLabelCell("System", isChanged(m_romDbEditor.System, m_romDbSaved.System));
                    drawStringEnumCell("##System", m_romDbEditor.System, {
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
                    drawLabelCell("Mapper", isChanged(m_romDbEditor.Mapper, m_romDbSaved.Mapper));
                    drawInputTextCell("##Mapper", m_romDbEditor.Mapper);

                    ImGui::TableNextRow();
                    drawLabelCell("Board", isChanged(m_romDbEditor.Board, m_romDbSaved.Board));
                    drawInputTextCell("##Board", m_romDbEditor.Board);
                    drawLabelCell("SubMapperId", isChanged(m_romDbEditor.SubMapperId, m_romDbSaved.SubMapperId));
                    drawInputTextCell("##SubMapperId", m_romDbEditor.SubMapperId);

                    ImGui::TableNextRow();
                    drawLabelCell("PCB", isChanged(m_romDbEditor.PCB, m_romDbSaved.PCB));
                    drawInputTextCell("##PCB", m_romDbEditor.PCB);
                    drawLabelCell("Mirroring", isChanged(m_romDbEditor.Mirroring, m_romDbSaved.Mirroring));
                    drawStringEnumCell("##Mirroring", m_romDbEditor.Mirroring, {
                        {"", "Default"},
                        {"h", "Horizontal"},
                        {"v", "Vertical"},
                        {"4", "Four Screen"},
                        {"0", "Single Screen A"},
                        {"1", "Single Screen B"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("Chip", isChanged(m_romDbEditor.Chip, m_romDbSaved.Chip));
                    drawInputTextCell("##Chip", m_romDbEditor.Chip);
                    drawLabelCell("InputType", isChanged(m_romDbEditor.InputType, m_romDbSaved.InputType));
                    drawIntEnumCell("##InputType", m_romDbEditor.InputType, {
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
                    drawLabelCell("PrgRomSize", isChanged(m_romDbEditor.PrgRomSize, m_romDbSaved.PrgRomSize));
                    drawInputTextCell("##PrgRomSize", m_romDbEditor.PrgRomSize);
                    drawLabelCell("VsSystemType", isChanged(m_romDbEditor.VsSystemType, m_romDbSaved.VsSystemType));
                    drawIntEnumCell("##VsSystemType", m_romDbEditor.VsSystemType, {
                        {0, "Default"},
                        {1, "RbiBaseballProtection"},
                        {2, "TkoBoxingProtection"},
                        {3, "SuperXeviousProtection"},
                        {4, "IceClimberProtection"},
                        {5, "VsDualSystem"},
                        {6, "RaidOnBungelingBayProtection"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRomSize", isChanged(m_romDbEditor.ChrRomSize, m_romDbSaved.ChrRomSize));
                    drawInputTextCell("##ChrRomSize", m_romDbEditor.ChrRomSize);
                    drawLabelCell("VsPpuModel", isChanged(m_romDbEditor.VsPpuModel, m_romDbSaved.VsPpuModel));
                    drawIntEnumCell("##VsPpuModel", m_romDbEditor.VsPpuModel, {
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
                    drawLabelCell("SaveRamSize", isChanged(m_romDbEditor.SaveRamSize, m_romDbSaved.SaveRamSize));
                    drawInputTextCell("##SaveRamSize", m_romDbEditor.SaveRamSize);
                    drawLabelCell("BusConflicts", isChanged(m_romDbEditor.BusConflicts, m_romDbSaved.BusConflicts));
                    drawStringEnumCell("##BusConflicts", m_romDbEditor.BusConflicts, {
                        {"", "Default"},
                        {"Y", "Yes"},
                        {"N", "No"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRamSize", isChanged(m_romDbEditor.ChrRamSize, m_romDbSaved.ChrRamSize));
                    drawInputTextCell("##ChrRamSize", m_romDbEditor.ChrRamSize);
                    drawEmptyPair();

                    ImGui::TableNextRow();
                    drawLabelCell("WorkRamSize", isChanged(m_romDbEditor.WorkRamSize, m_romDbSaved.WorkRamSize));
                    drawInputTextCell("##WorkRamSize", m_romDbEditor.WorkRamSize);
                    drawLabelCell("HasBattery", isChanged(m_romDbEditor.HasBattery, m_romDbSaved.HasBattery));
                    ImGui::TableNextColumn();
                    {
                        bool hasBattery = m_romDbEditor.HasBattery == "1";
                        if(ImGui::Checkbox("##HasBattery", &hasBattery)) {
                            m_romDbEditor.HasBattery = hasBattery ? "1" : "0";
                        }
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
            m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
            m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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

    if(m_snesMouseGrabActive || m_arkanoidGrabActive) {
        m_imGuiWindowFocusBlocksEmulator = false;
    }
}

inline void GeraNESApp::showOverlay()
{
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(mainViewport);
    const ImVec2 overlayOrigin = mainViewport != nullptr ? mainViewport->Pos : ImVec2(0.0f, 0.0f);

    if(AppSettings::instance().data.debug.showFps) {
        const int fontSize = 32;
        ImFont* fpsFont = m_fontFps != nullptr ? m_fontFps : ImGui::GetFont();
        const SDL_Rect clientArea = emulatorClientArea();

        std::string fpsText =
            "EMU " + std::to_string(m_emulatorFps) +
            "  DISP " + std::to_string(m_displayFps);
        ImVec2 fpsTextSize = fpsFont->CalcTextSizeA(fontSize, FLT_MAX, 0, fpsText.c_str());

        const ImVec2 pos = ImVec2(
            overlayOrigin.x + width() - fpsTextSize.x - 32,
            overlayOrigin.y + static_cast<float>(clientArea.y) + 16.0f
        );

        DrawTextOutlined(drawList, fpsFont, fontSize, pos, 0xFFFFFFFF, 0xFF000000, fpsText.c_str());
    }
    m_userToast.draw(drawList, overlayOrigin, static_cast<float>(width()), static_cast<float>(height()), m_fontToast);

    m_touch->draw(drawList, overlayOrigin);
}
