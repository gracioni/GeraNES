#pragma once

inline void GeraNESApp::drawModPixelInspectorWindow()
{
    auto lowerCopy = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    };
    auto appendColorLine = [](std::ostringstream& out, const char* label, uint32_t color) {
        out << label << ": #"
            << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << static_cast<unsigned int>(color & 0xFFu)
            << std::setw(2) << static_cast<unsigned int>((color >> 8) & 0xFFu)
            << std::setw(2) << static_cast<unsigned int>((color >> 16) & 0xFFu)
            << std::setw(2) << static_cast<unsigned int>((color >> 24) & 0xFFu)
            << std::dec << "\n";
    };
    auto appendStageText = [&](std::ostringstream& out, const ModManager::DebugComposeStage& stage) {
        if(!stage.valid) {
            return;
        }
        out << stage.stage << "\n";
        if(!stage.assetPath.empty()) {
            out << "  asset: " << stage.assetPath << "\n";
        }
        if(stage.priority >= 0) {
            out << "  prio: " << stage.priority << "\n";
        }
        if(stage.srcX >= 0 && stage.srcY >= 0) {
            out << "  src: (" << stage.srcX << ", " << stage.srcY << ")\n";
        }
        if(stage.rawRgba != 0 || stage.srcX >= 0 || stage.srcY >= 0) {
            appendColorLine(out, "  rgba", stage.rawRgba);
        }
        if(stage.indexedPaletteIndex >= 0) {
            out << "  index: " << stage.indexedPaletteIndex << "\n";
        }
        out << "  result: " << (stage.returnedBaseColor ? "baseColor" : "applied") << "\n";
        if(!stage.reason.empty()) {
            out << "  why: " << stage.reason << "\n";
        }
    };
    const bool hasStageFilter = !m_modPixelInspectorFilter.empty();
    auto stageMatchesFilter = [&](const ModManager::DebugComposeStage& stage) {
        if(!hasStageFilter) {
            return false;
        }
        const std::string filter = lowerCopy(m_modPixelInspectorFilter);
        return lowerCopy(stage.stage).find(filter) != std::string::npos ||
               lowerCopy(stage.assetPath).find(filter) != std::string::npos ||
               lowerCopy(stage.reason).find(filter) != std::string::npos;
    };
    auto stageVisibleInCompactMode = [&](const ModManager::DebugComposeStage& stage) {
        if(stageMatchesFilter(stage)) {
            return true;
        }
        if(stage.stage == "override candidate") {
            return !stage.returnedBaseColor;
        }
        return !stage.returnedBaseColor || stage.stage == "bg override" || stage.stage == "sprite override";
    };

    ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Screen Pixel Inspector", &m_showModPixelInspectorWindow)) {
        ImGui::End();
        return;
    }

    const bool hasRomLoaded = m_emu.valid();
    const bool modActive = hasRomLoaded && m_modManager.active();
    const int modScale = modActive ? std::clamp(m_modManager.resolutionMultiplier(), 1, 8) : 1;
    const ModManager::OverscanConfig overscan = effectiveOverscan();
    const int activeLeft = overscan.left;
    const int activeRight = PPU::SCREEN_WIDTH - overscan.right;
    const int activeTop = overscan.top;
    const int activeBottom = PPU::SCREEN_HEIGHT - overscan.bottom;
    const int activeWidth = std::max(1, activeRight - activeLeft);
    const int activeHeight = std::max(1, activeBottom - activeTop);
    m_modPixelInspectorZoom = std::clamp(m_modPixelInspectorZoom, 1.0f, 8.0f);

    if(!modActive) {
        m_modPixelInspectorInspectMod = false;
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Zoom", &m_modPixelInspectorZoom, 1.0f, 8.0f, "%.1fx");
    ImGui::SameLine();
    ImGui::BeginDisabled(!modActive);
    ImGui::Checkbox("Inspect mod", &m_modPixelInspectorInspectMod);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Shows the NES screen by default; enable mod inspection to inspect mod output.");
    if(m_modPixelInspectorInspectMod) {
        m_modPixelInspectorBlend = std::clamp(m_modPixelInspectorBlend, 0.0f, 1.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Original");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderFloat("##ModPixelInspectorBlend", &m_modPixelInspectorBlend, 0.0f, 1.0f, "");
        ImGui::SameLine();
        ImGui::TextUnformatted("Mod");
    }
    ImGui::Checkbox("Show sprites", &m_modPixelInspectorShowSprites);
    ImGui::SameLine();
    ImGui::Checkbox("Show background", &m_modPixelInspectorShowBackground);
    ImGui::TextDisabled("Click a pixel to pin its report below.");

    if(!hasRomLoaded) {
        ImGui::TextDisabled("No ROM loaded.");
        ImGui::End();
        return;
    }

    IEmulationHost::ModRenderSnapshot snapshot;
    std::vector<uint32_t> presentedModFramebuffer;
    bool hasSnapshot = m_emu.getModRenderFrame(snapshot, presentedModFramebuffer) && snapshot.valid;
    if(!hasSnapshot) {
        hasSnapshot = m_emu.withExclusiveAccess([&snapshot](auto& emu) {
            if(!emu.valid()) {
                return false;
            }

            PPU& ppu = emu.getConsole().ppu();
            snapshot = {};
            snapshot.valid = true;
            snapshot.frameCount = emu.frameCount();
            snapshot.scale = 1;
            for(int y = 0; y < PPU::SCREEN_HEIGHT; ++y) {
                snapshot.scrollXByLine[static_cast<size_t>(y)] = ppu.debugPresentedScanlineScrollX(y);
                snapshot.scrollYByLine[static_cast<size_t>(y)] = ppu.debugPresentedScanlineScrollY(y);
            }
            snapshot.scrollX = snapshot.scrollXByLine[0];
            snapshot.scrollY = snapshot.scrollYByLine[0];
            snapshot.universalBgColor = static_cast<uint8_t>(ppu.debugPeekPpuMemory(0x3F00) & 0x3F);
            snapshot.backgroundPixels.resize(ppu.debugPresentedBackgroundPixelsCount());
            snapshot.spritePixels.resize(ppu.debugPresentedSpritePixelsCount());
            ppu.debugCopyPresentedBackgroundPixels(snapshot.backgroundPixels);
            ppu.debugCopyPresentedSpritePixels(snapshot.spritePixels);
            for(size_t i = 0; i < snapshot.paletteColors.size(); ++i) {
                snapshot.paletteColors[i] = ppu.NESToRGBAColor(static_cast<uint8_t>(i));
            }
            for(size_t i = 0; i < snapshot.tileHashes.size(); ++i) {
                snapshot.tileHashes[i] = ppu.debugHashChrTile(static_cast<int>(i));
            }
            return !snapshot.backgroundPixels.empty() || !snapshot.spritePixels.empty();
        });
    }
    const uint32_t* sourceFramebuffer = m_emu.getFramebuffer();
    const bool inspectMod = m_modPixelInspectorInspectMod && modActive && hasSnapshot && !presentedModFramebuffer.empty();
    const int inspectorScale = inspectMod ? modScale : 1;
    const float modBlend = inspectMod ? std::clamp(m_modPixelInspectorBlend, 0.0f, 1.0f) : 0.0f;
    const bool showSprites = m_modPixelInspectorShowSprites;
    const bool showBackground = m_modPixelInspectorShowBackground;

    auto backgroundColorFor = [&](const PPU::DebugModBackgroundPixel* bgPixel) -> uint32_t {
        const uint32_t universalColor = snapshot.paletteColors[snapshot.universalBgColor & 0x3Fu];
        if(!showBackground || bgPixel == nullptr || !bgPixel->valid) {
            return universalColor;
        }
        return snapshot.paletteColors[bgPixel->paletteIndex & 0x3Fu];
    };
    auto spriteColorFor = [&](const PPU::DebugModSpriteCandidate& candidate) -> uint32_t {
        if(candidate.colorLowBits == 0) {
            return 0u;
        }
        const int paletteIndex = std::clamp(static_cast<int>(candidate.colorLowBits), 1, 3) - 1;
        return snapshot.paletteColors[candidate.palette[static_cast<size_t>(paletteIndex)] & 0x3Fu];
    };
    auto composeOriginalPixel = [&](const PPU::DebugModBackgroundPixel* bgPixel,
                                    const PPU::DebugModSpritePixel* spritePixel) -> uint32_t {
        const uint32_t universalColor = snapshot.paletteColors[snapshot.universalBgColor & 0x3Fu];
        const bool bgOpaque =
            showBackground &&
            bgPixel != nullptr &&
            bgPixel->valid &&
            bgPixel->colorLowBits != 0;
        const uint32_t bgColor = backgroundColorFor(bgPixel);

        if(showSprites && spritePixel != nullptr) {
            for(int i = 0; i < static_cast<int>(spritePixel->count); ++i) {
                const auto& candidate = spritePixel->candidates[static_cast<size_t>(i)];
                if(!candidate.valid || candidate.colorLowBits == 0) {
                    continue;
                }
                if(showBackground && candidate.behindBackground && bgOpaque) {
                    continue;
                }
                return spriteColorFor(candidate);
            }
        }

        if(showBackground) {
            return bgColor;
        }
        return universalColor;
    };

    const int statusScale = hasSnapshot ? std::clamp(snapshot.scale, 1, 8) : inspectorScale;
    const ModManager::OverscanConfig statusOverscan = effectiveOverscan();
    ImGui::Text(
        "Frame %u | Scale %dx | Overscan T%d R%d B%d L%d",
        hasSnapshot ? snapshot.frameCount : 0u,
        statusScale,
        statusOverscan.top,
        statusOverscan.right,
        statusOverscan.bottom,
        statusOverscan.left
    );

    if(!ImGui::BeginChild("ModPixelInspectorScroll", ImVec2(0.0f, -180.0f), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    if(m_modPixelInspectorTexture == 0) {
        glGenTextures(1, &m_modPixelInspectorTexture);
        glBindTexture(GL_TEXTURE_2D, m_modPixelInspectorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            m_modPixelInspectorTextureWidth,
            m_modPixelInspectorTextureHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr
        );
    }

    const int inspectorTextureWidth = 256 * inspectorScale;
    const int inspectorTextureHeight = 256 * inspectorScale;
    if(m_modPixelInspectorTextureWidth != inspectorTextureWidth ||
       m_modPixelInspectorTextureHeight != inspectorTextureHeight) {
        m_modPixelInspectorTextureWidth = inspectorTextureWidth;
        m_modPixelInspectorTextureHeight = inspectorTextureHeight;
        glBindTexture(GL_TEXTURE_2D, m_modPixelInspectorTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            inspectorTextureWidth,
            inspectorTextureHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr
        );
    }

    const size_t inspectorBufferSize =
        static_cast<size_t>(inspectorTextureWidth) * static_cast<size_t>(inspectorTextureHeight);
    if(m_modPixelInspectorTextureUploadBuffer.size() != inspectorBufferSize) {
        m_modPixelInspectorTextureUploadBuffer.assign(inspectorBufferSize, 0u);
    } else {
        std::fill(m_modPixelInspectorTextureUploadBuffer.begin(), m_modPixelInspectorTextureUploadBuffer.end(), 0u);
    }

    constexpr int kInspectorFramebufferHeight = 256;
    std::vector<uint32_t> originalInspectorFramebuffer(
        static_cast<size_t>(PPU::SCREEN_WIDTH * kInspectorFramebufferHeight),
        0u);
    const bool canUsePresentedFramebufferDirectly = sourceFramebuffer != nullptr;
    if(canUsePresentedFramebufferDirectly) {
        std::memcpy(
            originalInspectorFramebuffer.data(),
            sourceFramebuffer,
            static_cast<size_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT) * sizeof(uint32_t)
        );
    } else if(hasSnapshot) {
        ModManager::ChrRenderSnapshot baseSnapshot;
        baseSnapshot.scrollX = snapshot.scrollX;
        baseSnapshot.scrollY = snapshot.scrollY;
        baseSnapshot.scrollXByLine = snapshot.scrollXByLine;
        baseSnapshot.scrollYByLine = snapshot.scrollYByLine;
        baseSnapshot.universalBgColor = snapshot.universalBgColor;
        baseSnapshot.paletteColors = snapshot.paletteColors;
        baseSnapshot.tileHashes = snapshot.tileHashes;
        baseSnapshot.backgroundPixels = snapshot.backgroundPixels;
        baseSnapshot.spritePixels = snapshot.spritePixels;
        baseSnapshot.backgroundPixelsView = baseSnapshot.backgroundPixels.data();
        baseSnapshot.backgroundPixelsViewCount = baseSnapshot.backgroundPixels.size();
        baseSnapshot.spritePixelsView = baseSnapshot.spritePixels.data();
        baseSnapshot.spritePixelsViewCount = baseSnapshot.spritePixels.size();
        baseSnapshot.frameConditionStateView = nullptr;
        baseSnapshot.frameConditionState.frameCount = snapshot.frameConditionState.frameCount;
        baseSnapshot.frameConditionState.memoryValues = snapshot.frameConditionState.memoryValues;
        std::vector<uint32_t> neutralInspectorFramebuffer(
            static_cast<size_t>(PPU::SCREEN_WIDTH * kInspectorFramebufferHeight),
            snapshot.paletteColors[snapshot.universalBgColor & 0x3Fu]);
        m_modManager.composeChrFrame(
            originalInspectorFramebuffer,
            PPU::SCREEN_WIDTH,
            kInspectorFramebufferHeight,
            0,
            PPU::SCREEN_HEIGHT,
            1,
            neutralInspectorFramebuffer.data(),
            baseSnapshot,
            nullptr,
            false
        );
    } else if(sourceFramebuffer != nullptr) {
        std::memcpy(
            originalInspectorFramebuffer.data(),
            sourceFramebuffer,
            static_cast<size_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT) * sizeof(uint32_t)
        );
    }

    const bool applyLayerFilter =
        hasSnapshot &&
        (!showBackground || !showSprites) &&
        snapshot.backgroundPixels.size() >= static_cast<size_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT) &&
        snapshot.spritePixels.size() >= static_cast<size_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT);
    std::vector<uint32_t> filteredOriginalInspectorFramebuffer;
    if(applyLayerFilter) {
        filteredOriginalInspectorFramebuffer.assign(
            static_cast<size_t>(PPU::SCREEN_WIDTH * kInspectorFramebufferHeight),
            snapshot.paletteColors[snapshot.universalBgColor & 0x3Fu]);
        for(int y = 0; y < PPU::SCREEN_HEIGHT; ++y) {
            uint32_t* dstRow =
                filteredOriginalInspectorFramebuffer.data() +
                static_cast<size_t>(y) * PPU::SCREEN_WIDTH;
            for(int x = 0; x < PPU::SCREEN_WIDTH; ++x) {
                const size_t pixelIndex = static_cast<size_t>(y) * PPU::SCREEN_WIDTH + static_cast<size_t>(x);
                dstRow[static_cast<size_t>(x)] = composeOriginalPixel(
                    &snapshot.backgroundPixels[pixelIndex],
                    &snapshot.spritePixels[pixelIndex]);
            }
        }
    }
    const uint32_t* inspectorComposeSourceFramebuffer =
        applyLayerFilter
            ? filteredOriginalInspectorFramebuffer.data()
            : (sourceFramebuffer != nullptr ? sourceFramebuffer : originalInspectorFramebuffer.data());
    const std::vector<uint32_t>& displayOriginalInspectorFramebuffer =
        applyLayerFilter ? filteredOriginalInspectorFramebuffer : originalInspectorFramebuffer;

    if(inspectMod) {
        std::vector<uint32_t> modInspectorFramebuffer;
        const bool canUsePresentedModFramebufferDirectly =
            showSprites &&
            showBackground &&
            presentedModFramebuffer.size() == static_cast<size_t>(inspectorTextureWidth * inspectorTextureHeight);
        if(canUsePresentedModFramebufferDirectly) {
            modInspectorFramebuffer = presentedModFramebuffer;
        } else {
            ModManager::ChrRenderSnapshot inspectorSnapshot;
            inspectorSnapshot.scrollX = snapshot.scrollX;
            inspectorSnapshot.scrollY = snapshot.scrollY;
            inspectorSnapshot.scrollXByLine = snapshot.scrollXByLine;
            inspectorSnapshot.scrollYByLine = snapshot.scrollYByLine;
            inspectorSnapshot.universalBgColor = snapshot.universalBgColor;
            inspectorSnapshot.paletteColors = snapshot.paletteColors;
            inspectorSnapshot.tileHashes = snapshot.tileHashes;
            if(showBackground) {
                inspectorSnapshot.backgroundPixels = snapshot.backgroundPixels;
            }
            if(showSprites) {
                inspectorSnapshot.spritePixels = snapshot.spritePixels;
            }
            inspectorSnapshot.backgroundPixelsView = inspectorSnapshot.backgroundPixels.data();
            inspectorSnapshot.backgroundPixelsViewCount = inspectorSnapshot.backgroundPixels.size();
            inspectorSnapshot.spritePixelsView = inspectorSnapshot.spritePixels.data();
            inspectorSnapshot.spritePixelsViewCount = inspectorSnapshot.spritePixels.size();
            inspectorSnapshot.frameConditionStateView = nullptr;
            inspectorSnapshot.frameConditionState.frameCount = snapshot.frameConditionState.frameCount;
            inspectorSnapshot.frameConditionState.memoryValues = snapshot.frameConditionState.memoryValues;
            modInspectorFramebuffer.assign(static_cast<size_t>(inspectorTextureWidth * inspectorTextureHeight), 0u);

            m_modManager.composeChrFrame(
                modInspectorFramebuffer,
                inspectorTextureWidth,
                inspectorTextureHeight,
                activeTop * inspectorScale,
                activeBottom * inspectorScale,
                inspectorScale,
                inspectorComposeSourceFramebuffer,
                inspectorSnapshot,
                nullptr,
                true
            );
        }

        auto blendRgba = [](uint32_t originalColor, uint32_t modColor, float blend) {
            const auto blendChannel = [blend](uint32_t originalChannel, uint32_t modChannel) -> uint32_t {
                const float value =
                    static_cast<float>(originalChannel) +
                    (static_cast<float>(modChannel) - static_cast<float>(originalChannel)) * blend;
                return static_cast<uint32_t>(std::clamp(static_cast<int>(std::lround(value)), 0, 255));
            };

            const uint32_t r = blendChannel(originalColor & 0xFFu, modColor & 0xFFu);
            const uint32_t g = blendChannel((originalColor >> 8) & 0xFFu, (modColor >> 8) & 0xFFu);
            const uint32_t b = blendChannel((originalColor >> 16) & 0xFFu, (modColor >> 16) & 0xFFu);
            const uint32_t a = blendChannel((originalColor >> 24) & 0xFFu, (modColor >> 24) & 0xFFu);
            return r | (g << 8) | (b << 16) | (a << 24);
        };

        if(!modInspectorFramebuffer.empty()) {
            for(int y = 0; y < inspectorTextureHeight; ++y) {
                const int nesY = y / inspectorScale;
                const size_t originalRowOffset = static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
                const size_t modRowOffset = static_cast<size_t>(y) * static_cast<size_t>(inspectorTextureWidth);
                for(int x = 0; x < inspectorTextureWidth; ++x) {
                    const int nesX = x / inspectorScale;
                    const uint32_t originalColor = displayOriginalInspectorFramebuffer[originalRowOffset + static_cast<size_t>(nesX)];
                    const uint32_t modColor = modInspectorFramebuffer[modRowOffset + static_cast<size_t>(x)];
                    m_modPixelInspectorTextureUploadBuffer[modRowOffset + static_cast<size_t>(x)] =
                        blendRgba(originalColor, modColor, modBlend);
                }
            }
        } else {
            for(int y = 0; y < inspectorTextureHeight; ++y) {
                const int nesY = y / inspectorScale;
                uint32_t* dstRow =
                    m_modPixelInspectorTextureUploadBuffer.data() +
                    static_cast<size_t>(y) * static_cast<size_t>(inspectorTextureWidth);
                const uint32_t* srcRow =
                    originalInspectorFramebuffer.data() + static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
                for(int x = 0; x < inspectorTextureWidth; ++x) {
                    dstRow[static_cast<size_t>(x)] = srcRow[static_cast<size_t>(x / inspectorScale)];
                }
            }
        }
    } else if(!originalInspectorFramebuffer.empty()) {
        for(int y = activeTop; y < activeBottom; ++y) {
            uint32_t* dstRow =
                m_modPixelInspectorTextureUploadBuffer.data() +
                static_cast<size_t>(y * inspectorScale) * static_cast<size_t>(inspectorTextureWidth);
            if(!applyLayerFilter) {
                const uint32_t* srcRow = originalInspectorFramebuffer.data() + static_cast<size_t>(y) * PPU::SCREEN_WIDTH;
                std::memcpy(dstRow, srcRow, static_cast<size_t>(PPU::SCREEN_WIDTH) * sizeof(uint32_t));
                continue;
            }

            const uint32_t* srcRow =
                filteredOriginalInspectorFramebuffer.data() + static_cast<size_t>(y) * PPU::SCREEN_WIDTH;
            std::memcpy(dstRow, srcRow, static_cast<size_t>(PPU::SCREEN_WIDTH) * sizeof(uint32_t));
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_modPixelInspectorTexture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        inspectorTextureWidth,
        inspectorTextureHeight,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        m_modPixelInspectorTextureUploadBuffer.data()
    );

    const ImVec2 imageSize(static_cast<float>(activeWidth) * m_modPixelInspectorZoom, static_cast<float>(activeHeight) * m_modPixelInspectorZoom);
    const float textureWidth = static_cast<float>(std::max(1, inspectorTextureWidth));
    const float textureHeight = static_cast<float>(std::max(1, inspectorTextureHeight));
    const float uvLeft = static_cast<float>(activeLeft * inspectorScale) / textureWidth;
    const float uvRight = static_cast<float>(activeRight * inspectorScale) / textureWidth;
    const float uvTop = static_cast<float>(activeTop * inspectorScale) / textureHeight;
    const float uvBottom = static_cast<float>(activeBottom * inspectorScale) / textureHeight;
    ImGui::Image(
        static_cast<ImTextureID>(static_cast<uintptr_t>(m_modPixelInspectorTexture)),
        imageSize,
        ImVec2(uvLeft, uvTop),
        ImVec2(uvRight, uvBottom)
    );

    const ImVec2 imageMin = ImGui::GetItemRectMin();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool imageClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if(ImGui::IsItemHovered()) {
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const int hoveredLocalX = std::clamp(static_cast<int>((mousePos.x - imageMin.x) / m_modPixelInspectorZoom), 0, activeWidth - 1);
        const int hoveredX = activeLeft + hoveredLocalX;
        const int hoveredLocalY = std::clamp(static_cast<int>((mousePos.y - imageMin.y) / m_modPixelInspectorZoom), 0, activeHeight - 1);
        const int hoveredY = activeTop + hoveredLocalY;
        const size_t pixelIndex = static_cast<size_t>(hoveredY) * PPU::SCREEN_WIDTH + static_cast<size_t>(hoveredX);
        const ImVec2 pixelMin(
            imageMin.x + static_cast<float>(hoveredLocalX) * m_modPixelInspectorZoom,
            imageMin.y + static_cast<float>(hoveredLocalY) * m_modPixelInspectorZoom
        );
        const ImVec2 pixelMax(pixelMin.x + m_modPixelInspectorZoom, pixelMin.y + m_modPixelInspectorZoom);
        drawList->AddRect(pixelMin, pixelMax, ImGuiTheme::toU32(ImGuiTheme::eventWrite()), 0.0f, 0, 2.0f);

        uint32_t finalColor = 0;
        const size_t sampleX = static_cast<size_t>(hoveredX * inspectorScale);
        const size_t sampleY = static_cast<size_t>(hoveredY * inspectorScale);
        if(sampleY < static_cast<size_t>(inspectorTextureHeight) &&
           sampleX < static_cast<size_t>(inspectorTextureWidth) &&
           m_modPixelInspectorTextureUploadBuffer.size() == inspectorBufferSize) {
            finalColor = m_modPixelInspectorTextureUploadBuffer[sampleY * static_cast<size_t>(inspectorTextureWidth) + sampleX];
        }

        std::ostringstream copyText;
        copyText << "Screen pixel\n";
        copyText << "NES: (" << hoveredX << ", " << hoveredY << ")\n";
        appendColorLine(copyText, "Rendered RGBA", finalColor);

        ImGui::BeginTooltip();
        ImGui::Text("Screen pixel");
        ImGui::Text("NES: (%d, %d)", hoveredX, hoveredY);
        ImGui::Text("Rendered RGBA: #%02X%02X%02X%02X",
            static_cast<unsigned int>(finalColor & 0xFFu),
            static_cast<unsigned int>((finalColor >> 8) & 0xFFu),
            static_cast<unsigned int>((finalColor >> 16) & 0xFFu),
            static_cast<unsigned int>((finalColor >> 24) & 0xFFu));

        std::optional<ModManager::DebugComposePixel> composeDebug;
        if(inspectMod && !originalInspectorFramebuffer.empty()) {
            ModManager::ChrRenderSnapshot chrSnapshot;
            chrSnapshot.scrollX = snapshot.scrollX;
            chrSnapshot.scrollY = snapshot.scrollY;
            chrSnapshot.scrollXByLine = snapshot.scrollXByLine;
            chrSnapshot.scrollYByLine = snapshot.scrollYByLine;
            chrSnapshot.universalBgColor = snapshot.universalBgColor;
            chrSnapshot.paletteColors = snapshot.paletteColors;
            chrSnapshot.tileHashes = snapshot.tileHashes;
            chrSnapshot.backgroundPixels = snapshot.backgroundPixels;
            chrSnapshot.spritePixels = snapshot.spritePixels;
            chrSnapshot.frameConditionState.frameCount = snapshot.frameConditionState.frameCount;
            chrSnapshot.frameConditionState.memoryValues = snapshot.frameConditionState.memoryValues;
            composeDebug = m_modManager.debugComposePixel(originalInspectorFramebuffer.data(), chrSnapshot, modScale, hoveredX, hoveredY, m_modPixelInspectorFilter);
        }

        if(inspectMod && composeDebug.has_value() && composeDebug->valid) {
            ImGui::Text("Base RGBA: #%02X%02X%02X%02X",
                static_cast<unsigned int>(composeDebug->baseColor & 0xFFu),
                static_cast<unsigned int>((composeDebug->baseColor >> 8) & 0xFFu),
                static_cast<unsigned int>((composeDebug->baseColor >> 16) & 0xFFu),
                static_cast<unsigned int>((composeDebug->baseColor >> 24) & 0xFFu));
            ImGui::Text("Debug final: #%02X%02X%02X%02X",
                static_cast<unsigned int>(composeDebug->finalColor & 0xFFu),
                static_cast<unsigned int>((composeDebug->finalColor >> 8) & 0xFFu),
                static_cast<unsigned int>((composeDebug->finalColor >> 16) & 0xFFu),
                static_cast<unsigned int>((composeDebug->finalColor >> 24) & 0xFFu));
            appendColorLine(copyText, "Base RGBA", composeDebug->baseColor);
            appendColorLine(copyText, "Debug final", composeDebug->finalColor);
        }

        ImGui::Separator();
        ImGui::Text("Background");
        copyText << "\nBackground\n";
        if(hasSnapshot &&
           pixelIndex < snapshot.backgroundPixels.size() &&
           pixelIndex < snapshot.spritePixels.size()) {
            const auto& bg = snapshot.backgroundPixels[pixelIndex];
            const auto& sprite = snapshot.spritePixels[pixelIndex];

            if(bg.valid) {
                const uint32_t bgHash =
                    bg.tileHash != 0 ? bg.tileHash :
                    (bg.tileIndex < snapshot.tileHashes.size()
                        ? snapshot.tileHashes[bg.tileIndex]
                        : 0u);
                ImGui::Text("Tile index: $%03X", static_cast<unsigned int>(bg.tileIndex));
                ImGui::Text("Pattern addr: $%04X", static_cast<unsigned int>(bg.tileIndex * 16u));
                ImGui::Text("Tile hash: %08X", static_cast<unsigned int>(bgHash));
                ImGui::Text("Palette index: $%02X", static_cast<unsigned int>(bg.paletteIndex));
                ImGui::Text("Palette colors: %02X %02X %02X", bg.palette[0], bg.palette[1], bg.palette[2]);
                ImGui::Text("Color bits: %u", static_cast<unsigned int>(bg.colorLowBits));
                ImGui::Text("Tile offset: (%u, %u)", static_cast<unsigned int>(bg.offsetX), static_cast<unsigned int>(bg.offsetY));
                copyText << "Tile index: $" << std::uppercase << std::hex << static_cast<unsigned int>(bg.tileIndex) << std::dec << "\n";
                copyText << "Pattern addr: $" << std::uppercase << std::hex << static_cast<unsigned int>(bg.tileIndex * 16u) << std::dec << "\n";
                copyText << "Tile hash: " << std::uppercase << std::hex << static_cast<unsigned int>(bgHash) << std::dec << "\n";
                copyText << "Palette index: $" << std::uppercase << std::hex << static_cast<unsigned int>(bg.paletteIndex) << std::dec << "\n";
                copyText << "Palette colors: "
                         << std::uppercase << std::hex
                         << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(bg.palette[0]) << " "
                         << std::setw(2) << static_cast<unsigned int>(bg.palette[1]) << " "
                         << std::setw(2) << static_cast<unsigned int>(bg.palette[2])
                         << std::dec << "\n";
                copyText << "Color bits: " << static_cast<unsigned int>(bg.colorLowBits) << "\n";
                copyText << "Tile offset: (" << static_cast<unsigned int>(bg.offsetX) << ", " << static_cast<unsigned int>(bg.offsetY) << ")\n";
            } else {
                ImGui::TextDisabled("No background pixel captured.");
                copyText << "No background pixel captured.\n";
            }

            ImGui::Separator();
            ImGui::Text("Sprites");
            copyText << "\nSprites\n";
            std::vector<const PPU::DebugModSpriteCandidate*> orderedCandidates;
            orderedCandidates.reserve(sprite.count);
            const PPU::DebugModSpriteCandidate* renderedCandidate = nullptr;
            const bool bgOpaqueForOrdering = showBackground && bg.valid && bg.colorLowBits != 0;
            for(int i = 0; i < static_cast<int>(sprite.count); ++i) {
                const auto& candidate = sprite.candidates[static_cast<size_t>(i)];
                if(renderedCandidate == nullptr &&
                   candidate.valid &&
                   candidate.colorLowBits != 0 &&
                   (!showBackground || !candidate.behindBackground || !bgOpaqueForOrdering)) {
                    renderedCandidate = &candidate;
                }
            }
            if(renderedCandidate != nullptr) {
                orderedCandidates.push_back(renderedCandidate);
            }
            for(int i = 0; i < static_cast<int>(sprite.count); ++i) {
                const auto& candidate = sprite.candidates[static_cast<size_t>(i)];
                if(candidate.valid) {
                    if(&candidate == renderedCandidate) {
                        continue;
                    }
                    orderedCandidates.push_back(&candidate);
                }
            }
            if(!orderedCandidates.empty()) {
                for(size_t i = 0; i < orderedCandidates.size(); ++i) {
                    const auto& candidate = *orderedCandidates[i];
                    if(!candidate.valid) {
                        continue;
                    }
                    const uint32_t spriteHash =
                        candidate.tileIndex < snapshot.tileHashes.size()
                            ? snapshot.tileHashes[candidate.tileIndex]
                            : 0u;
                    ImGui::Text(
                        "#%d %s tile $%03X hash %08X pal %02X %02X %02X bits %u off (%u,%u) %s%s%s",
                        static_cast<int>(i),
                        i == 0 ? "[rendered]" : "",
                        static_cast<unsigned int>(candidate.tileIndex),
                        static_cast<unsigned int>(spriteHash),
                        static_cast<unsigned int>(candidate.palette[0]),
                        static_cast<unsigned int>(candidate.palette[1]),
                        static_cast<unsigned int>(candidate.palette[2]),
                        static_cast<unsigned int>(candidate.colorLowBits),
                        static_cast<unsigned int>(candidate.offsetX),
                        static_cast<unsigned int>(candidate.offsetY),
                        candidate.behindBackground ? "behind-bg " : "",
                        candidate.horizontalMirror ? "hflip " : "",
                        candidate.verticalMirror ? "vflip" : ""
                    );
                    copyText << "#" << i << " " << (i == 0 ? "[rendered] " : "")
                             << " tile $" << std::uppercase << std::hex << static_cast<unsigned int>(candidate.tileIndex)
                             << " hash " << static_cast<unsigned int>(spriteHash)
                             << std::dec
                             << " pal "
                             << std::uppercase << std::hex
                             << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(candidate.palette[0]) << " "
                             << std::setw(2) << static_cast<unsigned int>(candidate.palette[1]) << " "
                             << std::setw(2) << static_cast<unsigned int>(candidate.palette[2])
                             << std::dec
                             << " bits " << static_cast<unsigned int>(candidate.colorLowBits)
                             << " off (" << static_cast<unsigned int>(candidate.offsetX) << "," << static_cast<unsigned int>(candidate.offsetY) << ") ";
                    if(candidate.behindBackground) copyText << "behind-bg ";
                    if(candidate.horizontalMirror) copyText << "hflip ";
                    if(candidate.verticalMirror) copyText << "vflip";
                    copyText << "\n";
                }
            } else {
                ImGui::TextDisabled("No sprite candidates captured.");
                copyText << "No sprite candidates captured.\n";
            }

            if(inspectMod && composeDebug.has_value() && composeDebug->valid) {
                auto drawStage = [](const ModManager::DebugComposeStage& stage) {
                    if(!stage.valid) {
                        return;
                    }
                    ImGui::Text("%s", stage.stage.c_str());
                    if(!stage.assetPath.empty()) {
                        ImGui::Text("  asset: %s", stage.assetPath.c_str());
                    }
                    if(stage.priority >= 0) {
                        ImGui::Text("  prio: %d", stage.priority);
                    }
                    if(stage.srcX >= 0 && stage.srcY >= 0) {
                        ImGui::Text("  src: (%d, %d)", stage.srcX, stage.srcY);
                    }
                    if(stage.rawRgba != 0 || stage.srcX >= 0 || stage.srcY >= 0) {
                        ImGui::Text("  rgba: #%02X%02X%02X%02X",
                            static_cast<unsigned int>(stage.rawRgba & 0xFFu),
                            static_cast<unsigned int>((stage.rawRgba >> 8) & 0xFFu),
                            static_cast<unsigned int>((stage.rawRgba >> 16) & 0xFFu),
                            static_cast<unsigned int>((stage.rawRgba >> 24) & 0xFFu));
                    }
                    if(stage.indexedPaletteIndex >= 0) {
                        ImGui::Text("  index: %d", stage.indexedPaletteIndex);
                    }
                    ImGui::Text("  result: %s", stage.returnedBaseColor ? "baseColor" : "applied");
                    if(!stage.reason.empty()) {
                        ImGui::TextWrapped("  why: %s", stage.reason.c_str());
                    }
                };

                ImGui::Separator();
                ImGui::Text("Compose debug");
                copyText << "\nCompose debug\n";
                int compactStageCount = 0;
                constexpr int kCompactStageLimit = 24;
                auto includeStage = [&](const ModManager::DebugComposeStage& stage) {
                    const bool visible = hasStageFilter ? stageMatchesFilter(stage) : stageVisibleInCompactMode(stage);
                    if(!visible) {
                        return false;
                    }
                    if(hasStageFilter) {
                        return true;
                    }
                    if(compactStageCount >= kCompactStageLimit) {
                        return false;
                    }
                    compactStageCount++;
                    return true;
                };
                for(const auto& stage : composeDebug->backgroundCandidates) {
                    if(includeStage(stage)) {
                        drawStage(stage);
                        appendStageText(copyText, stage);
                    }
                }
                if(composeDebug->backgroundOverride.has_value()) {
                    if(includeStage(*composeDebug->backgroundOverride)) {
                        drawStage(*composeDebug->backgroundOverride);
                        appendStageText(copyText, *composeDebug->backgroundOverride);
                    }
                }
                for(const auto& stage : composeDebug->backgroundStages) {
                    if(includeStage(stage)) {
                        drawStage(stage);
                        appendStageText(copyText, stage);
                    }
                }
                for(const auto& stage : composeDebug->spriteStages) {
                    if(includeStage(stage)) {
                        drawStage(stage);
                        appendStageText(copyText, stage);
                    }
                }
            } else if(m_modPixelInspectorInspectMod && modActive) {
                ImGui::Separator();
                ImGui::TextDisabled("Mod debug snapshot unavailable for this frame.");
                copyText << "\nMod debug snapshot unavailable for this frame.\n";
            }
        } else {
            ImGui::TextDisabled("Background debug unavailable.");
            copyText << "Background debug unavailable.\n";
            ImGui::Separator();
            ImGui::Text("Sprites");
            ImGui::TextDisabled("Sprite debug unavailable.");
            copyText << "\nSprites\n";
            copyText << "Sprite debug unavailable.\n";
        }
        if(imageClicked) {
            m_modPixelInspectorLastDebugText = copyText.str();
        }
        ImGui::EndTooltip();
    }

    ImGui::EndChild();
    ImGui::Separator();
    ImGui::Text("Selected pixel debug");
    if(m_modPixelInspectorLastDebugText.empty()) {
        ImGui::TextDisabled("No pixel selected.");
    } else {
        ImGui::InputTextMultiline(
            "##ModPixelInspectorPinnedDebug",
            &m_modPixelInspectorLastDebugText,
            ImVec2(-FLT_MIN, 150.0f),
            ImGuiInputTextFlags_ReadOnly);
        if(ImGui::Button("Copy Selected Debug")) {
            ImGui::SetClipboardText(m_modPixelInspectorLastDebugText.c_str());
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear Selected Debug")) {
            m_modPixelInspectorLastDebugText.clear();
        }
    }
    ImGui::End();
}
