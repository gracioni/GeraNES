#pragma once

inline void GeraNESApp::drawPpuViewerWindow()
{
    constexpr int kNametableWidth = 512;
    constexpr int kNametableHeight = 480;
    constexpr int kChrWidth = 256;
    constexpr int kChrHeight = 128;
    constexpr float kPaletteSwatchSize = 18.0f;
    constexpr float kPaletteSwatchSpacing = 4.0f;

    ImGui::SetNextWindowSize(ImVec2(1080.0f, 830.0f), ImGuiCond_Appearing);

    if(!ImGui::Begin("PPU Viewer", &m_showPpuViewerWindow, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("Export")) {
            if(ImGui::MenuItem("CHR")) {
                exportCurrentPpuChrPng();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    auto syncPpuViewerScanlineTrace = [&](bool enabled) {
        if(m_ppuViewerScanlineTraceActive == enabled) {
            return;
        }

        m_emu.withExclusiveAccess([enabled](auto& emu) {
            emu.enablePpuViewerScanlineTrace(enabled);
        });
        m_ppuViewerScanlineTraceActive = enabled;

        if(!enabled) {
            m_ppuViewerScanlineStates.clear();
            m_ppuViewerScanlineSnapshots.clear();
        }
    };

    if(!m_emu.valid()) {
        syncPpuViewerScanlineTrace(false);
        m_emu.setPpuViewerCaptureEnabled(false, false);
        ImGui::TextDisabled("Load a ROM to inspect PPU data.");
        ImGui::End();
        return;
    }

    syncPpuViewerScanlineTrace(true);
    m_emu.setPpuViewerCaptureEnabled(true, true);

    if(m_ppuNametableTexture == 0) {
        glGenTextures(1, &m_ppuNametableTexture);
        glBindTexture(GL_TEXTURE_2D, m_ppuNametableTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kNametableWidth, kNametableHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    if(m_ppuChrTexture == 0) {
        glGenTextures(1, &m_ppuChrTexture);
        glBindTexture(GL_TEXTURE_2D, m_ppuChrTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kChrWidth, kChrHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    if(m_ppuNametableBuffer.size() != static_cast<size_t>(kNametableWidth * kNametableHeight)) {
        m_ppuNametableBuffer.resize(static_cast<size_t>(kNametableWidth * kNametableHeight));
    }

    if(m_ppuChrBuffer.size() != static_cast<size_t>(kChrWidth * kChrHeight)) {
        m_ppuChrBuffer.resize(static_cast<size_t>(kChrWidth * kChrHeight));
    }
    if(m_ppuChrExportBuffer.size() != static_cast<size_t>(kChrWidth * kChrHeight)) {
        m_ppuChrExportBuffer.resize(static_cast<size_t>(kChrWidth * kChrHeight));
    }

    EmulationHost::PpuViewerSnapshot viewerSnapshot;
    if(!m_emu.getPpuViewerSnapshot(viewerSnapshot) || !viewerSnapshot.valid) {
        ImGui::TextDisabled("Waiting for PPU snapshot...");
        ImGui::End();
        return;
    }
    const auto& chrData = viewerSnapshot.chrData;
    const auto& nametableData = viewerSnapshot.nametableData;
    const auto& paletteData = viewerSnapshot.paletteData;
    const auto& rgbPalette = viewerSnapshot.rgbPalette;
    const int backgroundPatternTableAddress = viewerSnapshot.backgroundPatternTableAddress;
    static constexpr std::array<const char*, 9> kChrPaletteModeLabels = {
        "None",
        "BG 0",
        "BG 1",
        "BG 2",
        "BG 3",
        "SPR 0",
        "SPR 1",
        "SPR 2",
        "SPR 3"
    };
    static constexpr std::array<uint32_t, 4> kChrGrayscaleColors = {
        0xFF000000u,
        0xFF555555u,
        0xFFAAAAAAu,
        0xFFFFFFFFu
    };
    const bool refreshViewer =
        m_ppuViewerCachedFrame != viewerSnapshot.frameCount ||
        m_ppuViewerCachedScanline != viewerSnapshot.ppuScanline ||
        m_ppuViewerCachedCycle != viewerSnapshot.ppuCycle ||
        m_ppuViewerCachedChrPaletteMode != m_ppuViewerChrPaletteMode;
    m_ppuViewerCachedFrame = viewerSnapshot.frameCount;
    m_ppuViewerCachedScanline = viewerSnapshot.ppuScanline;
    m_ppuViewerCachedCycle = viewerSnapshot.ppuCycle;
    m_ppuViewerCachedChrPaletteMode = m_ppuViewerChrPaletteMode;

    if(refreshViewer) {
        const auto traceData = m_emu.withExclusiveAccess([](const auto& emu) {
            return std::make_pair(emu.ppuViewerScanlineStates(), emu.ppuViewerScanlineSnapshots());
        });
        m_ppuViewerScanlineStates = std::move(traceData.first);
        m_ppuViewerScanlineSnapshots = std::move(traceData.second);
    }

    const auto colorForPaletteEntry = [&](uint8_t paletteEntry) -> uint32_t {
        return rgbPalette[paletteEntry & 0x3F];
    };
    const auto chrPixelColorForMode = [&](uint8_t colorIndex, const std::array<uint8_t, 32>& activePaletteData) -> uint32_t {
        if(m_ppuViewerChrPaletteMode == 0) {
            return kChrGrayscaleColors[static_cast<size_t>(colorIndex & 0x03)];
        }

        int paletteBase = 0;
        if(m_ppuViewerChrPaletteMode >= 1 && m_ppuViewerChrPaletteMode <= 4) {
            paletteBase = (m_ppuViewerChrPaletteMode - 1) * 4;
        } else {
            paletteBase = 0x10 + (m_ppuViewerChrPaletteMode - 5) * 4;
        }

        const uint8_t paletteEntry =
            static_cast<uint8_t>(activePaletteData[static_cast<size_t>(paletteBase + colorIndex)] & 0x3F);
        return colorForPaletteEntry(paletteEntry);
    };
    const auto wrapCoord = [](int value, int range) {
        int wrapped = value % range;
        if(wrapped < 0) {
            wrapped += range;
        }
        return wrapped;
    };

    const auto findScanlineTraceState = [&](int visibleScanline) -> const GeraNESEmu::PpuViewerScanlineState* {
        if(visibleScanline < 0 ||
           visibleScanline >= static_cast<int>(m_ppuViewerScanlineStates.size())) {
            return nullptr;
        }

        const auto& lineState = m_ppuViewerScanlineStates[static_cast<size_t>(visibleScanline)];
        return lineState.valid ? &lineState : nullptr;
    };
    const auto findScanlineTraceSnapshot =
        [&](const GeraNESEmu::PpuViewerScanlineState* lineState)
            -> const GeraNESEmu::PpuViewerScanlineSnapshot* {
        if(lineState == nullptr ||
           lineState->snapshotIndex == 0xFFFF ||
           lineState->snapshotIndex >= m_ppuViewerScanlineSnapshots.size()) {
            return nullptr;
        }

        return &m_ppuViewerScanlineSnapshots[lineState->snapshotIndex];
    };

    struct ScrollSpan
    {
        int startScanline = 0;
        int endScanline = 0;
        int rawScrollX = 0;
        int rawScrollY = 0;
        int virtualScrollX = 0;
        int virtualScrollY = 0;
    };

    std::vector<ScrollSpan> stableScrollSpans;
    int displayBackgroundPatternTableAddress = backgroundPatternTableAddress;
    if(!m_ppuViewerScanlineStates.empty()) {
        std::vector<ScrollSpan> rawSpans;
        rawSpans.reserve(m_ppuViewerScanlineStates.size());
        const auto sameRegionKey = [](const ScrollSpan& a, const ScrollSpan& b) {
            return a.virtualScrollX == b.virtualScrollX &&
                   a.virtualScrollY == b.virtualScrollY;
        };

        for(size_t scanline = 0; scanline < m_ppuViewerScanlineStates.size(); ++scanline) {
            const auto* lineState = findScanlineTraceState(static_cast<int>(scanline));
            if(lineState == nullptr) {
                continue;
            }

            const int lineRawScrollX = static_cast<int>(lineState->rawScrollX);
            int lineRawScrollY = static_cast<int>(lineState->rawScrollY) - static_cast<int>(scanline);
            lineRawScrollY %= 240;
            if(lineRawScrollY < 0) {
                lineRawScrollY += 240;
            }
            const int lineVirtualScrollX = static_cast<int>(lineState->virtualScrollX);
            int lineVirtualScrollY = static_cast<int>(lineState->virtualScrollY) - static_cast<int>(scanline);
            lineVirtualScrollY %= 480;
            if(lineVirtualScrollY < 0) {
                lineVirtualScrollY += 480;
            }

            if(rawSpans.empty() ||
               rawSpans.back().virtualScrollX != lineVirtualScrollX ||
               rawSpans.back().virtualScrollY != lineVirtualScrollY) {
                rawSpans.push_back(ScrollSpan{
                    static_cast<int>(scanline),
                    static_cast<int>(scanline),
                    lineRawScrollX,
                    lineRawScrollY,
                    lineVirtualScrollX,
                    lineVirtualScrollY
                });
            } else {
                rawSpans.back().endScanline = static_cast<int>(scanline);
            }
        }

        constexpr int kTransientSpanLines = 8;
        if(!rawSpans.empty()) {
            std::vector<ScrollSpan> mergedSpans = rawSpans;
            bool changed = true;

            while(changed) {
                changed = false;
                for(size_t i = 0; i < mergedSpans.size(); ++i) {
                    const int spanLength = mergedSpans[i].endScanline - mergedSpans[i].startScanline + 1;
                    if(spanLength > kTransientSpanLines) {
                        continue;
                    }

                    if(i == 0 && mergedSpans.size() > 1) {
                        mergedSpans[1].startScanline = mergedSpans[0].startScanline;
                        mergedSpans.erase(mergedSpans.begin());
                        changed = true;
                        break;
                    }

                    if(i + 1 == mergedSpans.size() && mergedSpans.size() > 1) {
                        mergedSpans[i - 1].endScanline = mergedSpans[i].endScanline;
                        mergedSpans.erase(mergedSpans.begin() + static_cast<std::ptrdiff_t>(i));
                        changed = true;
                        break;
                    }

                    if(i > 0 && i + 1 < mergedSpans.size()) {
                        if(sameRegionKey(mergedSpans[i - 1], mergedSpans[i + 1])) {
                            mergedSpans[i - 1].endScanline = mergedSpans[i + 1].endScanline;
                            mergedSpans.erase(
                                mergedSpans.begin() + static_cast<std::ptrdiff_t>(i),
                                mergedSpans.begin() + static_cast<std::ptrdiff_t>(i + 2)
                            );
                            changed = true;
                            break;
                        }

                        const int prevLength = mergedSpans[i - 1].endScanline - mergedSpans[i - 1].startScanline + 1;
                        const int nextLength = mergedSpans[i + 1].endScanline - mergedSpans[i + 1].startScanline + 1;
                        if(prevLength >= nextLength) {
                            mergedSpans[i - 1].endScanline = mergedSpans[i].endScanline;
                            mergedSpans.erase(mergedSpans.begin() + static_cast<std::ptrdiff_t>(i));
                        } else {
                            mergedSpans[i + 1].startScanline = mergedSpans[i].startScanline;
                            mergedSpans.erase(mergedSpans.begin() + static_cast<std::ptrdiff_t>(i));
                        }
                        changed = true;
                        break;
                    }
                }
            }

            stableScrollSpans = std::move(mergedSpans);
        }

        const auto representativeWrappedValue = [](const std::vector<int>& values, int range) {
            if(values.empty()) {
                return 0;
            }

            const int anchor = values.front();
            std::vector<int> unwrapped;
            unwrapped.reserve(values.size());

            for(int value : values) {
                int delta = value - anchor;
                if(delta > range / 2) {
                    delta -= range;
                } else if(delta < -(range / 2)) {
                    delta += range;
                }
                unwrapped.push_back(anchor + delta);
            }

            std::sort(unwrapped.begin(), unwrapped.end());
            int representative = unwrapped[unwrapped.size() / 2] % range;
            if(representative < 0) {
                representative += range;
            }
            return representative;
        };

        for(auto& span : stableScrollSpans) {
            std::vector<int> rawXs;
            std::vector<int> rawYs;
            std::vector<int> virtualXs;
            std::vector<int> virtualYs;

            for(int scanline = span.startScanline; scanline <= span.endScanline; ++scanline) {
                const auto* lineState = findScanlineTraceState(scanline);
                if(lineState == nullptr) {
                    continue;
                }

                rawXs.push_back(static_cast<int>(lineState->rawScrollX));

                int rawY = static_cast<int>(lineState->rawScrollY) - scanline;
                rawY %= 240;
                if(rawY < 0) {
                    rawY += 240;
                }
                rawYs.push_back(rawY);

                virtualXs.push_back(static_cast<int>(lineState->virtualScrollX));

                int virtualY = static_cast<int>(lineState->virtualScrollY) - scanline;
                virtualY %= 480;
                if(virtualY < 0) {
                    virtualY += 480;
                }
                virtualYs.push_back(virtualY);
            }

            if(!rawXs.empty()) {
                span.rawScrollX = representativeWrappedValue(rawXs, 256);
                span.rawScrollY = representativeWrappedValue(rawYs, 240);
                span.virtualScrollX = representativeWrappedValue(virtualXs, 512);
                span.virtualScrollY = representativeWrappedValue(virtualYs, 480);
            }
        }

        if(!stableScrollSpans.empty()) {
            const auto& firstSpan = stableScrollSpans.front();
            if(const auto* firstLineState = findScanlineTraceState(firstSpan.startScanline)) {
                displayBackgroundPatternTableAddress =
                    static_cast<int>(firstLineState->backgroundPatternTableAddress);
            }
        }
    }

    const auto findTraceStateForNametableRow = [&](int nametableY) -> const GeraNESEmu::PpuViewerScanlineState* {
        for(const auto& span : stableScrollSpans) {
            const int spanLength = span.endScanline - span.startScanline + 1;
            if(spanLength <= 0) {
                continue;
            }

            const int cursorY = wrapCoord(span.virtualScrollY + span.startScanline, kNametableHeight);
            const int delta = wrapCoord(nametableY - cursorY, kNametableHeight);
            if(delta >= spanLength) {
                continue;
            }

            return findScanlineTraceState(span.startScanline + delta);
        }

        return nullptr;
    };

    if(refreshViewer) {
        for(int y = 0; y < kNametableHeight; ++y) {
            const int nameTableRow = y >= 240 ? 2 : 0;
            const int localY = y % 240;
            const int tileY = localY >> 3;
            const int fineY = localY & 0x07;
            const auto* lineState = findTraceStateForNametableRow(y);
            const auto* lineSnapshot = findScanlineTraceSnapshot(lineState);
            const auto& rowChrData = lineSnapshot ? lineSnapshot->chrData : chrData;
            const auto& rowNametableData = lineSnapshot ? lineSnapshot->nametableData : nametableData;
            const auto& rowPaletteData = lineSnapshot ? lineSnapshot->paletteData : paletteData;
            const int rowBackgroundPatternTableAddress = lineState
                ? static_cast<int>(lineState->backgroundPatternTableAddress)
                : backgroundPatternTableAddress;
            const uint8_t rowUniversalBackground = static_cast<uint8_t>(rowPaletteData[0] & 0x3F);

            for(int x = 0; x < kNametableWidth; ++x) {
                const int nameTableIndex = nameTableRow + (x >= 256 ? 1 : 0);
                const int localX = x & 0xFF;
                const int tileX = localX >> 3;
                const int fineX = localX & 0x07;
                const int nameTableBase = nameTableIndex * 0x400;
                const uint8_t tileIndex = rowNametableData[static_cast<size_t>(nameTableBase + (tileY * 32) + tileX)];
                const uint8_t attrByte = rowNametableData[static_cast<size_t>(nameTableBase + 0x3C0 + ((tileY >> 2) * 8) + (tileX >> 2))];
                const int attrShift = ((tileY & 0x02) << 1) | (tileX & 0x02);
                const uint8_t paletteIndex = static_cast<uint8_t>((attrByte >> attrShift) & 0x03);
                const int patternAddr = rowBackgroundPatternTableAddress + (tileIndex * 16) + fineY;
                const uint8_t lowPlane = rowChrData[static_cast<size_t>(patternAddr)];
                const uint8_t highPlane = rowChrData[static_cast<size_t>(patternAddr + 8)];
                const int bit = 7 - fineX;
                const uint8_t colorIndex = static_cast<uint8_t>(((lowPlane >> bit) & 0x01) | (((highPlane >> bit) & 0x01) << 1));

                uint8_t paletteEntry = rowUniversalBackground;
                if(colorIndex != 0) {
                    paletteEntry = static_cast<uint8_t>(rowPaletteData[static_cast<size_t>((paletteIndex * 4) + colorIndex)] & 0x3F);
                }

                m_ppuNametableBuffer[static_cast<size_t>((y * kNametableWidth) + x)] = colorForPaletteEntry(paletteEntry);
            }
        }

        for(int table = 0; table < 2; ++table) {
            const int tableBase = table * 0x1000;
            const int xOffset = table * 128;

            for(int tileY = 0; tileY < 16; ++tileY) {
                for(int tileX = 0; tileX < 16; ++tileX) {
                    const int tileIndex = (tileY * 16) + tileX;

                    for(int fineY = 0; fineY < 8; ++fineY) {
                        const int dstY = (tileY * 8) + fineY;
                        const int nametableY = (dstY * kNametableHeight) / kChrHeight;
                        const auto* chrLineState = findTraceStateForNametableRow(nametableY);
                        const auto* chrLineSnapshot = findScanlineTraceSnapshot(chrLineState);
                        const auto& chrSource = chrLineSnapshot ? chrLineSnapshot->chrData : chrData;
                        const auto& chrPaletteData = chrLineSnapshot ? chrLineSnapshot->paletteData : paletteData;
                        const uint8_t lowPlane = chrSource[static_cast<size_t>(tableBase + (tileIndex * 16) + fineY)];
                        const uint8_t highPlane = chrSource[static_cast<size_t>(tableBase + (tileIndex * 16) + fineY + 8)];

                        for(int fineX = 0; fineX < 8; ++fineX) {
                            const int bit = 7 - fineX;
                            const uint8_t colorIndex = static_cast<uint8_t>(((lowPlane >> bit) & 0x01) | (((highPlane >> bit) & 0x01) << 1));
                            const int dstX = xOffset + (tileX * 8) + fineX;
                            const size_t dstIndex = static_cast<size_t>((dstY * kChrWidth) + dstX);
                            const uint32_t pixel = chrPixelColorForMode(colorIndex, chrPaletteData);
                            m_ppuChrBuffer[dstIndex] = pixel;
                            m_ppuChrExportBuffer[dstIndex] = (pixel & 0x00FFFFFFu) | (static_cast<uint32_t>(colorIndex) << 24);
                        }
                    }
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_ppuNametableTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kNametableWidth, kNametableHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_ppuNametableBuffer.data());
        glBindTexture(GL_TEXTURE_2D, m_ppuChrTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kChrWidth, kChrHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_ppuChrBuffer.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    ImGui::Text("Scroll regions: %d", static_cast<int>(stableScrollSpans.size()));
    ImGui::SameLine();
    ImGui::TextDisabled("Background pattern table: $%04X", displayBackgroundPatternTableAddress);
    ImGui::SameLine();
    ImGui::TextDisabled("PPU: %d,%d", viewerSnapshot.ppuScanline, viewerSnapshot.ppuCycle);
    const int tracedLines = static_cast<int>(std::count_if(
        m_ppuViewerScanlineStates.begin(),
        m_ppuViewerScanlineStates.end(),
        [](const auto& lineState) { return lineState.valid; }
    ));
    ImGui::SameLine();
    ImGui::TextDisabled("Trace lines: %d", tracedLines);
    if(!stableScrollSpans.empty()) {
        const float regionBoxHeight = ImGui::GetTextLineHeightWithSpacing() * 5.5f;
        if(ImGui::BeginChild("PpuViewerRegions", ImVec2(0.0f, regionBoxHeight), true)) {
            for(size_t i = 0; i < stableScrollSpans.size(); ++i) {
                const auto& span = stableScrollSpans[i];
                const int screenStart = span.startScanline;
                const int cursorY =
                    (span.virtualScrollY + screenStart) % kNametableHeight;
                const int cursorX = span.virtualScrollX;
                ImGui::Text(
                    "Region %d: lines %d-%d  cursorX=%d  cursorY=%d",
                    static_cast<int>(i) + 1,
                    screenStart,
                    span.endScanline,
                    cursorX,
                    cursorY
                );
            }
        }
        ImGui::EndChild();
    }
    ImGui::Spacing();

    auto drawPaletteStrip = [&](const char* label, int paletteBaseIndex, int displayBaseAddress) {
        ImGui::BeginGroup();
        ImGui::Text("%s $%04X", label, displayBaseAddress);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 start = ImGui::GetCursorScreenPos();

        for(int i = 0; i < 4; ++i) {
            const uint8_t paletteEntry = static_cast<uint8_t>(paletteData[static_cast<size_t>(paletteBaseIndex + i)] & 0x3F);
            const ImVec2 swatchMin(start.x + i * (kPaletteSwatchSize + kPaletteSwatchSpacing), start.y);
            const ImVec2 swatchMax(swatchMin.x + kPaletteSwatchSize, swatchMin.y + kPaletteSwatchSize);
            drawList->AddRectFilled(swatchMin, swatchMax, colorForPaletteEntry(paletteEntry), 3.0f);
            drawList->AddRect(swatchMin, swatchMax, ImGuiTheme::toU32(ImGuiTheme::textDisabled()), 3.0f);
        }

        ImGui::Dummy(ImVec2((kPaletteSwatchSize * 4.0f) + (kPaletteSwatchSpacing * 3.0f), kPaletteSwatchSize));
        ImGui::EndGroup();
    };

    ImGui::TextUnformatted("PPU Palettes");
    for(int paletteIndex = 0; paletteIndex < 4; ++paletteIndex) {
        if(paletteIndex > 0) {
            ImGui::SameLine();
        }
        drawPaletteStrip(("BG " + std::to_string(paletteIndex)).c_str(), paletteIndex * 4, 0x3F00 + paletteIndex * 4);
    }
    ImGui::NewLine();
    for(int paletteIndex = 0; paletteIndex < 4; ++paletteIndex) {
        if(paletteIndex > 0) {
            ImGui::SameLine();
        }
        drawPaletteStrip(("SPR " + std::to_string(paletteIndex)).c_str(), 0x10 + paletteIndex * 4, 0x3F10 + paletteIndex * 4);
    }
    ImGui::Separator();

    if(ImGui::BeginChild("PpuViewerScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Nametables");
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(m_ppuNametableTexture)),
            ImVec2(static_cast<float>(kNametableWidth), static_cast<float>(kNametableHeight))
        );

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const auto visibleMarkerCoord = [](int wrapped, int maxInclusive) {
            return std::clamp(wrapped, 0, std::max(0, maxInclusive));
        };
        const auto drawWrappedRegionRect =
            [&](int originX, int originY, int width, int height, ImU32 color, float thickness) {
        const int wrappedX = wrapCoord(originX, kNametableWidth);
        const int wrappedY = wrapCoord(originY, kNametableHeight);
        const int rectWidth = std::clamp(width, 0, kNametableWidth);
        const int rectHeight = std::clamp(height, 0, kNametableHeight);
        if(rectWidth <= 0 || rectHeight <= 0) {
            return;
        }

        const int firstWidth = std::min(rectWidth, kNametableWidth - wrappedX);
        const int secondWidth = rectWidth - firstWidth;
        const int firstHeight = std::min(rectHeight, kNametableHeight - wrappedY);
        const int secondHeight = rectHeight - firstHeight;

        const auto drawRectPixels = [&](int x, int y, int w, int h) {
            if(w <= 0 || h <= 0) {
                return;
            }

            drawList->AddRect(
                ImVec2(imageMin.x + static_cast<float>(x), imageMin.y + static_cast<float>(y)),
                ImVec2(imageMin.x + static_cast<float>(x + w), imageMin.y + static_cast<float>(y + h)),
                color,
                0.0f,
                0,
                thickness
            );
        };

        drawRectPixels(wrappedX, wrappedY, firstWidth, firstHeight);
        if(secondWidth > 0) {
            drawRectPixels(0, wrappedY, secondWidth, firstHeight);
        }
        if(secondHeight > 0) {
            drawRectPixels(wrappedX, 0, firstWidth, secondHeight);
        }
        if(secondWidth > 0 && secondHeight > 0) {
            drawRectPixels(0, 0, secondWidth, secondHeight);
        }
        };
        const float time = static_cast<float>(ImGui::GetTime());
        const auto animatedGuideColor = [&](size_t index) -> ImU32 {
            const float hue = std::fmod(time * 1.5f + static_cast<float>(index) * 0.21f, 1.0f);
            return ImGui::ColorConvertFloat4ToU32(ImColor::HSV(hue, 0.85f, 1.0f));
        };
        if(!m_ppuViewerScanlineStates.empty()) {
            for(size_t i = 0; i < stableScrollSpans.size(); ++i) {
                const auto& span = stableScrollSpans[i];
                const int screenStart = span.startScanline;
                const ImU32 guideColor = animatedGuideColor(i);
                const int wrappedX = visibleMarkerCoord(
                    wrapCoord(span.virtualScrollX, kNametableWidth),
                    kNametableWidth - 1
                );
                const int wrappedY = visibleMarkerCoord(
                    wrapCoord(span.virtualScrollY + screenStart, kNametableHeight),
                    kNametableHeight - 1
                );
                const float markerX = imageMin.x + static_cast<float>(wrappedX);
                const float markerY = imageMin.y + static_cast<float>(wrappedY);
                const float thickness = (i == 0) ? 2.0f : 1.0f;
                const int regionHeight = span.endScanline - span.startScanline + 1;
                drawWrappedRegionRect(
                    span.virtualScrollX,
                    span.virtualScrollY + screenStart,
                    256,
                    regionHeight,
                    guideColor,
                    thickness
                );
                const std::string label = "R" + std::to_string(i + 1);
                drawList->AddText(
                    ImVec2(markerX + 4.0f, markerY + 2.0f),
                    guideColor,
                    label.c_str()
                );
            }
        }

        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool hasHoveredNametableChrTile = false;
        int hoveredNametableChrTileX = 0;
        int hoveredNametableChrTileY = 0;
        if(ImGui::IsItemHovered()) {
            const int hoveredX = std::clamp(static_cast<int>(mousePos.x - imageMin.x), 0, kNametableWidth - 1);
            const int hoveredY = std::clamp(static_cast<int>(mousePos.y - imageMin.y), 0, kNametableHeight - 1);
            const int tileX = hoveredX / 8;
            const int tileY = hoveredY / 8;
            const int nameTableIndex = (tileY >= 30 ? 2 : 0) + (tileX >= 32 ? 1 : 0);
            const int localTileX = tileX % 32;
            const int localTileY = tileY % 30;
            const uint16_t tileAddress = static_cast<uint16_t>(0x2000 + nameTableIndex * 0x400 + localTileY * 32 + localTileX);
            const int nameTableBase = nameTableIndex * 0x400;
            const uint8_t tileIndexValue = nametableData[static_cast<size_t>(tileAddress - 0x2000)];
            const uint8_t attrByte = nametableData[static_cast<size_t>(nameTableBase + 0x3C0 + ((localTileY >> 2) * 8) + (localTileX >> 2))];
            const int attrShift = ((localTileY & 0x02) << 1) | (localTileX & 0x02);
            const uint8_t paletteIndex = static_cast<uint8_t>((attrByte >> attrShift) & 0x03);
            const auto* hoveredLineState = findTraceStateForNametableRow(hoveredY);
            const int hoveredPatternTableAddress = hoveredLineState != nullptr
                ? static_cast<int>(hoveredLineState->backgroundPatternTableAddress)
                : backgroundPatternTableAddress;

            hasHoveredNametableChrTile = true;
            hoveredNametableChrTileX =
                ((hoveredPatternTableAddress >= 0x1000) ? 16 : 0) + (static_cast<int>(tileIndexValue) & 0x0F);
            hoveredNametableChrTileY = static_cast<int>(tileIndexValue) >> 4;

            const ImVec2 tileMin(imageMin.x + tileX * 8.0f, imageMin.y + tileY * 8.0f);
            const ImVec2 tileMax(tileMin.x + 8.0f, tileMin.y + 8.0f);
            drawList->AddRect(tileMin, tileMax, ImGuiTheme::toU32(ImGuiTheme::eventWrite()), 0.0f, 0, 2.0f);

            ImGui::BeginTooltip();
            ImGui::Text("Nametable tile");
            ImGui::Text("Tile: (%d, %d)", tileX, tileY);
            ImGui::Text("Nametable addr: $%04X", static_cast<unsigned int>(tileAddress));
            ImGui::Text("Tile index: $%02X", static_cast<unsigned int>(tileIndexValue));
            ImGui::Text("Pattern addr: $%04X", static_cast<unsigned int>(hoveredPatternTableAddress + tileIndexValue * 16));
            ImGui::Text("BG palette: %u ($3F%02X-$3F%02X)",
                        static_cast<unsigned int>(paletteIndex),
                        static_cast<unsigned int>(paletteIndex * 4),
                        static_cast<unsigned int>(paletteIndex * 4 + 3));
            ImGui::Separator();
            for(int i = 0; i < 4; ++i) {
                if(i > 0) {
                    ImGui::SameLine();
                }
                const int paletteOffset = static_cast<int>(paletteIndex) * 4 + i;
                const uint8_t paletteEntry = static_cast<uint8_t>(paletteData[static_cast<size_t>(paletteOffset)] & 0x3F);
                const ImVec2 swatchMin = ImGui::GetCursorScreenPos();
                const ImVec2 swatchMax(swatchMin.x + 16.0f, swatchMin.y + 16.0f);
                ImGui::Dummy(ImVec2(16.0f, 16.0f));
                ImGui::GetWindowDrawList()->AddRectFilled(swatchMin, swatchMax, colorForPaletteEntry(paletteEntry), 2.0f);
                ImGui::GetWindowDrawList()->AddRect(swatchMin, swatchMax, ImGuiTheme::toU32(ImGuiTheme::textDisabled()), 2.0f);
            }
            ImGui::EndTooltip();
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextUnformatted("CHR / Pattern Tables");
        ImGui::TextDisabled("Left: $0000   Right: $1000");
        SetNextItemWidthScaledClamped(160.0f);
        ImGui::Combo("Display palette", &m_ppuViewerChrPaletteMode, kChrPaletteModeLabels.data(), static_cast<int>(kChrPaletteModeLabels.size()));
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(m_ppuChrTexture)),
            ImVec2(static_cast<float>(kChrWidth * 2), static_cast<float>(kChrHeight * 2))
        );

        const ImVec2 chrImageMin = ImGui::GetItemRectMin();
        if(hasHoveredNametableChrTile) {
            const ImVec2 tileMin(
                chrImageMin.x + hoveredNametableChrTileX * 16.0f,
                chrImageMin.y + hoveredNametableChrTileY * 16.0f
            );
            const ImVec2 tileMax(tileMin.x + 16.0f, tileMin.y + 16.0f);
            drawList->AddRect(tileMin, tileMax, ImGuiTheme::toU32(ImGuiTheme::eventWrite()), 0.0f, 0, 2.0f);
        }
        if(ImGui::IsItemHovered()) {
            const int hoveredX = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.x - chrImageMin.x) / 2.0f), 0, kChrWidth - 1);
            const int hoveredY = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.y - chrImageMin.y) / 2.0f), 0, kChrHeight - 1);
            const int tileX = hoveredX / 8;
            const int tileY = hoveredY / 8;
            const int tableIndex = tileX >= 16 ? 1 : 0;
            const int localTileX = tileX % 16;
            const int tileIndex = tileY * 16 + localTileX;
            const uint16_t tileAddress = static_cast<uint16_t>(tableIndex * 0x1000 + tileIndex * 16);

            const ImVec2 tileMin(chrImageMin.x + tileX * 16.0f, chrImageMin.y + tileY * 16.0f);
            const ImVec2 tileMax(tileMin.x + 16.0f, tileMin.y + 16.0f);
            drawList->AddRect(tileMin, tileMax, ImGuiTheme::toU32(ImGuiTheme::eventWrite()), 0.0f, 0, 2.0f);

            ImGui::BeginTooltip();
            ImGui::Text("CHR tile");
            ImGui::Text("Tile: (%d, %d)", tileX, tileY);
            ImGui::Text("Tile index: $%02X", static_cast<unsigned int>(tileIndex));
            ImGui::Text("Pattern addr: $%04X", static_cast<unsigned int>(tileAddress));
            if(hasHoveredNametableChrTile &&
               tileX == hoveredNametableChrTileX &&
               tileY == hoveredNametableChrTileY) {
                ImGui::Text("Hovered nametable tile: yes");
            }
            ImGui::EndTooltip();
        }
        ImGui::EndGroup();
    }
    ImGui::EndChild();

    ImGui::End();
}
