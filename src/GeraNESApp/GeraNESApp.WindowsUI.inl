#pragma once

inline void GeraNESApp::showGui()
{
    const ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();

    float lastMenuBarHeight = m_menuBarHeight;

    if(m_showMenuBar) menuBar();
    else m_menuBarHeight = 0;

    if(lastMenuBarHeight != m_menuBarHeight) updateBuffers();

    m_controllerConfigWindow.update();

    if(m_showImprovementsWindow) {
        ImGui::SetNextWindowSize(ImVec2(320, 0));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Improvements", &m_showImprovementsWindow, ImGuiWindowFlags_NoResize)) {
            bool disableSpritesLimit = m_emu.spriteLimitDisabled();
            if(ImGui::Checkbox("Disable Sprites Limit", &disableSpritesLimit)) {
                m_emu.disableSpriteLimit(disableSpritesLimit);
            }
            AppSettings::instance().data.improvements.disableSpritesLimit = m_emu.spriteLimitDisabled();

            bool overclock = m_emu.overclocked();
            if(ImGui::Checkbox("Overclock", &overclock)) {
                m_emu.enableOverclock(overclock);
            }
            AppSettings::instance().data.improvements.overclock = m_emu.overclocked();

            ImGui::SetNextItemWidth(100);

            int value = AppSettings::instance().data.improvements.maxRewindTime;
            if(ImGui::InputInt("Max Rewind Time(s)", &value)) {
                value = std::max(0, value);
                AppSettings::instance().data.improvements.maxRewindTime = value;
                m_emu.setupRewindSystem(value > 0, value);
            }
        }

        ImGui::End();
    }

    if(m_showAboutWindow) {
        ImGui::SetNextWindowSize(ImVec2(320, 0));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("About", &m_showAboutWindow, ImGuiWindowFlags_NoResize)) {
            std::string txt = std::string(GERANES_NAME) + " " + GERANES_VERSION;

            TextCenteredWrapped(txt);

            txt = std::string("Racionisoft 2015 - ") + std::to_string(compileTimeYear());

            ImGui::NewLine();
            TextCenteredWrapped(txt);

            ImGui::NewLine();
            ImGui::NewLine();

            txt = "geraldoracioni@gmail.com";
            TextCenteredWrapped(txt);
        }

        ImGui::End();
    }

    if(m_showErrorWindow) {
        ImGui::SetNextWindowSize(ImVec2(320, 0));
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        bool lastState = m_showErrorWindow;

        if(ImGui::Begin("Error", &m_showErrorWindow)) {
            float windowWidth = ImGui::GetContentRegionAvail().x;

            TextCenteredWrapped(m_errorMessage.c_str());

            ImGui::Spacing();
            ImGui::Spacing();

            const char* btnLabel = "OK";

            ImVec2 btnSize = ImGui::CalcTextSize(btnLabel);
            btnSize.x += ImGui::GetStyle().FramePadding.x * 2.0f;
            btnSize.y += ImGui::GetStyle().FramePadding.y * 2.0f;

            float posX = (windowWidth - btnSize.x) * 0.5f;
            ImGui::SetCursorPosX(posX);

            if(ImGui::Button(btnLabel, btnSize)) {
                m_showErrorWindow = false;
            }
        }
        ImGui::End();

        if(lastState && !m_showErrorWindow) m_showErrorWindow = false;
    }

    if(m_showLogWindow) {
        ImGui::SetNextWindowSize(ImVec2(600, 0), ImGuiCond_Once);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Log", &m_showLogWindow)) {
            constexpr const char* kLogTextId = "##LogMultilineInput";
            ImGui::InputTextMultiline(kLogTextId, m_logBuf.data(), m_logBuf.size(),
                    ImVec2(-1, 400), ImGuiInputTextFlags_ReadOnly);

            if(!(ImGui::IsItemActive() || ImGui::IsItemEdited())) {
                ImGuiContext& g = *GImGui;
                const char* child_window_name = NULL;
                ImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g.CurrentWindow->Name, kLogTextId, ImGui::GetID(kLogTextId));
                ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);

                if(child_window) {
                    ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
                }
            }

            ImGui::Spacing();

            const char* btnLabel = "Clear";
            ImVec2 btnTextSize = ImGui::CalcTextSize(btnLabel);
            ImVec2 btnSize = ImVec2(btnTextSize.x + ImGui::GetStyle().FramePadding.x * 2.0f,
                                    btnTextSize.y + ImGui::GetStyle().FramePadding.y * 2.0f);

            float windowWidth = ImGui::GetContentRegionAvail().x;
            float posX = (windowWidth - btnSize.x) * 0.5f;
            ImGui::SetCursorPosX(posX);

            if(ImGui::Button(btnLabel, btnSize)) {
                m_log.clear();
                m_logBuf.clear();
                m_logBuf.push_back('\0');
            }
        }

        ImGui::End();
    }
}

inline void GeraNESApp::showOverlay()
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if(AppSettings::instance().data.debug.showFps) {
        const int fontSize = 32;

        std::string fpsText = std::to_string(m_fps);
        ImVec2 fpsTextSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, fpsText.c_str());

        const ImVec2 pos = ImVec2(width() - fpsTextSize.x - 32, 40);

        DrawTextOutlined(drawList, nullptr, fontSize, pos, 0xFFFFFFFF, 0xFF000000, fpsText.c_str());
    }

    m_touch->draw(drawList);
}
