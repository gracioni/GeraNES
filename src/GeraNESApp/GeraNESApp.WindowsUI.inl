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
        ImGuiWindowFlags_NoNavFocus
    )) {
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 winSize = ImGui::GetWindowSize();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), IM_COL32(208, 208, 202, 255));
        drawList->AddRectFilled(ImVec2(winPos.x, winPos.y + winSize.y - 6.0f), ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), IM_COL32(185, 42, 42, 255));
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
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(controlButtonWidth, controlButtonHeight));
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
        ImGui::InvisibleButton("##ChromeDragArea", ImVec2(std::max(0.0f, winSize.x - (controlsLeft + controlsWidth + 24.0f) - buttonRowWidth - rightInset - 10.0f), titleBarHeight - 12.0f));
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
            const bool pressed = ImGui::Button(id, ImVec2(buttonWidth, buttonHeight));
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

inline void GeraNESApp::showGui()
{
    const ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();

    float lastMenuBarHeight = m_menuBarHeight;

    drawCustomWindowChrome();
    if(m_showMenuBar) menuBar();
    else m_menuBarHeight = 0;

    if(lastMenuBarHeight != m_menuBarHeight) updateBuffers();

#ifdef ENABLE_NSF_PLAYER
    if(m_emu.isNsfLoaded()) drawNsfPlayerVisualizer();
#endif

    m_inputBindingConfigWindow.update();
    m_powerPadConfigWindow.update();

    if(m_showImprovementsWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
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

            const bool netplayRewindDisabled = shouldSuppressRewindForNetplay();
            int value = netplayRewindDisabled
                ? 0
                : AppSettings::instance().data.improvements.maxRewindTime;
            ImGui::BeginDisabled(netplayRewindDisabled);
            if(ImGui::InputInt("Max Rewind Time(s)", &value)) {
                value = std::max(0, value);
                AppSettings::instance().data.improvements.maxRewindTime = value;
                m_emu.setupRewindSystem(value > 0, value);
            }
            ImGui::EndDisabled();
            if(netplayRewindDisabled) {
                ImGui::TextDisabled("Rewind is disabled while netplay is active.");
            }
        }

        ImGui::End();
    }

    if(m_showPaletteWindow) {
        drawPaletteWindow();
    }

    if(m_showShaderStackWindow) {
        drawShaderStackWindow();
    }

    if(m_showPpuViewerWindow) {
        drawPpuViewerWindow();
    } else {
        m_emu.setPpuViewerCaptureEnabled(false, false);
        if(m_ppuViewerScanlineTraceActive) {
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.enablePpuViewerScanlineTrace(false);
            });
            m_ppuViewerScanlineTraceActive = false;
            m_ppuViewerScanlineStates.clear();
            m_ppuViewerScanlineSnapshots.clear();
        }
    }

    if(m_showEventViewerWindow) {
        drawEventViewerWindow();
    } else {
        m_ppuEventViewerEnabled = false;
        m_emu.setPpuEventViewerCaptureEnabled(false);
    }

    if(m_showCpuDebuggerWindow) {
        drawCpuDebuggerWindow();
    }

    if(m_showCpuBreakpointsWindow) {
        drawCpuBreakpointsWindow();
    }

    if(m_showAboutWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
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

    if(m_showArkanoidNesConfigWindow) {
        SetNextWindowSizeClamped(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Arkanoid Controller Config (NES)", &m_showArkanoidNesConfigWindow, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("The NES Arkanoid paddle uses grabbed relative mouse movement. Press Escape to release the mouse.");
            ImGui::Separator();

            float sensitivity = AppSettings::instance().data.input.arkanoid.sensitivity;
            ImGui::SetNextItemWidth(180.0f);
            if(ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 4.0f, "%.2fx")) {
                AppSettings::instance().data.input.arkanoid.sensitivity = std::clamp(sensitivity, 0.05f, 4.0f);
            }

        }

        ImGui::End();
    }

    if(m_showArkanoidFamicomConfigWindow) {
        SetNextWindowSizeClamped(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Arkanoid Controller Config (Famicom)", &m_showArkanoidFamicomConfigWindow, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("The Famicom Arkanoid paddle uses grabbed relative mouse movement. Press Escape to release the mouse.");
            ImGui::Separator();

            float sensitivity = AppSettings::instance().data.input.arkanoid.sensitivity;
            ImGui::SetNextItemWidth(180.0f);
            if(ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 4.0f, "%.2fx")) {
                AppSettings::instance().data.input.arkanoid.sensitivity = std::clamp(sensitivity, 0.05f, 4.0f);
            }

        }

        ImGui::End();
    }

    if(m_showSnesMouseConfigWindow) {
        SetNextWindowSizeClamped(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Mouse Config", &m_showSnesMouseConfigWindow, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("The selected mouse device uses grabbed relative movement. Press Escape to release the mouse.");
            ImGui::Separator();

            float sensitivity = AppSettings::instance().data.input.snesMouse.sensitivity;
            ImGui::SetNextItemWidth(180.0f);
            if(ImGui::SliderFloat("Sensitivity", &sensitivity, 0.05f, 2.0f, "%.2fx")) {
                AppSettings::instance().data.input.snesMouse.sensitivity = std::clamp(sensitivity, 0.05f, 2.0f);
            }
        }

        ImGui::End();
    }

    if(m_showKonamiHyperShotConfigWindow) {
        m_inputBindingConfigWindow.show("Konami Hyper Shot Config", m_konamiHyperShot);
        m_showKonamiHyperShotConfigWindow = false;
    }

    if(m_showNetplayWindow) {
        GeraNESNetplay::drawNetplayWindow(m_showNetplayWindow, m_netplayRuntime, viewportCenter);
    }

    if(m_showRomDatabaseWindow) {
        SetNextWindowSizeClamped(ImVec2(720.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Rom Database");
    }

    if(ImGui::BeginPopupModal("Rom Database", &m_showRomDatabaseWindow)) {
        if(!m_romDbEditor.loaded) {
            ImGui::TextWrapped("%s", m_romDbEditor.statusMessage.c_str());
        }
        else {
                const ImVec4 statusColor = m_romDbEditor.foundInDatabase
                    ? ImGuiTheme::success()
                    : ImGuiTheme::accentActive();
                ImGui::TextColored(statusColor, "%s", m_romDbEditor.statusMessage.c_str());
                ImGui::Separator();

                const bool compareWithSaved = m_romDbEditor.foundInDatabase;
                auto isChanged = [&](const std::string& a, const std::string& b) {
                    return compareWithSaved && a != b;
                };

                auto drawStringEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<const char*, const char*>>& options) {
                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(value == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

                    if(ImGui::BeginCombo(id, options[current].second)) {
                        for(size_t i = 0; i < options.size(); ++i) {
                            const bool selected = static_cast<int>(i) == current;
                            if(ImGui::Selectable(options[i].second, selected)) {
                                value = options[i].first;
                            }
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                };

                auto drawIntEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<int, const char*>>& options) {
                    int parsed = 0;
                    try { parsed = value.empty() ? options.front().first : std::stoi(value); } catch(...) { parsed = options.front().first; }

                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(parsed == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

                    if(ImGui::BeginCombo(id, options[current].second)) {
                        for(size_t i = 0; i < options.size(); ++i) {
                            const bool selected = static_cast<int>(i) == current;
                            if(ImGui::Selectable(options[i].second, selected)) {
                                value = std::to_string(options[i].first);
                            }
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                };

                bool ch = false;
                ch = isChanged(m_romDbEditor.PrgChrCrc32, m_romDbSaved.PrgChrCrc32);
                if(ch) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                }
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("PrgChrCrc32");
                if(ch) {
                    ImGui::PopStyleColor();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText("##PrgChrCrc32", &m_romDbEditor.PrgChrCrc32, ImGuiInputTextFlags_ReadOnly);

                auto drawLabelCell = [&](const char* label, bool changed = false) {
                    ImGui::TableNextColumn();
                    if(changed) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    }
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    if(changed) {
                        ImGui::PopStyleColor();
                    }
                };
                auto drawInputTextCell = [&](const char* id, std::string& value) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText(id, &value);
                };
                auto drawStringEnumCell = [&](const char* id, std::string& value, const std::vector<std::pair<const char*, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawStringEnumCombo(id, value, options);
                };
                auto drawIntEnumCell = [&](const char* id, std::string& value, const std::vector<std::pair<int, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawIntEnumCombo(id, value, options);
                };
                auto drawEmptyPair = [&]() {
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                };

                const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;
                if(ImGui::BeginTable("RomDbFieldsTable", 4, tableFlags)) {
                    ImGui::TableSetupColumn("LabelA", ImGuiTableColumnFlags_WidthFixed, 115.0f);
                    ImGui::TableSetupColumn("ValueA", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("LabelB", ImGuiTableColumnFlags_WidthFixed, 115.0f);
                    ImGui::TableSetupColumn("ValueB", ImGuiTableColumnFlags_WidthStretch, 1.0f);

                    ImGui::TableNextRow();
                    drawLabelCell("System", isChanged(m_romDbEditor.System, m_romDbSaved.System));
                    drawStringEnumCell("##System", m_romDbEditor.System, {
                        {"", "Default"},
                        {"NesNtsc", "NesNtsc"},
                        {"NesPal", "NesPal"},
                        {"Famicom", "Famicom"},
                        {"Dendy", "Dendy"},
                        {"VsSystem", "VsSystem"},
                        {"Playchoice", "Playchoice"},
                        {"FDS", "FDS"},
                        {"FamicomNetworkSystem", "FamicomNetworkSystem"},
                    });
                    drawLabelCell("Mapper", isChanged(m_romDbEditor.Mapper, m_romDbSaved.Mapper));
                    drawInputTextCell("##Mapper", m_romDbEditor.Mapper);

                    ImGui::TableNextRow();
                    drawLabelCell("Board", isChanged(m_romDbEditor.Board, m_romDbSaved.Board));
                    drawInputTextCell("##Board", m_romDbEditor.Board);
                    drawLabelCell("SubMapperId", isChanged(m_romDbEditor.SubMapperId, m_romDbSaved.SubMapperId));
                    drawInputTextCell("##SubMapperId", m_romDbEditor.SubMapperId);

                    ImGui::TableNextRow();
                    drawLabelCell("PCB", isChanged(m_romDbEditor.PCB, m_romDbSaved.PCB));
                    drawInputTextCell("##PCB", m_romDbEditor.PCB);
                    drawLabelCell("Mirroring", isChanged(m_romDbEditor.Mirroring, m_romDbSaved.Mirroring));
                    drawStringEnumCell("##Mirroring", m_romDbEditor.Mirroring, {
                        {"", "Default"},
                        {"h", "Horizontal"},
                        {"v", "Vertical"},
                        {"4", "Four Screen"},
                        {"0", "Single Screen A"},
                        {"1", "Single Screen B"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("Chip", isChanged(m_romDbEditor.Chip, m_romDbSaved.Chip));
                    drawInputTextCell("##Chip", m_romDbEditor.Chip);
                    drawLabelCell("InputType", isChanged(m_romDbEditor.InputType, m_romDbSaved.InputType));
                    drawIntEnumCell("##InputType", m_romDbEditor.InputType, {
                        {0, "Unspecified"}, {1, "StandardControllers"}, {2, "FourScore"}, {3, "FourPlayerAdapter"},
                        {4, "VsSystem"}, {5, "VsSystemSwapped"}, {6, "VsSystemSwapAB"}, {7, "VsZapper"},
                        {8, "Zapper"}, {9, "TwoZappers"}, {10, "BandaiHypershot"}, {11, "PowerPadSideA"},
                        {12, "PowerPadSideB"}, {13, "FamilyTrainerSideA"}, {14, "FamilyTrainerSideB"},
                        {15, "ArkanoidControllerNes"}, {16, "ArkanoidControllerFamicom"}, {17, "DoubleArkanoidController"},
                        {18, "KonamiHyperShot"}, {19, "PachinkoController"}, {20, "ExcitingBoxing"},
                        {21, "JissenMahjong"}, {22, "PartyTap"}, {23, "OekaKidsTablet"}, {24, "BarcodeBattler"},
                        {25, "MiraclePiano"}, {26, "PokkunMoguraa"}, {27, "TopRider"}, {28, "DoubleFisted"},
                        {29, "Famicom3dSystem"}, {30, "DoremikkoKeyboard"}, {31, "ROB"}, {32, "FamicomDataRecorder"},
                        {33, "TurboFile"}, {34, "BattleBox"}, {35, "FamilyBasicKeyboard"}, {36, "Pec586Keyboard"},
                        {37, "Bit79Keyboard"}, {38, "SuborKeyboard"}, {39, "SuborKeyboardMouse1"},
                        {40, "SuborKeyboardMouse2"}, {41, "SnesMouse"}, {42, "GenericMulticart"},
                        {43, "SnesControllers"}, {44, "RacermateBicycle"}, {45, "UForce"}
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("PrgRomSize", isChanged(m_romDbEditor.PrgRomSize, m_romDbSaved.PrgRomSize));
                    drawInputTextCell("##PrgRomSize", m_romDbEditor.PrgRomSize);
                    drawLabelCell("VsSystemType", isChanged(m_romDbEditor.VsSystemType, m_romDbSaved.VsSystemType));
                    drawIntEnumCell("##VsSystemType", m_romDbEditor.VsSystemType, {
                        {0, "Default"},
                        {1, "RbiBaseballProtection"},
                        {2, "TkoBoxingProtection"},
                        {3, "SuperXeviousProtection"},
                        {4, "IceClimberProtection"},
                        {5, "VsDualSystem"},
                        {6, "RaidOnBungelingBayProtection"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRomSize", isChanged(m_romDbEditor.ChrRomSize, m_romDbSaved.ChrRomSize));
                    drawInputTextCell("##ChrRomSize", m_romDbEditor.ChrRomSize);
                    drawLabelCell("VsPpuModel", isChanged(m_romDbEditor.VsPpuModel, m_romDbSaved.VsPpuModel));
                    drawIntEnumCell("##VsPpuModel", m_romDbEditor.VsPpuModel, {
                        {0, "Ppu2C02"},
                        {1, "Ppu2C03"},
                        {2, "Ppu2C04A"},
                        {3, "Ppu2C04B"},
                        {4, "Ppu2C04C"},
                        {5, "Ppu2C04D"},
                        {6, "Ppu2C05A"},
                        {7, "Ppu2C05B"},
                        {8, "Ppu2C05C"},
                        {9, "Ppu2C05D"},
                        {10, "Ppu2C05E"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("SaveRamSize", isChanged(m_romDbEditor.SaveRamSize, m_romDbSaved.SaveRamSize));
                    drawInputTextCell("##SaveRamSize", m_romDbEditor.SaveRamSize);
                    drawLabelCell("BusConflicts", isChanged(m_romDbEditor.BusConflicts, m_romDbSaved.BusConflicts));
                    drawStringEnumCell("##BusConflicts", m_romDbEditor.BusConflicts, {
                        {"", "Default"},
                        {"Y", "Yes"},
                        {"N", "No"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRamSize", isChanged(m_romDbEditor.ChrRamSize, m_romDbSaved.ChrRamSize));
                    drawInputTextCell("##ChrRamSize", m_romDbEditor.ChrRamSize);
                    drawEmptyPair();

                    ImGui::TableNextRow();
                    drawLabelCell("WorkRamSize", isChanged(m_romDbEditor.WorkRamSize, m_romDbSaved.WorkRamSize));
                    drawInputTextCell("##WorkRamSize", m_romDbEditor.WorkRamSize);
                    drawLabelCell("HasBattery", isChanged(m_romDbEditor.HasBattery, m_romDbSaved.HasBattery));
                    ImGui::TableNextColumn();
                    {
                        bool hasBattery = m_romDbEditor.HasBattery == "1";
                        if(ImGui::Checkbox("##HasBattery", &hasBattery)) {
                            m_romDbEditor.HasBattery = hasBattery ? "1" : "0";
                        }
                    }

                    ImGui::EndTable();
                }

                const bool hasChanges = !compareWithSaved
                    || isChanged(m_romDbEditor.PrgChrCrc32, m_romDbSaved.PrgChrCrc32)
                    || isChanged(m_romDbEditor.System, m_romDbSaved.System)
                    || isChanged(m_romDbEditor.Board, m_romDbSaved.Board)
                    || isChanged(m_romDbEditor.PCB, m_romDbSaved.PCB)
                    || isChanged(m_romDbEditor.Chip, m_romDbSaved.Chip)
                    || isChanged(m_romDbEditor.Mapper, m_romDbSaved.Mapper)
                    || isChanged(m_romDbEditor.PrgRomSize, m_romDbSaved.PrgRomSize)
                    || isChanged(m_romDbEditor.ChrRomSize, m_romDbSaved.ChrRomSize)
                    || isChanged(m_romDbEditor.ChrRamSize, m_romDbSaved.ChrRamSize)
                    || isChanged(m_romDbEditor.WorkRamSize, m_romDbSaved.WorkRamSize)
                    || isChanged(m_romDbEditor.SaveRamSize, m_romDbSaved.SaveRamSize)
                    || isChanged(m_romDbEditor.HasBattery, m_romDbSaved.HasBattery)
                    || isChanged(m_romDbEditor.Mirroring, m_romDbSaved.Mirroring)
                    || isChanged(m_romDbEditor.InputType, m_romDbSaved.InputType)
                    || isChanged(m_romDbEditor.BusConflicts, m_romDbSaved.BusConflicts)
                    || isChanged(m_romDbEditor.SubMapperId, m_romDbSaved.SubMapperId)
                    || isChanged(m_romDbEditor.VsSystemType, m_romDbSaved.VsSystemType)
                    || isChanged(m_romDbEditor.VsPpuModel, m_romDbSaved.VsPpuModel);

                ImGui::Separator();
                const float saveButtonWidth = 120.0f;
                const float removeButtonWidth = 120.0f;

                ImGui::BeginDisabled(compareWithSaved && !hasChanges);
                if(ImGui::Button("Save", ImVec2(saveButtonWidth, 0))) {
                    ImGui::OpenPopup("Confirm Save ROM Database Entry");
                }
                ImGui::EndDisabled();

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - removeButtonWidth);
                ImGui::BeginDisabled(!m_romDbEditor.foundInDatabase);
                if(ImGui::Button("Remove", ImVec2(removeButtonWidth, 0))) {
                    ImGui::OpenPopup("Confirm Remove ROM Database Entry");
                }
                ImGui::EndDisabled();

                ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if(ImGui::BeginPopupModal("Confirm Save ROM Database Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped("Are you sure you want to save this entry to db.txt?");
                    ImGui::Spacing();
                    if(ImGui::Button("Yes", ImVec2(120, 0))) {
                        saveRomDatabaseEditor();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("No", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if(ImGui::BeginPopupModal("Confirm Remove ROM Database Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped("Are you sure you want to remove this entry from db.txt?");
                    ImGui::Spacing();
                    if(ImGui::Button("Yes", ImVec2(120, 0))) {
                        removeRomDatabaseEditor();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("No", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
        }
        ImGui::EndPopup();
    }
    if(!ImGui::IsPopupOpen("Rom Database")) {
        m_showRomDatabaseWindow = false;
    }

    if(m_showErrorWindow) {
        SetNextWindowSizeClamped(ImVec2(320.0f, 0.0f));
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
        SetNextWindowSizeClamped(ImVec2(600.0f, 0.0f), ImGuiCond_Once);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Log", &m_showLogWindow)) {
            InputTextMultilineLog("##LogOuterChild", "##LogMultilineInput", m_logBuf.data(), m_logBuf.size(), ImVec2(-1, 400));

            ImGui::Spacing();

            const char* copyBtnLabel = "Copy";
            const char* clearBtnLabel = "Clear";
            const ImGuiStyle& style = ImGui::GetStyle();
            const ImVec2 copyBtnTextSize = ImGui::CalcTextSize(copyBtnLabel);
            const ImVec2 clearBtnTextSize = ImGui::CalcTextSize(clearBtnLabel);
            const ImVec2 copyBtnSize = ImVec2(copyBtnTextSize.x + style.FramePadding.x * 2.0f,
                                              copyBtnTextSize.y + style.FramePadding.y * 2.0f);
            const ImVec2 clearBtnSize = ImVec2(clearBtnTextSize.x + style.FramePadding.x * 2.0f,
                                               clearBtnTextSize.y + style.FramePadding.y * 2.0f);

            const float buttonRowWidth = copyBtnSize.x + style.ItemSpacing.x + clearBtnSize.x;
            const float windowWidth = ImGui::GetContentRegionAvail().x;
            const float posX = (windowWidth - buttonRowWidth) * 0.5f;
            ImGui::SetCursorPosX(posX);

            if(ImGui::Button(copyBtnLabel, copyBtnSize)) {
#ifdef __EMSCRIPTEN__
                emcriptenCopyTextToClipboardExact(m_log.c_str());
#else
                ImGui::SetClipboardText(m_log.c_str());
#endif
            }
            ImGui::SameLine();
            if(ImGui::Button(clearBtnLabel, clearBtnSize)) {
                m_log.clear();
                m_logBuf.clear();
                m_logBuf.push_back('\0');
            }
        }

        ImGui::End();
    }
}

#ifdef ENABLE_NSF_PLAYER
inline void GeraNESApp::drawNsfPlayerVisualizer()
{
    const SDL_Rect clientArea = emulatorClientArea();
    m_nsfVisualizer.draw(
        m_audioOutput.getRecentMixedSamples(),
        m_audioOutput.outputSampleRate(),
        clientArea.y,
        width(),
        height(),
        m_emu.nsfCurrentSong(),
        m_emu.nsfTotalSongs(),
        m_emu.nsfIsPlaying(),
        m_emu.nsfIsPaused(),
        m_emu.nsfHasEnded(),
        m_lastMainLoopDtMs,
        m_fontNsfTitle,
        m_fontNsfSubtitle
    );
}
#endif

inline void DrawCpuBreakpointHitSummary(const GeraNESEmu::DebugBreakpointHit& hit)
{
    if(!hit.valid) {
        ImGui::TextDisabled("No breakpoint hit yet.");
        return;
    }

    ImGui::TextColored(ImGuiTheme::accent(), "%s", hit.reason.c_str());
    if(hit.hasAddress) {
        ImGui::Text(
            "%s $%04X = %02X  Frame %u  CPU %u  PPU %d,%d",
            hit.isWrite ? "Write" : "Read",
            hit.address,
            hit.value,
            hit.frame,
            hit.cpuCycle,
            hit.ppuScanline,
            hit.ppuCycle
        );
    } else {
        ImGui::Text(
            "Frame %u  CPU %u  PPU %d,%d",
            hit.frame,
            hit.cpuCycle,
            hit.ppuScanline,
            hit.ppuCycle
        );
    }
}

inline void GeraNESApp::drawCpuDebuggerWindow()
{
    SetNextWindowSizeClamped(ImVec2(860.0f, 620.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(!ImGui::Begin("CPU Debugger", &m_showCpuDebuggerWindow)) {
        AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;

    const bool hasRomLoaded = m_emu.valid();
    const auto netplaySnapshot = m_netplayRuntime.uiSnapshot();
    const bool debugBlockedByNetplay =
        netplaySnapshot.active ||
        netplaySnapshot.hosting ||
        netplaySnapshot.connected ||
        netplaySnapshot.reconnecting;

    if(!hasRomLoaded) {
        ImGui::TextDisabled("Load a ROM to inspect CPU state.");
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    if(debugBlockedByNetplay) {
        ImGui::TextDisabled("Opening the CPU debugger disconnects the current netplay session.");
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    if(!AppSettings::instance().data.debug.cpuDebuggerEnabled) {
        requestEnableCpuDebugger();
    }

    if(!AppSettings::instance().data.debug.cpuDebuggerEnabled) {
        ImGui::TextDisabled("Waiting for CPU debugger to become available.");
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    const bool paused = m_emu.paused();
    if(paused) {
        if(ImGui::Button("Resume")) {
            m_cpuDebuggerAutoPaused = false;
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.clearDebugBreakpointHit();
                emu.setPaused(false);
            });
        }
    } else {
        if(ImGui::Button("Pause")) {
            m_cpuDebuggerAutoPaused = false;
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.setPaused(true);
            });
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    if(ImGui::Button("Step")) {
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.clearDebugBreakpointHit();
            emu.debugStepInstruction();
        });
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled(paused ? "Stopped" : "Running");

    ImGui::SameLine();
    if(ImGui::Button("Breakpoints")) {
        m_showCpuBreakpointsWindow = true;
        AppSettings::instance().data.debug.showCpuBreakpoints = true;
    }

    CPU2A03::DebugState cpuState;
    GeraNESEmu::ExecutionPoint execPoint;
    GeraNESEmu::DebugBreakpointHit breakpointHit;
    std::vector<CPU2A03DebugLine> disassembly;
    m_emu.withExclusiveAccess([&](auto& emu) {
        cpuState = emu.getConsole().cpu().debugState();
        execPoint = emu.executionPoint();
        breakpointHit = emu.debugBreakpointHit();
        disassembly = CPU2A03Debug::disassembleAround(
            cpuState.pc,
            8,
            24,
            [&](uint16_t addr) {
                return emu.debugPeekCpuMemory(addr);
            }
        );
    });

    ImGui::Separator();
    DrawCpuBreakpointHitSummary(breakpointHit);

    if(breakpointHit.valid) {
        ImGui::SameLine();
        if(ImGui::Button("Clear Hit")) {
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.clearDebugBreakpointHit();
            });
        }
    }

    ImGui::Separator();

    if(ImGui::BeginTable("CpuRegisters", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("PC  %04X", cpuState.pc);
        ImGui::TableNextColumn(); ImGui::Text("A   %02X", cpuState.a);
        ImGui::TableNextColumn(); ImGui::Text("X   %02X", cpuState.x);
        ImGui::TableNextColumn(); ImGui::Text("Y   %02X", cpuState.y);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("SP  %02X", cpuState.sp);
        ImGui::TableNextColumn(); ImGui::Text("P   %02X", cpuState.status);
        ImGui::TableNextColumn(); ImGui::Text("Flags  %s", CPU2A03Debug::formatStatus(cpuState.status).c_str());
        ImGui::TableNextColumn(); ImGui::Text("Frame  %u", execPoint.frame);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("CPU Cycles  %u", cpuState.cycleCounter);
        ImGui::TableNextColumn(); ImGui::Text("Tick  %llu", static_cast<unsigned long long>(execPoint.emulationTick));
        ImGui::TableNextColumn(); ImGui::Text("Pending Cycles  %u", execPoint.cpuCyclesRemaining);
        ImGui::TableNextColumn(); ImGui::Text("Last Opcode  %02X", cpuState.opcode);
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Disassembly");

    if(ImGui::BeginChild("CpuDisassembly", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        for(const CPU2A03DebugLine& line : disassembly) {
            if(line.isCurrent) {
                ImGui::TextColored(
                    ImGuiTheme::accent(),
                    "> %04X  %-8s  %s",
                    line.address,
                    line.bytes.c_str(),
                    line.mnemonic.c_str()
                );
            } else {
                ImGui::Text(
                    "  %04X  %-8s  %s",
                    line.address,
                    line.bytes.c_str(),
                    line.mnemonic.c_str()
                );
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();

    if(!m_showCpuDebuggerWindow) {
        disableCpuDebugging();
    }
}

inline void GeraNESApp::drawCpuBreakpointsWindow()
{
    SetNextWindowSizeClamped(ImVec2(620.0f, 620.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(!ImGui::Begin("CPU Breakpoints", &m_showCpuBreakpointsWindow)) {
        AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;
        ImGui::End();
        return;
    }

    AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;

    const bool hasRomLoaded = m_emu.valid();
    const auto netplaySnapshot = m_netplayRuntime.uiSnapshot();
    const bool debugBlockedByNetplay =
        netplaySnapshot.active ||
        netplaySnapshot.hosting ||
        netplaySnapshot.connected ||
        netplaySnapshot.reconnecting;
    const bool debugEnabled = AppSettings::instance().data.debug.cpuDebuggerEnabled;

    GeraNESEmu::DebugBreakpointConfig config;
    GeraNESEmu::DebugBreakpointHit hit;
    m_emu.withExclusiveAccess([&](auto& emu) {
        config = emu.debugBreakpointConfig();
        hit = emu.debugBreakpointHit();
    });

    if(!hasRomLoaded) {
        ImGui::TextDisabled("Load a ROM to configure CPU breakpoints.");
        ImGui::End();
        return;
    }

    if(debugBlockedByNetplay) {
        ImGui::TextDisabled("CPU breakpoints are disabled while netplay is active.");
        ImGui::End();
        return;
    }

    if(!debugEnabled) {
        ImGui::TextDisabled("Enable the CPU debugger first. Breakpoints only arm while debug mode is enabled.");
        ImGui::End();
        return;
    }

    bool configChanged = false;

    configChanged |= ImGui::Checkbox("Enable event breakpoints", &config.enabled);
    ImGui::Separator();
    ImGui::TextUnformatted("Events");
    configChanged |= ImGui::Checkbox("PPU NMI start", &config.breakOnNmiStart);
    configChanged |= ImGui::Checkbox("PPU NMI end", &config.breakOnNmiEnd);
    configChanged |= ImGui::Checkbox("IRQ start", &config.breakOnIrqStart);
    configChanged |= ImGui::Checkbox("IRQ end", &config.breakOnIrqEnd);
    configChanged |= ImGui::Checkbox("Sprite zero hit", &config.breakOnSpriteZeroHit);
    configChanged |= ImGui::Checkbox("OAM DMA start ($4014)", &config.breakOnOamDmaStart);
    configChanged |= ImGui::Checkbox("DMC DMA start", &config.breakOnDmcDmaStart);

    ImGui::Separator();
    ImGui::TextUnformatted("Register Access");
    configChanged |= ImGui::Checkbox("PPU register writes", &config.breakOnPpuRegisterWrite);
    configChanged |= ImGui::Checkbox("PPU register reads", &config.breakOnPpuRegisterRead);
    configChanged |= ImGui::Checkbox("APU register writes", &config.breakOnApuRegisterWrite);
    configChanged |= ImGui::Checkbox("APU register reads", &config.breakOnApuRegisterRead);
    configChanged |= ImGui::Checkbox("Controller reads", &config.breakOnControllerRead);
    configChanged |= ImGui::Checkbox("Controller writes", &config.breakOnControllerWrite);
    configChanged |= ImGui::Checkbox("Mapper register writes", &config.breakOnMapperRegisterWrite);
    configChanged |= ImGui::Checkbox("Mapper register reads", &config.breakOnMapperRegisterRead);

    ImGui::Separator();
    ImGui::TextUnformatted("Exact CPU Watch");
    configChanged |= ImGui::Checkbox("Break on CPU read", &config.breakOnExactCpuRead);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    configChanged |= ImGui::InputScalar("##ExactCpuReadAddress", ImGuiDataType_U16, &config.exactCpuReadAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
    configChanged |= ImGui::Checkbox("Break on CPU write", &config.breakOnExactCpuWrite);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    configChanged |= ImGui::InputScalar("##ExactCpuWriteAddress", ImGuiDataType_U16, &config.exactCpuWriteAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);

    if(configChanged) {
        m_emu.withExclusiveAccess([&](auto& emu) {
            emu.setDebugBreakpointConfig(config);
        });
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Last Hit");
    DrawCpuBreakpointHitSummary(hit);
    if(hit.valid && ImGui::Button("Clear Breakpoint Hit")) {
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.clearDebugBreakpointHit();
        });
    }

    ImGui::End();
}

inline void GeraNESApp::showOverlay()
{
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(mainViewport);
    const ImVec2 overlayOrigin = mainViewport != nullptr ? mainViewport->Pos : ImVec2(0.0f, 0.0f);

    if(AppSettings::instance().data.debug.showFps) {
        const int fontSize = 32;
        ImFont* fpsFont = m_fontFps != nullptr ? m_fontFps : ImGui::GetFont();
        const SDL_Rect clientArea = emulatorClientArea();

        std::string fpsText = std::to_string(m_fps);
        ImVec2 fpsTextSize = fpsFont->CalcTextSizeA(fontSize, FLT_MAX, 0, fpsText.c_str());

        const ImVec2 pos = ImVec2(
            overlayOrigin.x + width() - fpsTextSize.x - 32,
            overlayOrigin.y + static_cast<float>(clientArea.y) + 8.0f
        );

        DrawTextOutlined(drawList, fpsFont, fontSize, pos, 0xFFFFFFFF, 0xFF000000, fpsText.c_str());
    }
    m_userToast.draw(drawList, overlayOrigin, static_cast<float>(width()), static_cast<float>(height()), m_fontToast);

    m_touch->draw(drawList, overlayOrigin);
}
