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
            const bool needsOffscreenTargets = m_shaderPasses.size() > 1;
            if(!needsOffscreenTargets || ensurePostProcessTargets(drawableW, drawableH)) {
                const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
                if(cullEnabled) glDisable(GL_CULL_FACE);

                GLuint sourceTexture = m_texture;
                glm::vec2 sourceSize(256.0f, 256.0f);

                for(size_t i = 0; i < m_shaderPasses.size(); ++i) {
                    const bool finalPass = i + 1 == m_shaderPasses.size();
                    if(finalPass) m_vao.bind();
                    else m_postProcessVao.bind();

                    if(!finalPass) {
                        glBindFramebuffer(GL_FRAMEBUFFER, m_postProcessTargets[i % m_postProcessTargets.size()].fbo);
                    } else {
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }

                    glViewport(0, 0, drawableW, drawableH);
                    glClear(GL_COLOR_BUFFER_BIT);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, sourceTexture);

                    ShaderPass& pass = m_shaderPasses[i];
                    if(pass.program.bind()) {
                        pass.program.setUniformValue("MVPMatrix", finalPass ? m_mvp : glm::mat4(1.0f));
                        pass.program.setUniformValue("Texture", 0);
                        pass.program.setUniformValue("FrameDirection", m_emu.isRewinding() ? -1 : 1);
                        pass.program.setUniformValue("FrameCount", m_emu.frameCount());
                        pass.program.setUniformValue("OutputSize", glm::vec2(static_cast<float>(drawableW), static_cast<float>(drawableH)));
                        pass.program.setUniformValue("TextureSize", sourceSize);
                        pass.program.setUniformValue("InputSize", sourceSize);

                        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                        pass.program.release();
                    }

                    if(finalPass) m_vao.release();
                    else m_postProcessVao.release();

                    if(!finalPass) {
                        sourceTexture = m_postProcessTargets[i % m_postProcessTargets.size()].texture;
                        sourceSize = glm::vec2(static_cast<float>(drawableW), static_cast<float>(drawableH));
                    }
                }

                if(cullEnabled) glEnable(GL_CULL_FACE);
            }
        }
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
