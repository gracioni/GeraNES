#pragma once

inline void GeraNESApp::fillNoRomStaticFramebuffer()
{
    if(m_framebufferUploadCopy.size() != static_cast<size_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT)) {
        m_framebufferUploadCopy.assign(static_cast<size_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT), 0u);
    }

    auto hash32 = [](uint32_t value) -> uint32_t {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    };

    const uint32_t tick = static_cast<uint32_t>(SDL_GetTicks());
    const uint32_t frameSeed = tick / 16u;
    const uint32_t slowSeed = tick / 61u;
    const uint32_t coarseSeed = tick / 97u;
    const int syncBandCenter = static_cast<int>((tick / 4u) % static_cast<uint32_t>(PPU::SCREEN_HEIGHT + 72)) - 36;
    const int syncBandWidth = 10 + static_cast<int>(hash32(slowSeed ^ 0x2468ace1u) % 8u);
    const int tearBandA = static_cast<int>(hash32(slowSeed ^ 0x51f2ab13u) % 211u);
    const int tearBandB = static_cast<int>(hash32(slowSeed ^ 0x9e3779b9u) % 197u);
    const int tearBandC = static_cast<int>(hash32(slowSeed ^ 0xa24baed4u) % 193u);
    const int tearOffsetA = static_cast<int>(hash32(frameSeed ^ 0x1b56c4e9u) % 17u) - 8;
    const int tearOffsetB = static_cast<int>(hash32((frameSeed / 2u) ^ 0x7f4a7c15u) % 13u) - 6;
    const int tearOffsetC = static_cast<int>(hash32((frameSeed / 3u) ^ 0x52dce729u) % 11u) - 5;
    const int dropoutBand = static_cast<int>(hash32(slowSeed ^ 0xc13fa9a9u) % 223u);
    const int vignetteStrength = 10 + static_cast<int>(hash32(coarseSeed ^ 0x31415926u) % 10u);

    for(int y = 0; y < PPU::SCREEN_HEIGHT; ++y) {
        int rowShift = 0;
        if(y >= tearBandA && y < tearBandA + 3) rowShift = tearOffsetA;
        if(y >= tearBandB && y < tearBandB + 2) rowShift += tearOffsetB;
        if(y >= tearBandC && y < tearBandC + 1) rowShift += tearOffsetC;

        const int syncDistance = std::abs(y - syncBandCenter);
        const int syncBoost = std::max(0, 56 - (syncDistance * 56) / std::max(1, syncBandWidth));
        const int scanlineBias = (y & 1) == 0 ? 7 : -9;
        const int rowDropout = (y >= dropoutBand && y < dropoutBand + 2) ? -34 : 0;
        uint32_t* dstRow = m_framebufferUploadCopy.data() + static_cast<size_t>(y) * PPU::SCREEN_WIDTH;

        for(int x = 0; x < PPU::SCREEN_WIDTH; ++x) {
            const int sampleX = (x + rowShift + PPU::SCREEN_WIDTH) % PPU::SCREEN_WIDTH;
            const uint32_t fineNoise = hash32(
                static_cast<uint32_t>(sampleX) * 73856093u ^
                static_cast<uint32_t>(y) * 19349663u ^
                frameSeed * 83492791u
            );
            const uint32_t coarseNoise = hash32(
                static_cast<uint32_t>(sampleX >> 2) * 2654435761u ^
                static_cast<uint32_t>(y >> 1) * 2246822519u ^
                coarseSeed * 3266489917u
            );
            const uint32_t cloudNoise = hash32(
                static_cast<uint32_t>(sampleX / 9) * 1597334677u ^
                static_cast<uint32_t>(y / 7) * 3812015801u ^
                slowSeed * 668265263u
            );
            const uint32_t streakNoise = hash32(
                static_cast<uint32_t>(sampleX / 20) * 122949829u ^
                static_cast<uint32_t>(y) * 275604541u ^
                slowSeed * 198491317u
            );

            const uint32_t previousPixel = dstRow[x];
            const int previousLuma =
                (static_cast<int>(previousPixel & 0xFFu) +
                 static_cast<int>((previousPixel >> 8) & 0xFFu) +
                 static_cast<int>((previousPixel >> 16) & 0xFFu)) / 3;

            int value = 92;
            value += static_cast<int>((fineNoise >> 24) & 0xFF) - 128;
            value += (static_cast<int>((coarseNoise >> 24) & 0xFF) - 128) / 3;
            value += (static_cast<int>((cloudNoise >> 24) & 0xFF) - 128) / 5;
            value += (static_cast<int>((streakNoise >> 24) & 0xFF) - 128) / 6;
            value += syncBoost;
            value += scanlineBias;
            value += rowDropout;

            const int edgeFalloffX = std::abs(x - (PPU::SCREEN_WIDTH / 2));
            const int edgeFalloffY = std::abs(y - (PPU::SCREEN_HEIGHT / 2));
            value -= (edgeFalloffX * vignetteStrength) / 256;
            value -= (edgeFalloffY * vignetteStrength) / 220;

            if((fineNoise & 0x1ffu) == 0u) value += 95;
            if(((coarseNoise >> 8) & 0x3ffu) < 3u) value -= 55;
            if(((streakNoise >> 11) & 0xffu) > 244u) value += 24;

            // Blend with the previous idle frame to create shimmer instead of full random replacement.
            value = (value * 7 + previousLuma * 5) / 12;

            value = std::clamp(value, 0, 255);

            const uint8_t base = static_cast<uint8_t>(value);
            const uint8_t red = static_cast<uint8_t>(std::clamp(value - 11 + static_cast<int>((cloudNoise >> 9) & 0x7), 0, 255));
            const uint8_t green = static_cast<uint8_t>(std::clamp(value + static_cast<int>((fineNoise >> 13) & 0x7) - 4, 0, 255));
            const uint8_t blue = static_cast<uint8_t>(std::clamp(value + 10 + static_cast<int>((coarseNoise >> 17) & 0x7), 0, 255));

            dstRow[x] = 0xFF000000u |
                        static_cast<uint32_t>(red) |
                        (static_cast<uint32_t>(green) << 8) |
                        (static_cast<uint32_t>(blue) << 16);

            if(base > 248 && ((fineNoise >> 5) & 0x3u) == 0u && x + 1 < PPU::SCREEN_WIDTH) {
                dstRow[x + 1] = dstRow[x];
            }
        }
    }
}

