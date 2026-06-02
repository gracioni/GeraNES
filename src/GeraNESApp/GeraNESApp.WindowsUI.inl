#pragma once

#include "GeraNESApp/GeraNESApp.CustomWindowChromeUI.inl"
#include "GeraNESApp/GeraNESApp.NsfPlayerWindowUI.inl"
#include "GeraNESApp/GeraNESApp.NsfPlayerVisualizerUI.inl"
#include "GeraNESApp/GeraNESApp.CpuDebugSharedUI.inl"
#include "GeraNESApp/GeraNESApp.ImprovementsWindowUI.inl"
#include "GeraNESApp/GeraNESApp.AboutWindowUI.inl"
#include "GeraNESApp/GeraNESApp.ReplayWindowUI.inl"
#include "GeraNESApp/GeraNESApp.ArkanoidNesConfigWindowUI.inl"
#include "GeraNESApp/GeraNESApp.ArkanoidFamicomConfigWindowUI.inl"
#include "GeraNESApp/GeraNESApp.SnesMouseConfigWindowUI.inl"
#include "GeraNESApp/GeraNESApp.RomDatabaseSaveConfirmPopupUI.inl"
#include "GeraNESApp/GeraNESApp.RomDatabaseRemoveConfirmPopupUI.inl"
#include "GeraNESApp/GeraNESApp.RomDatabaseWindowUI.inl"
#include "GeraNESApp/GeraNESApp.ErrorWindowUI.inl"
#include "GeraNESApp/GeraNESApp.LogWindowUI.inl"
#include "GeraNESApp/GeraNESApp.MemoryViewerEditPopupUI.inl"
#include "GeraNESApp/GeraNESApp.MemoryViewerWindowUI.inl"
#include "GeraNESApp/GeraNESApp.MemoryCompareWindowUI.inl"
#include "GeraNESApp/GeraNESApp.CpuDebuggerWindowUI.inl"
#include "GeraNESApp/GeraNESApp.CpuBreakpointsWindowUI.inl"

