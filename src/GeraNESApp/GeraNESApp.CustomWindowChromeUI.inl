#pragma once

inline void GeraNESApp::drawCustomWindowChrome()
{
    if(!useCustomWindowChrome()) return;

    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    const float titleBarHeight = customTitleBarHeight();
    const float buttonWidth = 34.0f;
    const float buttonHeight = 24.0f;
    const float buttonSpacing = 8.0f;
    const float rightInset = 12.0f;
    const float buttonRowWidth = buttonWidth * 3.0f + buttonSpacing * 2.0f;
    const float ledSize = 12.0f;
    const float controlButtonWidth = 108.0f;
    const float controlButtonHeight = 26.0f;
    const float controlSpacing = 12.0f;
    const float controlsLeft = 18.0f;
    const float controlsTop = 9.0f;
    const float controlsWidth = ledSize + 14.0f + controlButtonWidth * 2.0f + controlSpacing;

    ImGui::SetNextWindowViewport(mainViewport->ID);
    ImGui::SetNextWindowPos(mainViewport->Pos);
    ImGui::SetNextWindowSize(ImVec2(mainViewport->Size.x, titleBarHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGuiTheme::windowBg());

    if(ImGui::Begin(
        "##GeraNESCustomChrome",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav
    )) {
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 winSize = ImGui::GetWindowSize();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), IM_COL32(208, 208, 202, 255));
        const ImVec2 ledMin(winPos.x + controlsLeft, winPos.y + controlsTop + 7.0f);
        const ImVec2 ledMax(ledMin.x + ledSize, ledMin.y + ledSize);
        drawList->AddRectFilled(ledMin, ledMax, IM_COL32(78, 220, 255, 255), 2.0f);
        drawList->AddRect(ledMin, ledMax, IM_COL32(170, 245, 255, 220), 2.0f);
        drawList->AddRectFilledMultiColor(
            ImVec2(ledMin.x + 2.0f, ledMin.y + 2.0f),
            ImVec2(ledMax.x - 2.0f, ledMax.y - 2.0f),
            IM_COL32(220, 250, 255, 255),
            IM_COL32(92, 215, 255, 255),
            IM_COL32(40, 150, 235, 255),
            IM_COL32(145, 235, 255, 255)
        );

        auto drawChromeControlButton = [&](const ImVec2& min, const char* id, const char* label, bool enabled) -> bool {
            const ImVec2 max(min.x + controlButtonWidth, min.y + controlButtonHeight);
            const ImU32 outerColor = enabled ? IM_COL32(138, 138, 132, 255) : IM_COL32(154, 154, 150, 255);
            const ImU32 innerColor = enabled ? IM_COL32(112, 112, 108, 255) : IM_COL32(132, 132, 128, 255);
            const ImU32 highlightColor = enabled ? IM_COL32(182, 182, 176, 255) : IM_COL32(194, 194, 190, 220);
            const ImU32 shadowColor = enabled ? IM_COL32(58, 58, 56, 255) : IM_COL32(104, 104, 100, 220);
            const ImU32 textColor = enabled ? IM_COL32(165, 28, 28, 255) : IM_COL32(126, 126, 124, 255);

            drawList->AddRectFilled(min, max, outerColor, 2.0f);
            drawList->AddRectFilled(ImVec2(min.x + 2.0f, min.y + 2.0f), ImVec2(max.x - 2.0f, max.y - 2.0f), innerColor, 2.0f);
            drawList->AddLine(ImVec2(min.x + 3.0f, min.y + 3.0f), ImVec2(max.x - 3.0f, min.y + 3.0f), highlightColor, 1.0f);
            drawList->AddLine(ImVec2(min.x + 3.0f, min.y + 3.0f), ImVec2(min.x + 3.0f, max.y - 3.0f), highlightColor, 1.0f);
            drawList->AddLine(ImVec2(min.x + 2.0f, max.y - 3.0f), ImVec2(max.x - 2.0f, max.y - 3.0f), shadowColor, 1.0f);
            drawList->AddLine(ImVec2(max.x - 3.0f, min.y + 2.0f), ImVec2(max.x - 3.0f, max.y - 3.0f), shadowColor, 1.0f);

            ImGui::SetCursorScreenPos(min);
            if(!enabled) {
                ImGui::BeginDisabled();
            }
            ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
            ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
            ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(controlButtonWidth, controlButtonHeight));
            ImGui::PopItemFlag();
            ImGui::PopItemFlag();
            ImGui::PopItemFlag();
            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();
            if(!enabled) {
                ImGui::EndDisabled();
            }
            if(hovered || active) {
                const ImU32 overlay = active ? IM_COL32(255, 255, 255, 26) : IM_COL32(255, 255, 255, 14);
                drawList->AddRectFilled(ImVec2(min.x + 2.0f, min.y + 2.0f), ImVec2(max.x - 2.0f, max.y - 2.0f), overlay, 2.0f);
            }

            const ImVec2 textSize = ImGui::CalcTextSize(label);
            drawList->AddText(
                ImVec2(min.x + (controlButtonWidth - textSize.x) * 0.5f, min.y + (controlButtonHeight - textSize.y) * 0.5f),
                textColor,
                label
            );
            return pressed;
        };

        const ImVec2 powerButtonMin(winPos.x + controlsLeft + ledSize + 14.0f, winPos.y + controlsTop);
        const ImVec2 resetButtonMin(powerButtonMin.x + controlButtonWidth + controlSpacing, powerButtonMin.y);
        const bool powerEnabled = true;
        const bool resetEnabled = m_emu.valid() && !isNetplayClientRestricted();

        if(drawChromeControlButton(powerButtonMin, "##ChromePower", "POWER", powerEnabled)) {
            quit();
        }
        if(drawChromeControlButton(resetButtonMin, "##ChromeReset", "RESET", resetEnabled)) {
            resetAction();
        }

        const std::string chromeTitle = title().empty() ? std::string("GeraNES") : title();
        const ImVec2 titleTextSize = ImGui::CalcTextSize(chromeTitle.c_str());
        const ImVec2 titlePos(
            resetButtonMin.x + controlButtonWidth + 22.0f,
            winPos.y + (titleBarHeight - titleTextSize.y) * 0.5f - 1.0f
        );
        const ImU32 titleColor = ImGuiTheme::toU32(ImGuiTheme::accentActive());
        drawList->AddText(ImVec2(titlePos.x, titlePos.y), titleColor, chromeTitle.c_str());

        ImGui::SetCursorScreenPos(ImVec2(winPos.x + controlsLeft + controlsWidth + 24.0f, winPos.y + 6.0f));
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
        ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
        ImGui::InvisibleButton("##ChromeDragArea", ImVec2(std::max(0.0f, winSize.x - (controlsLeft + controlsWidth + 24.0f) - buttonRowWidth - rightInset - 10.0f), titleBarHeight - 12.0f));
        ImGui::PopItemFlag();
        ImGui::PopItemFlag();
        ImGui::PopItemFlag();
        const bool dragAreaHovered = ImGui::IsItemHovered();
        const bool dragAreaActive = ImGui::IsItemActive();
        if(dragAreaHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if(isMaximized()) restoreWindow();
            else maximizeWindow();
        }
        if(dragAreaActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !isMaximized()) {
            int mouseX = 0;
            int mouseY = 0;
            SDL_GetGlobalMouseState(&mouseX, &mouseY);
            if(!m_customChromeDragging) {
                m_customChromeDragging = true;
                m_customChromeDragStartMouseX = mouseX;
                m_customChromeDragStartMouseY = mouseY;
                SDL_GetWindowPosition(sdlWindow(), &m_customChromeDragStartWindowX, &m_customChromeDragStartWindowY);
            }
            SDL_SetWindowPosition(
                sdlWindow(),
                m_customChromeDragStartWindowX + (mouseX - m_customChromeDragStartMouseX),
                m_customChromeDragStartWindowY + (mouseY - m_customChromeDragStartMouseY)
            );
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_customChromeDragging = false;
        }

        ImGui::SetCursorScreenPos(ImVec2(winPos.x + winSize.x - buttonRowWidth - rightInset, winPos.y + 10.0f));
        enum class ChromeCaptionButtonIcon {
            Minimize,
            Maximize,
            Restore,
            Close
        };
        auto chromeButton = [&](const char* id, ChromeCaptionButtonIcon icon, const ImVec4& color) -> bool {
            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + 0.08f, color.y + 0.08f, color.z + 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(std::max(0.0f, color.x - 0.06f), std::max(0.0f, color.y - 0.06f), std::max(0.0f, color.z - 0.06f), 1.0f));
            ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
            ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
            ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
            const bool pressed = ImGui::Button(id, ImVec2(buttonWidth, buttonHeight));
            ImGui::PopItemFlag();
            ImGui::PopItemFlag();
            ImGui::PopItemFlag();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const ImU32 iconColor = IM_COL32(248, 248, 248, 255);
            const float stroke = 1.2f;
            const ImVec2 center(min.x + buttonWidth * 0.5f, min.y + buttonHeight * 0.5f);

            switch(icon) {
                case ChromeCaptionButtonIcon::Minimize:
                    drawList->AddLine(
                        ImVec2(center.x - 5.0f, center.y + 4.0f),
                        ImVec2(center.x + 5.0f, center.y + 4.0f),
                        iconColor,
                        stroke
                    );
                    break;
                case ChromeCaptionButtonIcon::Maximize:
                    drawList->AddRect(
                        ImVec2(center.x - 5.0f, center.y - 5.0f),
                        ImVec2(center.x + 5.0f, center.y + 5.0f),
                        iconColor,
                        0.0f,
                        0,
                        stroke
                    );
                    break;
                case ChromeCaptionButtonIcon::Restore:
                    drawList->AddRect(
                        ImVec2(center.x - 4.0f, center.y - 2.0f),
                        ImVec2(center.x + 2.0f, center.y + 4.0f),
                        iconColor,
                        0.0f,
                        0,
                        stroke
                    );
                    drawList->AddLine(
                        ImVec2(center.x - 1.0f, center.y - 5.0f),
                        ImVec2(center.x + 4.0f, center.y - 5.0f),
                        iconColor,
                        stroke
                    );
                    drawList->AddLine(
                        ImVec2(center.x + 4.0f, center.y - 5.0f),
                        ImVec2(center.x + 4.0f, center.y + 0.0f),
                        iconColor,
                        stroke
                    );
                    break;
                case ChromeCaptionButtonIcon::Close:
                    drawList->AddLine(
                        ImVec2(center.x - 4.5f, center.y - 4.5f),
                        ImVec2(center.x + 4.5f, center.y + 4.5f),
                        iconColor,
                        stroke
                    );
                    drawList->AddLine(
                        ImVec2(center.x + 4.5f, center.y - 4.5f),
                        ImVec2(center.x - 4.5f, center.y + 4.5f),
                        iconColor,
                        stroke
                    );
                    break;
            }
            drawList->AddRect(min, max, IM_COL32(35, 35, 35, 160), 4.0f);
            ImGui::PopStyleColor(3);
            return pressed;
        };

        if(chromeButton("##ChromeMinimize", ChromeCaptionButtonIcon::Minimize, ImGuiTheme::chromeButton())) {
            minimizeWindow();
        }
        ImGui::SameLine(0.0f, buttonSpacing);
        if(chromeButton("##ChromeMaximize", isMaximized() ? ChromeCaptionButtonIcon::Restore : ChromeCaptionButtonIcon::Maximize, ImGuiTheme::chromeButtonAlt())) {
            if(isMaximized()) restoreWindow();
            else maximizeWindow();
        }
        ImGui::SameLine(0.0f, buttonSpacing);
        if(chromeButton("##ChromeClose", ChromeCaptionButtonIcon::Close, ImGuiTheme::chromeCloseButton())) {
            quit();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
