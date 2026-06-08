#pragma once

inline void GeraNESApp::drawPaletteWindow()
{
    ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_Appearing);

    if(!ImGui::Begin("Palette", &m_showPaletteWindow)) {
        ImGui::End();
        return;
    }
    m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if(ImGui::BeginChild("PaletteList", ImVec2(180.0f, 0.0f), true)) {
        if(ImGui::Selectable("Default", m_selectedPaletteName == "Default")) {
            applyPalette(m_paletteList.front().colors, "Default");
        }

        for(const PaletteItem& item : m_paletteList) {
            if(item.name == "Default") continue;
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
        SetNextItemWidthScaledClamped(260.0f);
        ImGui::InputText("##PaletteName", &m_paletteNameInput);

        ImGui::SameLine();
        if(ImGui::Button("Save")) {
            saveCurrentPalette();
        }

        ImGui::SameLine();
        const auto selectedPaletteIt = std::find_if(m_paletteList.begin(), m_paletteList.end(), [this](const PaletteItem& item) {
            return item.name == m_selectedPaletteName;
        });
        const bool canDeletePalette =
            selectedPaletteIt != m_paletteList.end() &&
            selectedPaletteIt->name != "Default" &&
            !selectedPaletteIt->builtIn;
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
