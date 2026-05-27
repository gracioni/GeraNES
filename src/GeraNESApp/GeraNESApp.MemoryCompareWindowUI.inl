#pragma once

inline void GeraNESApp::drawMemoryCompareWindow()
{
    struct MemoryCompareRegion {
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

    static constexpr MemoryCompareRegion kRegions[] = {
        {"CPU Memory", 0x0000, 0x10000, MemoryCompareRegion::Source::Cpu},
        {"PPU Memory", 0x0000, 0x4000, MemoryCompareRegion::Source::Ppu},
        {"PRG ROM", 0x8000, 0x8000, MemoryCompareRegion::Source::Cpu},
        {"System RAM", 0x0000, 0x0800, MemoryCompareRegion::Source::Cpu},
        {"Work RAM", 0x6000, 0x2000, MemoryCompareRegion::Source::Cpu},
        {"Nametable RAM (CIRAM)", 0x2000, 0x1000, MemoryCompareRegion::Source::Ppu},
        {"Sprite RAM (OAM)", 0x0000, 0x0100, MemoryCompareRegion::Source::PrimaryOam},
        {"Secondary OAM RAM", 0x0000, 0x0020, MemoryCompareRegion::Source::SecondaryOam},
        {"Palette RAM", 0x3F00, 0x0020, MemoryCompareRegion::Source::Ppu},
        {"CHR ROM", 0x0000, 0x2000, MemoryCompareRegion::Source::Ppu},
    };

    auto readRegion = [&](const MemoryCompareRegion& region) {
        std::vector<uint8_t> memory(region.size);
        m_emu.withExclusiveAccess([&](auto& emu) {
            for(uint32_t i = 0; i < region.size; ++i) {
                const uint16_t address = static_cast<uint16_t>(region.baseAddress + i);
                switch(region.source) {
                    case MemoryCompareRegion::Source::Cpu:
                        memory[i] = emu.debugPeekCpuMemory(address);
                        break;
                    case MemoryCompareRegion::Source::Ppu:
                        memory[i] = emu.getConsole().ppu().debugPeekPpuMemory(address);
                        break;
                    case MemoryCompareRegion::Source::PrimaryOam:
                        memory[i] = emu.getConsole().ppu().debugPeekPrimaryOam(static_cast<uint8_t>(i));
                        break;
                    case MemoryCompareRegion::Source::SecondaryOam:
                        memory[i] = emu.getConsole().ppu().debugPeekSecondaryOam(static_cast<uint8_t>(i));
                        break;
                }
            }
        });
        return memory;
    };
    auto parseByteText = [](const char* text) {
        uint8_t value = 0;
        for(const char* c = text; *c != '\0'; ++c) {
            value <<= 4;
            if(*c >= '0' && *c <= '9') {
                value |= static_cast<uint8_t>(*c - '0');
            } else if(*c >= 'a' && *c <= 'f') {
                value |= static_cast<uint8_t>(*c - 'a' + 10);
            } else if(*c >= 'A' && *c <= 'F') {
                value |= static_cast<uint8_t>(*c - 'A' + 10);
            }
        }
        return value;
    };

    SetNextWindowCenteredOnMainViewport(ImVec2(620.0f, 620.0f));

    if(!ImGui::Begin("Memory Compare", &m_showMemoryCompareWindow)) {
        ImGui::End();
        return;
    }
    m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if(!m_emu.valid()) {
        ImGui::TextDisabled("Load a ROM to compare memory.");
        ImGui::End();
        return;
    }

    if(m_memoryCompareType < 0 || m_memoryCompareType >= static_cast<int>(std::size(kRegions))) {
        m_memoryCompareType = 0;
    }

    const MemoryCompareRegion& comboRegion = kRegions[m_memoryCompareType];
    ImGui::TextUnformatted("Memory Type");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if(ImGui::BeginCombo("##MemoryCompareType", comboRegion.name)) {
        for(int i = 0; i < static_cast<int>(std::size(kRegions)); ++i) {
            const bool selected = i == m_memoryCompareType;
            if(ImGui::Selectable(kRegions[i].name, selected)) {
                m_memoryCompareType = i;
                m_memoryCompareBaseline.clear();
                m_memoryCompareCurrent.clear();
                m_memoryCompareMask.clear();
                m_memoryCompareMaskStack.clear();
                m_memoryCompareMaskActive = false;
                m_memoryCompareStatus.clear();
            }
            if(selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    const MemoryCompareRegion& region = kRegions[m_memoryCompareType];
    const bool hasBaseline = m_memoryCompareBaseline.size() == region.size;

    ImGui::SameLine();
    if(ImGui::Button("Capture Baseline")) {
        m_memoryCompareBaseline = readRegion(region);
        m_memoryCompareCurrent = m_memoryCompareBaseline;
        if(m_memoryCompareMaskActive) {
            const auto invalidMask = std::find_if(
                m_memoryCompareMask.begin(),
                m_memoryCompareMask.end(),
                [&](size_t index) { return index >= region.size; }
            );
            if(invalidMask != m_memoryCompareMask.end()) {
                m_memoryCompareMask.clear();
                m_memoryCompareMaskStack.clear();
                m_memoryCompareMaskActive = false;
            }
        }
        m_memoryCompareStatus = "Baseline captured.";
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasBaseline);
    if(ImGui::Button("Refresh")) {
        m_memoryCompareCurrent = readRegion(region);
        m_memoryCompareStatus = "Current frame refreshed.";
    }
    ImGui::EndDisabled();

    ImGui::Checkbox("Auto refresh", &m_memoryCompareAutoRefresh);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170.0f);
    const char* filterItems[] = {"All", "Changed", "Unchanged", "Changed from -> to"};
    ImGui::Combo("Filter", &m_memoryCompareFilter, filterItems, static_cast<int>(std::size(filterItems)));
    if(m_memoryCompareFilter == 3) {
        ImGui::SameLine();
        ImGui::TextUnformatted("From");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(42.0f);
        ImGui::InputText(
            "##MemoryCompareFrom",
            m_memoryCompareFromText,
            sizeof(m_memoryCompareFromText),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll
        );
        ImGui::SameLine();
        ImGui::TextUnformatted("To");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(42.0f);
        ImGui::InputText(
            "##MemoryCompareTo",
            m_memoryCompareToText,
            sizeof(m_memoryCompareToText),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll
        );
    }

    if(hasBaseline && (m_memoryCompareAutoRefresh || m_memoryCompareCurrent.size() != region.size)) {
        m_memoryCompareCurrent = readRegion(region);
    }

    if(!m_memoryCompareStatus.empty()) {
        ImGui::TextDisabled("%s", m_memoryCompareStatus.c_str());
    }

    if(!hasBaseline) {
        ImGui::Separator();
        ImGui::TextWrapped("Capture a baseline before changing the game state, then change level or trigger the event and watch which bytes changed.");
        ImGui::End();
        return;
    }

    const uint8_t compareFrom = parseByteText(m_memoryCompareFromText);
    const uint8_t compareTo = parseByteText(m_memoryCompareToText);
    size_t changedCount = 0;
    size_t candidateCount = 0;
    std::vector<size_t> visibleIndices;
    visibleIndices.reserve(m_memoryCompareMaskActive ? m_memoryCompareMask.size() : m_memoryCompareBaseline.size());
    const size_t memorySize = std::min(m_memoryCompareBaseline.size(), m_memoryCompareCurrent.size());
    auto testIndex = [&](size_t i) {
        if(i >= memorySize) {
            return;
        }
        ++candidateCount;
        const uint8_t baseline = m_memoryCompareBaseline[i];
        const uint8_t current = m_memoryCompareCurrent[i];
        const bool changed = baseline != current;
        if(changed) {
            ++changedCount;
        }
        if(
            m_memoryCompareFilter == 0 ||
            (m_memoryCompareFilter == 1 && changed) ||
            (m_memoryCompareFilter == 2 && !changed) ||
            (m_memoryCompareFilter == 3 && baseline == compareFrom && current == compareTo)
        ) {
            visibleIndices.push_back(i);
        }
    };
    if(m_memoryCompareMaskActive) {
        for(size_t index : m_memoryCompareMask) {
            testIndex(index);
        }
    } else {
        for(size_t i = 0; i < memorySize; ++i) {
            testIndex(i);
        }
    }

    ImGui::Separator();
    if(m_memoryCompareFilter == 3) {
        ImGui::Text(
            "Changed: %zu / %zu   Matches $%02X -> $%02X: %zu",
            changedCount,
            candidateCount,
            static_cast<unsigned int>(compareFrom),
            static_cast<unsigned int>(compareTo),
            visibleIndices.size()
        );
    } else {
        ImGui::Text("Changed: %zu / %zu   Visible: %zu", changedCount, candidateCount, visibleIndices.size());
    }
    if(m_memoryCompareMaskActive) {
        ImGui::SameLine();
        ImGui::TextDisabled("Mask %zu: %zu", m_memoryCompareMaskStack.size(), m_memoryCompareMask.size());
    }
    ImGui::BeginDisabled(visibleIndices.empty());
    if(ImGui::Button("Push Results As Mask")) {
        m_memoryCompareMask = visibleIndices;
        m_memoryCompareMaskStack.push_back(m_memoryCompareMask);
        m_memoryCompareMaskActive = true;
        m_memoryCompareStatus = "Mask layer pushed from current results.";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_memoryCompareMaskActive);
    if(ImGui::Button("Pop Mask")) {
        if(!m_memoryCompareMaskStack.empty()) {
            m_memoryCompareMaskStack.pop_back();
        }
        if(m_memoryCompareMaskStack.empty()) {
            m_memoryCompareMask.clear();
            m_memoryCompareMaskActive = false;
            m_memoryCompareStatus = "Mask stack cleared.";
        } else {
            m_memoryCompareMask = m_memoryCompareMaskStack.back();
            m_memoryCompareMaskActive = true;
            m_memoryCompareStatus = "Returned to previous mask layer.";
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_memoryCompareMaskActive);
    if(ImGui::Button("Clear Masks")) {
        m_memoryCompareMask.clear();
        m_memoryCompareMaskStack.clear();
        m_memoryCompareMaskActive = false;
        m_memoryCompareStatus = "Mask stack cleared.";
    }
    ImGui::EndDisabled();

    const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
    if(ImGui::BeginChild("MemoryCompareData", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Columns(4, "MemoryCompareColumns");
        ImGui::SetColumnWidth(0, 90.0f);
        ImGui::SetColumnWidth(1, 80.0f);
        ImGui::SetColumnWidth(2, 80.0f);
        ImGui::SetColumnWidth(3, 80.0f);
        ImGui::TextUnformatted("Address");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Baseline");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Current");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Delta");
        ImGui::NextColumn();
        ImGui::Separator();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visibleIndices.size()), rowHeight);
        while(clipper.Step()) {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const size_t index = visibleIndices[static_cast<size_t>(row)];
                if(index >= m_memoryCompareBaseline.size() || index >= m_memoryCompareCurrent.size()) {
                    continue;
                }

                const uint8_t baseline = m_memoryCompareBaseline[index];
                const uint8_t current = m_memoryCompareCurrent[index];
                const bool changed = baseline != current;

                const ImVec4 changedColor = ImGuiTheme::accentActive();
                if(changed) {
                    ImGui::PushStyleColor(ImGuiCol_Text, changedColor);
                }
                ImGui::Text("$%04X", static_cast<unsigned int>((region.baseAddress + index) & 0xFFFFu));
                ImGui::NextColumn();
                ImGui::Text("$%02X", static_cast<unsigned int>(baseline));
                ImGui::NextColumn();
                ImGui::Text("$%02X", static_cast<unsigned int>(current));
                ImGui::NextColumn();
                ImGui::Text("%+d", static_cast<int>(current) - static_cast<int>(baseline));
                ImGui::NextColumn();
                if(changed) {
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::Columns(1);
    }
    ImGui::EndChild();

    ImGui::End();
}
