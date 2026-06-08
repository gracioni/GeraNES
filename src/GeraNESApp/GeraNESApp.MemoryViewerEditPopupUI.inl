#pragma once

inline void GeraNESApp::drawMemoryViewerEditPopup(uint32_t regionBaseAddress, int regionSource)
{
    if(m_memoryViewerEditOpen && ImGui::BeginPopupModal("Edit Memory Byte", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Address: $%04X", static_cast<unsigned int>(m_memoryViewerEditAddress & 0xFFFFu));
        if(ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        SetNextItemWidthScaledClamped(70.0f);
        const bool editSubmitted = ImGui::InputText(
            "Value",
            m_memoryViewerEditText,
            sizeof(m_memoryViewerEditText),
            ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll
        );
        const bool writeClicked = ImGui::Button("Write");
        if(editSubmitted || writeClicked) {
            uint8_t value = 0;
            for(char c : m_memoryViewerEditText) {
                if(c == '\0') {
                    break;
                }

                value <<= 4;
                if(c >= '0' && c <= '9') {
                    value |= static_cast<uint8_t>(c - '0');
                } else if(c >= 'a' && c <= 'f') {
                    value |= static_cast<uint8_t>(c - 'a' + 10);
                } else if(c >= 'A' && c <= 'F') {
                    value |= static_cast<uint8_t>(c - 'A' + 10);
                }
            }
            m_memoryViewerEditValue = value;
            const uint32_t address = m_memoryViewerEditAddress;
            m_emu.withExclusiveAccess([&](auto& emu) {
                switch(regionSource) {
                    case 0:
                        emu.debugWriteCpuMemory(static_cast<uint16_t>(address), value);
                        break;
                    case 1:
                        emu.getConsole().ppu().debugWritePpuMemory(static_cast<uint16_t>(address), value);
                        break;
                    case 2:
                        emu.getConsole().ppu().debugWritePrimaryOam(static_cast<uint8_t>(address - regionBaseAddress), value);
                        break;
                    case 3:
                        emu.getConsole().ppu().debugWriteSecondaryOam(static_cast<uint8_t>(address - regionBaseAddress), value);
                        break;
                }
            });
            m_memoryViewerEditOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")) {
            m_memoryViewerEditOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
