#pragma once

inline void GeraNESApp::render()
{
    m_emu.copyFramebuffer(m_framebufferUploadCopy);
    if(m_framebufferUploadCopy.size() < PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT) return;

    const int textureWidth = 256;
    const int textureHeight = 256;
    const int activeTop = m_clipHeightValue;
    const int activeBottom = PPU::SCREEN_HEIGHT - m_clipHeightValue;

    if(m_textureUploadBuffer.size() != static_cast<size_t>(textureWidth * textureHeight)) {
        m_textureUploadBuffer.assign(static_cast<size_t>(textureWidth * textureHeight), 0u);
    } else {
        std::fill(m_textureUploadBuffer.begin(), m_textureUploadBuffer.end(), 0u);
    }

    for(int y = activeTop; y < activeBottom; ++y) {
        const uint32_t* srcRow = m_framebufferUploadCopy.data() + static_cast<size_t>(y) * textureWidth;
        uint32_t* dstRow = m_textureUploadBuffer.data() + static_cast<size_t>(y) * textureWidth;
        std::memcpy(dstRow, srcRow, static_cast<size_t>(textureWidth) * sizeof(uint32_t));
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight, GL_RGBA, GL_UNSIGNED_BYTE, m_textureUploadBuffer.data());
}

inline void GeraNESApp::paintGL()
{
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

    mainLoop();

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
                    drawPass(m_shaderPasses.front(), m_texture, glm::vec2(256.0f, 256.0f), true, 0);
                } else {
                    GLuint sourceTexture = m_texture;
                    glm::vec2 sourceSize(256.0f, 256.0f);

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
}