inline void GeraNESApp::render()
{
    const int modScale = (m_emu.valid() && m_modManager.active()) ? std::clamp(m_modManager.resolutionMultiplier(), 1, 8) : 1;
    const int textureWidth = 256 * modScale;
    const int textureHeight = 256 * modScale;
    const int activeTop = m_clipHeightValue;
    const int activeBottom = PPU::SCREEN_HEIGHT - m_clipHeightValue;
    const int activeTopScaled = activeTop * modScale;
    const int activeBottomScaled = activeBottom * modScale;
    const uint32_t* framebuffer = nullptr;
    const bool useModRenderPath = m_emu.valid() && m_modManager.active();
    const bool canUseSnapshotChrRenderPath = useModRenderPath;

    if(!m_emu.valid()) {
        fillNoRomStaticFramebuffer();
        framebuffer = m_framebufferUploadCopy.data();
    } else if(!useModRenderPath) {
        framebuffer = m_emu.getFramebuffer();
    }
    if(!useModRenderPath && framebuffer == nullptr) return;

    if(m_renderTextureWidth != textureWidth || m_renderTextureHeight != textureHeight) {
        m_renderTextureWidth = textureWidth;
        m_renderTextureHeight = textureHeight;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        m_updateObjectsFlag = true;
    }

    if(m_textureUploadBuffer.size() != static_cast<size_t>(textureWidth * textureHeight) ||
       m_lastTextureUploadClipHeightValue != m_clipHeightValue ||
       m_lastTextureUploadModScale != modScale) {
        m_textureUploadBuffer.assign(static_cast<size_t>(textureWidth * textureHeight), 0u);
        m_lastTextureUploadClipHeightValue = m_clipHeightValue;
        m_lastTextureUploadModScale = modScale;
    }

    auto copyScaledFramebuffer = [&](const uint32_t* sourceFramebuffer) {
        if(sourceFramebuffer == nullptr) return;
        for(int y = activeTop; y < activeBottom; ++y) {
            const uint32_t* srcRow = sourceFramebuffer + static_cast<size_t>(y) * PPU::SCREEN_WIDTH;
            for(int sy = 0; sy < modScale; ++sy) {
                uint32_t* dstRow = m_textureUploadBuffer.data() + static_cast<size_t>(y * modScale + sy) * static_cast<size_t>(textureWidth);
                if(modScale == 1) {
                    std::memcpy(dstRow, srcRow, static_cast<size_t>(PPU::SCREEN_WIDTH) * sizeof(uint32_t));
                } else {
                    for(int x = 0; x < PPU::SCREEN_WIDTH; ++x) {
                        std::fill_n(dstRow + x * modScale, modScale, srcRow[x]);
                    }
                }
            }
        }
    };

    if(canUseSnapshotChrRenderPath) {
        IEmulationHost::ModRenderSnapshot hostSnapshot;
        std::vector<uint32_t> presentedFramebufferCopy;
        if(!m_emu.getModRenderFrame(hostSnapshot, presentedFramebufferCopy) || presentedFramebufferCopy.empty()) {
            copyScaledFramebuffer(m_emu.getFramebuffer());
        } else {
            const uint32_t* presentedFramebuffer = presentedFramebufferCopy.data();
            ModManager::ChrRenderSnapshot chrSnapshot;
            chrSnapshot.scrollX = hostSnapshot.scrollX;
            chrSnapshot.scrollY = hostSnapshot.scrollY;
            chrSnapshot.universalBgColor = hostSnapshot.universalBgColor;
            chrSnapshot.paletteColors = hostSnapshot.paletteColors;
            chrSnapshot.tileHashes = hostSnapshot.tileHashes;
            chrSnapshot.backgroundPixels = std::move(hostSnapshot.backgroundPixels);
            chrSnapshot.spritePixels = std::move(hostSnapshot.spritePixels);
            m_modManager.composeChrFrame(
                m_textureUploadBuffer,
                textureWidth,
                textureHeight,
                activeTopScaled,
                activeBottomScaled,
                modScale,
                presentedFramebuffer,
                chrSnapshot
            );
        }
    } else {
        copyScaledFramebuffer(framebuffer);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_textureUploadBuffer.data());
}

