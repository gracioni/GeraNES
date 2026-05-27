#pragma once

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
        m_emu.setPpuEventViewerCaptureEnabled(false);
        ImGui::End();
        return;
    }
    m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if(!m_emu.valid()) {
        m_emu.setPpuEventViewerCaptureEnabled(false);
        m_ppuEventViewerEnabled = false;
        ImGui::TextDisabled("Load a ROM to inspect PPU events.");
        ImGui::End();
        return;
    }

    m_ppuEventViewerEnabled = true;
    m_emu.setPpuEventViewerCaptureEnabled(true);
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

    EmulationHost::PpuEventViewerSnapshot eventSnapshot;
    const bool hasEventSnapshot = m_emu.getPpuEventViewerSnapshot(eventSnapshot);
    const bool refreshEventViewer =
        hasEventSnapshot &&
        (m_eventViewerCachedFrame != eventSnapshot.frameCount ||
         m_eventViewerCachedTraceEnabled != eventSnapshot.traceEnabled);

    if(refreshEventViewer) {
        m_cachedPpuEvents = eventSnapshot.events;
        std::fill(m_ppuEventBuffer.begin(), m_ppuEventBuffer.end(), 0xFF000000u);
        if(!eventSnapshot.framebuffer.empty()) {
            for(int y = 0; y < kVisibleFrameHeight; ++y) {
                const size_t srcOffset = static_cast<size_t>(y * kVisibleFrameWidth);
                const size_t dstOffset = static_cast<size_t>((y * kEventWidth) + kVisibleFrameXOffset);
                std::copy_n(eventSnapshot.framebuffer.data() + srcOffset,
                            kVisibleFrameWidth,
                            m_ppuEventBuffer.begin() + static_cast<std::ptrdiff_t>(dstOffset));
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_ppuEventTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kEventWidth, kEventHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_ppuEventBuffer.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        m_eventViewerCachedFrame = eventSnapshot.frameCount;
        m_eventViewerCachedTraceEnabled = eventSnapshot.traceEnabled;
    }

    const auto& ppuEvents = m_cachedPpuEvents;

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