inline void GeraNESApp::showGui()
{
    float lastMenuBarHeight = m_menuBarHeight;

    drawCustomWindowChrome();
    if(m_showMenuBar) menuBar();
    else m_menuBarHeight = 0;

    if(lastMenuBarHeight != m_menuBarHeight) updateBuffers();
    m_imGuiWindowFocusBlocksEmulator = false;

#ifdef ENABLE_NSF_PLAYER
    if(m_emu.isNsfLoaded()) {
        drawNsfPlayerVisualizer();
        drawNsfPlayerWindow();
    }
#endif

    m_inputBindingConfigWindow.update();
    m_powerPadConfigWindow.update();

    if(m_showImprovementsWindow) {
        drawImprovementsWindow();
    }

    if(m_showPaletteWindow) {
        drawPaletteWindow();
    }

    if(m_showReplayWindow) {
        drawReplayWindow();
    }

    if(m_showOverscanWindow) {
        drawOverscanWindow();
    }

    if(m_showShaderWindow) {
        drawShaderWindow();
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

    const bool shouldEnableModPixelInspectorPpuCapture =
        m_emu.valid() && (m_showModPixelInspectorWindow || m_modManager.active());
    if(shouldEnableModPixelInspectorPpuCapture != m_modPixelInspectorPpuCaptureEnabled) {
        m_emu.withExclusiveAccess([shouldEnableModPixelInspectorPpuCapture](auto& emu) {
            emu.getConsole().ppu().debugSetModRenderCaptureEnabled(shouldEnableModPixelInspectorPpuCapture);
        });
        m_modPixelInspectorPpuCaptureEnabled = shouldEnableModPixelInspectorPpuCapture;
    }

    if(m_showModPixelInspectorWindow) {
        drawModPixelInspectorWindow();
    }

    if(m_showEventViewerWindow) {
        drawEventViewerWindow();
    } else {
        m_ppuEventViewerEnabled = false;
        m_emu.setPpuEventViewerCaptureEnabled(false);
    }

    if(m_showMemoryViewerWindow) {
        drawMemoryViewerWindow();
    }

    if(m_showMemoryCompareWindow) {
        drawMemoryCompareWindow();
    }

    if(m_showCpuDebuggerWindow) {
        drawCpuDebuggerWindow();
    }

    if(m_showCpuBreakpointsWindow) {
        drawCpuBreakpointsWindow();
    } else {
        m_cpuBreakpointsFocused = false;
    }

    if(m_showAboutWindow) {
        drawAboutWindow();
    }

    if(m_showArkanoidNesConfigWindow) {
        drawArkanoidNesConfigWindow();
    }

    if(m_showArkanoidFamicomConfigWindow) {
        drawArkanoidFamicomConfigWindow();
    }

    if(m_showSnesMouseConfigWindow) {
        drawSnesMouseConfigWindow();
    }

    if(m_showKonamiHyperShotConfigWindow) {
        m_inputBindingConfigWindow.show("Konami Hyper Shot Config", m_konamiHyperShot);
        m_showKonamiHyperShotConfigWindow = false;
    }

    if(m_showNetplayWindow) {
        GeraNESNetplay::drawNetplayWindow(m_showNetplayWindow, m_netplayRuntime, ImGui::GetMainViewport()->GetCenter());
    }

    drawRomDatabaseWindow();

    if(m_showErrorWindow) {
        drawErrorWindow();
    }

    if(m_showLogWindow) {
        drawLogWindow();
    }

    if(m_pendingRomLoad.active) {
        drawPendingRomLoadOverlay();
        m_imGuiWindowFocusBlocksEmulator = true;
    }

    if(m_snesMouseGrabActive || m_arkanoidGrabActive) {
        m_imGuiWindowFocusBlocksEmulator = false;
    }
}

inline void GeraNESApp::showOverlay()
{
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(mainViewport);
    const ImVec2 overlayOrigin = mainViewport != nullptr ? mainViewport->Pos : ImVec2(0.0f, 0.0f);
    const auto replayPlaybackStatus = m_emu.replayPlaybackStatus();

    if(replayPlaybackStatus.seeking) {
        const SDL_Rect clientArea = emulatorClientArea();
        const ImVec2 panelMin(
            overlayOrigin.x + static_cast<float>(clientArea.x) + (static_cast<float>(clientArea.w) - 220.0f) * 0.5f,
            overlayOrigin.y + static_cast<float>(clientArea.y) + (static_cast<float>(clientArea.h) - 92.0f) * 0.5f
        );
        const ImVec2 panelMax(panelMin.x + 220.0f, panelMin.y + 92.0f);
        const ImVec2 panelCenter((panelMin.x + panelMax.x) * 0.5f, (panelMin.y + panelMax.y) * 0.5f);
        const float time = static_cast<float>(SDL_GetTicks()) / 1000.0f;
        const float angleOffset = time * 3.5f;

        drawList->AddRectFilled(panelMin, panelMax, IM_COL32(0, 0, 0, 170), 12.0f);
        drawList->AddRect(panelMin, panelMax, IM_COL32(255, 255, 255, 64), 12.0f, 0, 1.5f);

        for(int i = 0; i < 8; ++i) {
            const float angle = angleOffset + (static_cast<float>(i) * 3.14159265f * 0.25f);
            const float alphaScale = static_cast<float>(i + 1) / 8.0f;
            const ImVec2 dotPos(
                panelCenter.x + std::cos(angle) * 16.0f,
                panelCenter.y - 12.0f + std::sin(angle) * 16.0f
            );
            drawList->AddCircleFilled(
                dotPos,
                3.5f,
                IM_COL32(255, 255, 255, static_cast<int>(70.0f + alphaScale * 185.0f))
            );
        }

        const char* label = "Seeking replay...";
        const ImVec2 labelSize = ImGui::CalcTextSize(label);
        drawList->AddText(
            ImVec2(panelCenter.x - labelSize.x * 0.5f, panelCenter.y + 20.0f),
            IM_COL32(255, 255, 255, 255),
            label
        );
    }

    drawInputMiniaturesOverlay(drawList, overlayOrigin);

    if(AppSettings::instance().data.debug.showFps) {
        const int fontSize = 24;
        ImFont* fpsFont = m_fontFps != nullptr ? m_fontFps : ImGui::GetFont();
        const SDL_Rect clientArea = emulatorClientArea();
        const std::array<std::string, 2> fpsLines = {
            "EMU " + std::to_string(m_emulatorFps),
            "DISP " + std::to_string(m_displayFps)
        };

        float maxTextWidth = 0.0f;
        float lineHeight = 0.0f;
        for(const std::string& line : fpsLines) {
            const ImVec2 textSize = fpsFont->CalcTextSizeA(fontSize, FLT_MAX, 0, line.c_str());
            maxTextWidth = std::max(maxTextWidth, textSize.x);
            lineHeight = std::max(lineHeight, textSize.y);
        }

        ImVec2 pos = ImVec2(
            overlayOrigin.x + width() - maxTextWidth - 32.0f,
            overlayOrigin.y + static_cast<float>(clientArea.y) + 16.0f
        );

        for(const std::string& line : fpsLines) {
            DrawTextOutlined(drawList, fpsFont, fontSize, pos, 0xFFFFFFFF, 0xFF000000, line.c_str());
            pos.y += lineHeight;
        }
    }
    m_userToast.draw(drawList, overlayOrigin, static_cast<float>(width()), static_cast<float>(height()), m_fontToast);

    m_touch->draw(drawList, overlayOrigin);
}

inline void GeraNESApp::drawInputMiniaturesOverlay(ImDrawList* drawList, const ImVec2& overlayOrigin)
{
    if(drawList == nullptr || !AppSettings::instance().data.input.showMiniatures || !m_emu.valid()) {
        return;
    }

    InputState state{};
    {
        std::scoped_lock selectedFrameLock(m_selectedInputFrameMutex);
        if(m_latestSelectedInputFrame.has_value()) {
            state = m_latestSelectedInputFrame->state;
        } else {
            state.topology = m_inputTopology;
        }
    }

    auto isMiniaturePadDevice = [](Settings::Device device) {
        switch(device) {
            case Settings::Device::CONTROLLER:
            case Settings::Device::FAMICOM_CONTROLLER:
            case Settings::Device::SNES_CONTROLLER:
            case Settings::Device::VIRTUAL_BOY_CONTROLLER:
                return true;
            default:
                return false;
        }
    };

    struct MiniaturePad {
        const char* label = "";
        InputState::PadButtons buttons = {};
    };

    std::vector<MiniaturePad> pads;
    if(state.multitapActive()) {
        pads.push_back({"P1", state.portButtons(1)});
        pads.push_back({"P2", state.portButtons(2)});
        pads.push_back({"P3", state.portButtons(3)});
        pads.push_back({"P4", state.portButtons(4)});
    } else {
        if(isMiniaturePadDevice(state.topology.port1Device)) {
            pads.push_back({"P1", state.portButtons(1)});
        }
        if(isMiniaturePadDevice(state.topology.port2Device)) {
            pads.push_back({"P2", state.portButtons(2)});
        }
        if(state.topology.expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
            pads.push_back({"EXP", state.portButtons(3)});
        }
    }

    if(pads.empty()) {
        return;
    }

    const SDL_Rect clientArea = emulatorClientArea();
    const float cardWidth = 88.0f;
    const float cardHeight = 42.0f;
    const float outerPadding = 8.0f;
    const float gap = 6.0f;
    const float availableWidth = std::max(1.0f, static_cast<float>(clientArea.w) - outerPadding * 2.0f);
    const int maxColumns = std::max(1, static_cast<int>((availableWidth + gap) / (cardWidth + gap)));
    const int columns = std::max(1, std::min(static_cast<int>(pads.size()), maxColumns));
    const int rows = static_cast<int>((pads.size() + static_cast<size_t>(columns) - 1u) / static_cast<size_t>(columns));
    const float panelWidth = outerPadding * 2.0f + static_cast<float>(columns) * cardWidth + static_cast<float>(columns - 1) * gap;
    const float panelHeight = outerPadding * 2.0f + static_cast<float>(rows) * cardHeight + static_cast<float>(rows - 1) * gap;
    const ImVec2 panelMin(
        overlayOrigin.x + static_cast<float>(clientArea.x + clientArea.w) - panelWidth - 16.0f,
        overlayOrigin.y + static_cast<float>(clientArea.y + clientArea.h) - panelHeight - 16.0f
    );
    const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + panelHeight);

    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(18, 15, 11, 176), 8.0f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(223, 209, 166, 92), 8.0f, 0, 1.0f);

    auto drawPressedBlock = [drawList](const ImVec2& min, const ImVec2& max, bool pressed) {
        drawList->AddRectFilled(min, max, pressed ? IM_COL32(196, 44, 36, 255) : IM_COL32(73, 67, 55, 255), 1.0f);
    };

    auto drawMiniPad = [&](const ImVec2& topLeft, const MiniaturePad& pad) {
        const ImVec2 cardMax(topLeft.x + cardWidth, topLeft.y + cardHeight);
        drawList->AddRectFilled(topLeft, cardMax, IM_COL32(173, 158, 122, 232), 4.0f);
        drawList->AddRect(topLeft, cardMax, IM_COL32(60, 52, 40, 255), 4.0f, 0, 1.0f);

        drawList->AddText(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), IM_COL32(34, 30, 24, 255), pad.label);

        const float u = 4.0f;
        const float dpadX = topLeft.x + 10.0f;
        const float dpadY = topLeft.y + 16.0f;
        drawPressedBlock(ImVec2(dpadX + u, dpadY), ImVec2(dpadX + u * 2.0f, dpadY + u), pad.buttons.up);
        drawPressedBlock(ImVec2(dpadX + u, dpadY + u * 2.0f), ImVec2(dpadX + u * 2.0f, dpadY + u * 3.0f), pad.buttons.down);
        drawPressedBlock(ImVec2(dpadX, dpadY + u), ImVec2(dpadX + u, dpadY + u * 2.0f), pad.buttons.left);
        drawPressedBlock(ImVec2(dpadX + u * 2.0f, dpadY + u), ImVec2(dpadX + u * 3.0f, dpadY + u * 2.0f), pad.buttons.right);
        drawPressedBlock(ImVec2(dpadX + u, dpadY + u), ImVec2(dpadX + u * 2.0f, dpadY + u * 2.0f), false);

        const float centerY = topLeft.y + 24.0f;
        drawPressedBlock(ImVec2(topLeft.x + 35.0f, centerY), ImVec2(topLeft.x + 42.0f, centerY + 3.0f), pad.buttons.select);
        drawPressedBlock(ImVec2(topLeft.x + 45.0f, centerY), ImVec2(topLeft.x + 52.0f, centerY + 3.0f), pad.buttons.start);

        const float buttonY = topLeft.y + 18.0f;
        drawPressedBlock(ImVec2(topLeft.x + 63.0f, buttonY + 6.0f), ImVec2(topLeft.x + 70.0f, buttonY + 13.0f), pad.buttons.b);
        drawPressedBlock(ImVec2(topLeft.x + 72.0f, buttonY), ImVec2(topLeft.x + 79.0f, buttonY + 7.0f), pad.buttons.a);
    };

    for(size_t index = 0; index < pads.size(); ++index) {
        const int column = static_cast<int>(index % static_cast<size_t>(columns));
        const int row = static_cast<int>(index / static_cast<size_t>(columns));
        const ImVec2 cardMin(
            panelMin.x + outerPadding + static_cast<float>(column) * (cardWidth + gap),
            panelMin.y + outerPadding + static_cast<float>(row) * (cardHeight + gap)
        );
        drawMiniPad(cardMin, pads[index]);
    }
}

