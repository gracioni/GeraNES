#pragma once

inline void GeraNESApp::drawRomDatabaseWindow()
{
    if(m_showRomDatabaseWindow) {
        SetNextWindowCenteredOnMainViewport(ImVec2(720.0f, 0.0f));
        ImGui::OpenPopup("Rom Database");
    }

    if(ImGui::BeginPopupModal("Rom Database", &m_showRomDatabaseWindow)) {
        if(!m_romDbEditor.loaded) {
            ImGui::TextWrapped("%s", m_romDbEditor.statusMessage.c_str());
        } else {
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

            drawConfirmSaveRomDatabaseEntryPopup();
            drawConfirmRemoveRomDatabaseEntryPopup();
        }
        ImGui::EndPopup();
    }
    if(!ImGui::IsPopupOpen("Rom Database")) {
        m_showRomDatabaseWindow = false;
    }
}
