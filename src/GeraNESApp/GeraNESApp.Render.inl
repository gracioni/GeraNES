#pragma once

inline void GeraNESApp::render()
{
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, m_clipHeightValue, 256, 240 - 2 * m_clipHeightValue, GL_RGBA, GL_UNSIGNED_BYTE, m_emu.getFramebuffer() + m_clipHeightValue * 256);
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

    ImGui::Render();

    mainLoop();

    if(m_updateObjectsFlag) {
        m_updateObjectsFlag = false;
        updateBuffers();
    }

    m_vao.bind();

    if(!m_emu.isNsfLoaded()) {
        glBindTexture(GL_TEXTURE_2D, m_texture);

        if(m_shaderProgram.bind()) {
            m_shaderProgram.setUniformValue("MVPMatrix", m_mvp);
            m_shaderProgram.setUniformValue("Texture", 0);

            m_shaderProgram.setUniformValue("FrameDirection", m_emu.isRewinding() ? -1 : 1);
            m_shaderProgram.setUniformValue("FrameCount", m_emu.frameCount());
            m_shaderProgram.setUniformValue("OutputSize", glm::vec2((float)width(), (float)height()));
            m_shaderProgram.setUniformValue("TextureSize", glm::vec2(256, 256));
            m_shaderProgram.setUniformValue("InputSize", glm::vec2(256, 256));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            m_shaderProgram.release();
        }
    }

    m_vao.release();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