inline bool GeraNESApp::paintGL()
{
    mainLoop();

    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    //ImGui::ShowDemoWindow();

    showOverlay();
    showGui();

#ifdef __EMSCRIPTEN__
    emcriptenSyncImGuiClipboardSelection();
    emcriptenSyncImGuiTextInput(ImGui::GetIO().WantTextInput);
#endif

    ImGui::Render();

    if(m_updateObjectsFlag) {
        m_updateObjectsFlag = false;
        updateBuffers();
    }

    if(
#ifdef ENABLE_NSF_PLAYER
       !m_emu.isNsfLoaded()
#else
       true
#endif
    ) {
        int drawableW = 0;
        int drawableH = 0;
        SDL_GL_GetDrawableSize(sdlWindow(), &drawableW, &drawableH);

        if(!m_shaderPasses.empty() && drawableW > 0 && drawableH > 0) {
            const size_t passCount = m_shaderPasses.size();
            const bool needsOffscreenTargets = passCount > 1;
            if(!needsOffscreenTargets || ensurePostProcessTargets(drawableW, drawableH)) {
                const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
                const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
                if(cullEnabled) glDisable(GL_CULL_FACE);
                if(blendEnabled) glDisable(GL_BLEND);
                glDisable(GL_DEPTH_TEST);

                auto drawPass = [&](ShaderPass& pass,
                                    GLuint sourceTexture,
                                    const glm::vec2& sourceSize,
                                    bool finalPass,
                                    GLuint targetFbo) {
                    if(finalPass) m_vao.bind();
                    else m_postProcessVao.bind();

                    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
                    glViewport(0, 0, drawableW, drawableH);
                    glClear(GL_COLOR_BUFFER_BIT);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, sourceTexture);

                    if(pass.program.bind()) {
                        pass.program.setUniformValue("MVPMatrix", finalPass ? m_mvp : glm::mat4(1.0f));
                        pass.program.setUniformValue("Texture", 0);
                        pass.program.setUniformValue("FrameDirection", m_emu.isRewinding() ? -1 : 1);
                        pass.program.setUniformValue("FrameCount", m_emu.frameCount());
                        pass.program.setUniformValue("OutputSize", glm::vec2(static_cast<float>(drawableW), static_cast<float>(drawableH)));
                        pass.program.setUniformValue("TextureSize", sourceSize);
                        pass.program.setUniformValue("InputSize", sourceSize);
                        for(const ShaderPass::Parameter& parameter : pass.parameters) {
                            pass.program.setUniformValue(parameter.name.c_str(), parameter.value);
                        }

                        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                        pass.program.release();
                    }

                    if(finalPass) m_vao.release();
                    else m_postProcessVao.release();
                };

                if(passCount == 1) {
                    drawPass(m_shaderPasses.front(), m_texture, glm::vec2(static_cast<float>(m_renderTextureWidth), static_cast<float>(m_renderTextureHeight)), true, 0);
                } else {
                    GLuint sourceTexture = m_texture;
                    glm::vec2 sourceSize(static_cast<float>(m_renderTextureWidth), static_cast<float>(m_renderTextureHeight));

                    for(size_t i = 0; i < passCount; ++i) {
                        const bool finalPass = i + 1 == passCount;
                        const GLuint targetFbo = finalPass ? 0 : m_postProcessTargets[i % m_postProcessTargets.size()].fbo;
                        drawPass(m_shaderPasses[i], sourceTexture, sourceSize, finalPass, targetFbo);

                        if(!finalPass) {
                            sourceTexture = m_postProcessTargets[i % m_postProcessTargets.size()].texture;
                            sourceSize = glm::vec2(static_cast<float>(drawableW), static_cast<float>(drawableH));
                        }
                    }
                }

                if(blendEnabled) glEnable(GL_BLEND);
                if(cullEnabled) glEnable(GL_CULL_FACE);
            }
        }
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#ifndef __EMSCRIPTEN__
    if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window* backupWindow = SDL_GL_GetCurrentWindow();
        SDL_GLContext backupContext = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backupWindow, backupContext);
    }
#endif
    return true;
}