inline void GeraNESApp::drawPendingRomLoadOverlay()
{
    const ModManager::StartupAssetPreloadStatus preloadStatus = m_modManager.startupAssetPreloadStatus();
    const uint32_t totalAssets = std::max<uint32_t>(1u, preloadStatus.totalAssets);
    const float progress = std::clamp(static_cast<float>(preloadStatus.completedAssets) / static_cast<float>(totalAssets), 0.0f, 1.0f);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    (void)viewport;

    SetNextWindowCenteredOnMainViewport(ImVec2(420.0f, 120.0f), ImGuiCond_Always, ImGuiCond_Always, 24.0f, ImVec2(320.0f, 100.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 255));

    if(ImGui::Begin("##ModLoadProgressOverlay", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const ImVec2 labelSize = ImGui::CalcTextSize("Loading mod assets...");
        const float labelX = windowPos.x + (windowSize.x - labelSize.x) * 0.5f;
        drawList->AddText(ImVec2(labelX, windowPos.y + 16.0f), IM_COL32(255, 255, 255, 255), "Loading mod assets...");

        const float barWidth = windowSize.x - 40.0f;
        const float barHeight = 22.0f;
        const ImVec2 barMin(windowPos.x + 20.0f, windowPos.y + 56.0f);
        const ImVec2 barMax(barMin.x + barWidth, barMin.y + barHeight);
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
        if(progress > 0.0f) {
            const float fillWidth = std::max(0.0f, (barWidth - 4.0f) * progress);
            drawList->AddRectFilled(
                ImVec2(barMin.x + 2.0f, barMin.y + 2.0f),
                ImVec2(barMin.x + 2.0f + fillWidth, barMax.y - 2.0f),
                IM_COL32(255, 255, 255, 255));
        }

        const std::string progressText = std::to_string(preloadStatus.completedAssets) + " / " + std::to_string(preloadStatus.totalAssets);
        const ImVec2 progressSize = ImGui::CalcTextSize(progressText.c_str());
        const float progressX = windowPos.x + (windowSize.x - progressSize.x) * 0.5f;
        drawList->AddText(ImVec2(progressX, windowPos.y + 88.0f), IM_COL32(255, 255, 255, 255), progressText.c_str());
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
