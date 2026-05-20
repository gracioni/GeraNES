#pragma once

inline void GeraNESApp::drawMemoryViewerWindow()
{
    struct MemoryViewerRegion {
        const char* name;
        uint32_t baseAddress;
        uint32_t size;
        enum class Source {
            Cpu,
            Ppu,
            PrimaryOam,
            SecondaryOam
        } source;
    };

    static constexpr MemoryViewerRegion kRegions[] = {
        {"CPU Memory", 0x0000, 0x10000, MemoryViewerRegion::Source::Cpu},
        {"PPU Memory", 0x0000, 0x4000, MemoryViewerRegion::Source::Ppu},
        {"PRG ROM", 0x8000, 0x8000, MemoryViewerRegion::Source::Cpu},
        {"System RAM", 0x0000, 0x0800, MemoryViewerRegion::Source::Cpu},
        {"Work RAM", 0x6000, 0x2000, MemoryViewerRegion::Source::Cpu},
        {"Nametable RAM (CIRAM)", 0x2000, 0x1000, MemoryViewerRegion::Source::Ppu},
        {"Sprite RAM (OAM)", 0x0000, 0x0100, MemoryViewerRegion::Source::PrimaryOam},
        {"Secondary OAM RAM", 0x0000, 0x0020, MemoryViewerRegion::Source::SecondaryOam},
        {"Palette RAM", 0x3F00, 0x0020, MemoryViewerRegion::Source::Ppu},
        {"CHR ROM", 0x0000, 0x2000, MemoryViewerRegion::Source::Ppu},
    };

    SetNextWindowSizeClamped(ImVec2(760.0f, 620.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(!ImGui::Begin("Memory Viewer", &m_showMemoryViewerWindow)) {
        ImGui::End();
        return;
    }
    m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if(!m_emu.valid()) {
        ImGui::TextDisabled("Load a ROM to inspect memory.");
        ImGui::End();
        return;
    }

    if(m_memoryViewerType < 0 || m_memoryViewerType >= static_cast<int>(std::size(kRegions))) {
        m_memoryViewerType = 0;
    }
    auto setMemoryViewerGotoText = [this](uint32_t address) {
        std::ostringstream text;
        text << std::uppercase << std::hex;
        text.width(4);
        text.fill('0');
        text << static_cast<unsigned int>(address & 0xFFFFu);
        const std::string value = text.str();
        m_memoryViewerGotoText[0] = value[0];
        m_memoryViewerGotoText[1] = value[1];
        m_memoryViewerGotoText[2] = value[2];
        m_memoryViewerGotoText[3] = value[3];
        m_memoryViewerGotoText[4] = '\0';
    };
    auto parseMemoryViewerHexText = [](const char* text) {
        uint32_t value = 0;
        for(const char* c = text; *c != '\0'; ++c) {
            value <<= 4;
            if(*c >= '0' && *c <= '9') {
                value |= static_cast<uint32_t>(*c - '0');
            } else if(*c >= 'a' && *c <= 'f') {
                value |= static_cast<uint32_t>(*c - 'a' + 10);
            } else if(*c >= 'A' && *c <= 'F') {
                value |= static_cast<uint32_t>(*c - 'A' + 10);
            }
        }
        return value;
    };
    const MemoryViewerRegion& comboRegion = kRegions[m_memoryViewerType];

    ImGui::TextUnformatted("Memory Type");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if(ImGui::BeginCombo("##MemoryViewerType", comboRegion.name)) {
        for(int i = 0; i < static_cast<int>(std::size(kRegions)); ++i) {
            const bool selected = i == m_memoryViewerType;
            if(ImGui::Selectable(kRegions[i].name, selected)) {
                m_memoryViewerType = i;
                m_memoryViewerAddress = static_cast<uint16_t>(kRegions[i].baseAddress);
                m_memoryViewerGotoAddress = m_memoryViewerAddress;
                setMemoryViewerGotoText(m_memoryViewerGotoAddress);
                m_memoryViewerScrollToAddress = kRegions[i].baseAddress;
                m_memoryViewerScrollPendingFrames = 3;
            }
            if(selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    const MemoryViewerRegion& region = kRegions[m_memoryViewerType];

    ImGui::SameLine();
    ImGui::TextUnformatted("Address");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(86.0f);
    const bool gotoSubmitted = ImGui::InputText(
        "##MemoryViewerAddress",
        m_memoryViewerGotoText,
        sizeof(m_memoryViewerGotoText),
        ImGuiInputTextFlags_CharsHexadecimal |
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_AutoSelectAll
    );
    ImGui::SameLine();
    const bool gotoClicked = ImGui::Button("Go");
    if(gotoSubmitted || gotoClicked) {
        const uint32_t minAddress = region.baseAddress;
        const uint32_t maxAddress = region.baseAddress + region.size - 1;
        const uint32_t requestedAddress = parseMemoryViewerHexText(m_memoryViewerGotoText);
        const uint32_t clamped = std::clamp<uint32_t>(requestedAddress, minAddress, maxAddress);
        m_memoryViewerAddress = static_cast<uint16_t>(clamped);
        m_memoryViewerGotoAddress = m_memoryViewerAddress;
        setMemoryViewerGotoText(m_memoryViewerGotoAddress);
        m_memoryViewerScrollToAddress = clamped;
        m_memoryViewerScrollPendingFrames = 3;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(64.0f);
    ImGui::InputInt("Columns", &m_memoryViewerColumns, 0, 0);
    m_memoryViewerColumns = std::clamp(m_memoryViewerColumns, 4, 32);
    ImGui::SameLine();
    ImGui::Checkbox("ASCII", &m_memoryViewerShowAscii);

    ImGui::Separator();

    std::vector<uint8_t> memory(region.size);
    m_emu.withExclusiveAccess([&](auto& emu) {
        for(uint32_t i = 0; i < region.size; ++i) {
            const uint16_t address = static_cast<uint16_t>(region.baseAddress + i);
            switch(region.source) {
                case MemoryViewerRegion::Source::Cpu:
                    memory[i] = emu.debugPeekCpuMemory(address);
                    break;
                case MemoryViewerRegion::Source::Ppu:
                    memory[i] = emu.getConsole().ppu().debugPeekPpuMemory(address);
                    break;
                case MemoryViewerRegion::Source::PrimaryOam:
                    memory[i] = emu.getConsole().ppu().debugPeekPrimaryOam(static_cast<uint8_t>(i));
                    break;
                case MemoryViewerRegion::Source::SecondaryOam:
                    memory[i] = emu.getConsole().ppu().debugPeekSecondaryOam(static_cast<uint8_t>(i));
                    break;
            }
        }
    });

    const int columns = m_memoryViewerColumns;
    const int rowCount = static_cast<int>((region.size + static_cast<uint32_t>(columns) - 1) / static_cast<uint32_t>(columns));
    const uint32_t scrollAddress = m_memoryViewerScrollToAddress != UINT32_MAX
        ? m_memoryViewerScrollToAddress
        : m_memoryViewerAddress;
    const uint32_t scrollOffset = scrollAddress >= region.baseAddress
        ? std::min<uint32_t>(scrollAddress - region.baseAddress, region.size - 1)
        : 0;
    const int scrollRow = static_cast<int>(scrollOffset / static_cast<uint32_t>(columns));
    bool openEditPopup = false;

    if(ImGui::BeginChild("MemoryViewerData", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        const float lineHeight = ImGui::GetTextLineHeight();
        if(ImGui::IsWindowAppearing() || (m_memoryViewerScrollToAddress != UINT32_MAX && m_memoryViewerScrollPendingFrames > 0)) {
            ImGui::SetScrollY(std::max(0.0f, static_cast<float>(scrollRow) * lineHeight - lineHeight * 3.0f));
            --m_memoryViewerScrollPendingFrames;
            if(m_memoryViewerScrollPendingFrames <= 0) {
                m_memoryViewerScrollToAddress = UINT32_MAX;
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGuiListClipper clipper;
        clipper.Begin(rowCount, lineHeight);
        while(clipper.Step()) {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const uint32_t rowOffset = static_cast<uint32_t>(row) * static_cast<uint32_t>(columns);
                const uint32_t rowAddress = region.baseAddress + rowOffset;

                std::ostringstream addressText;
                addressText << std::uppercase << std::hex;
                addressText.width(4);
                addressText.fill('0');
                addressText << static_cast<unsigned int>(rowAddress & 0xFFFFu);

                ImGui::TextUnformatted(addressText.str().c_str());
                if(ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Address: $%04X", static_cast<unsigned int>(rowAddress & 0xFFFFu));
                    ImGui::EndTooltip();
                }

                ImGui::SameLine(0.0f, ImGui::CalcTextSize("  ").x);
                const float byteCellWidth = ImGui::CalcTextSize("00 ").x;
                for(int col = 0; col < columns; ++col) {
                    const uint32_t offset = rowOffset + static_cast<uint32_t>(col);
                    if(offset >= region.size) {
                        break;
                    }

                    std::ostringstream byteText;
                    byteText << std::uppercase << std::hex;
                    byteText.width(2);
                    byteText.fill('0');
                    byteText << static_cast<unsigned int>(memory[offset]);

                    ImGui::PushID(static_cast<int>(offset));
                    if(ImGui::Selectable(byteText.str().c_str(), false, 0, ImVec2(byteCellWidth, lineHeight))) {
                        m_memoryViewerEditOpen = true;
                        m_memoryViewerEditAddress = region.baseAddress + offset;
                        m_memoryViewerEditValue = memory[offset];
                        m_memoryViewerEditText[0] = byteText.str()[0];
                        m_memoryViewerEditText[1] = byteText.str()[1];
                        m_memoryViewerEditText[2] = '\0';
                        openEditPopup = true;
                    }
                    if(ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Address: $%04X", static_cast<unsigned int>((region.baseAddress + offset) & 0xFFFFu));
                        ImGui::Text("Value: $%02X", static_cast<unsigned int>(memory[offset]));
                        ImGui::EndTooltip();
                    }
                    ImGui::PopID();

                    if(col + 1 < columns && offset + 1 < region.size) {
                        ImGui::SameLine(0.0f, 0.0f);
                    }
                }

                if(m_memoryViewerShowAscii) {
                    std::ostringstream ascii;
                    for(int col = 0; col < columns; ++col) {
                        const uint32_t offset = rowOffset + static_cast<uint32_t>(col);
                        if(offset < region.size) {
                            const uint8_t value = memory[offset];
                            ascii << ((value >= 0x20 && value <= 0x7E) ? static_cast<char>(value) : '.');
                        } else {
                            ascii << ' ';
                        }
                    }
                    ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
                    ImGui::TextUnformatted(ascii.str().c_str());
                }
            }
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    if(openEditPopup) {
        m_memoryViewerEditOpen = true;
        ImGui::OpenPopup("Edit Memory Byte");
    }

    if(m_memoryViewerEditOpen && ImGui::BeginPopupModal("Edit Memory Byte", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Address: $%04X", static_cast<unsigned int>(m_memoryViewerEditAddress & 0xFFFFu));
        if(ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(70.0f);
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
            const MemoryViewerRegion writeRegion = region;
            m_emu.withExclusiveAccess([&](auto& emu) {
                switch(writeRegion.source) {
                    case MemoryViewerRegion::Source::Cpu:
                        emu.debugWriteCpuMemory(static_cast<uint16_t>(address), value);
                        break;
                    case MemoryViewerRegion::Source::Ppu:
                        emu.getConsole().ppu().debugWritePpuMemory(static_cast<uint16_t>(address), value);
                        break;
                    case MemoryViewerRegion::Source::PrimaryOam:
                        emu.getConsole().ppu().debugWritePrimaryOam(static_cast<uint8_t>(address - writeRegion.baseAddress), value);
                        break;
                    case MemoryViewerRegion::Source::SecondaryOam:
                        emu.getConsole().ppu().debugWriteSecondaryOam(static_cast<uint8_t>(address - writeRegion.baseAddress), value);
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

    ImGui::End();
}
