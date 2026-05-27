#pragma once

#include "GeraNESApp/GeraNESApp.CustomWindowChromeUI.inl"
#include "GeraNESApp/GeraNESApp.NsfPlayerVisualizerUI.inl"
#include "GeraNESApp/GeraNESApp.CpuDebugSharedUI.inl"
#include "GeraNESApp/GeraNESApp.ImprovementsWindowUI.inl"
#include "GeraNESApp/GeraNESApp.AboutWindowUI.inl"
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
    if(m_emu.isNsfLoaded()) drawNsfPlayerVisualizer();
#endif

    m_inputBindingConfigWindow.update();
    m_powerPadConfigWindow.update();

    if(m_showImprovementsWindow) {
        drawImprovementsWindow();
    }

    if(m_showPaletteWindow) {
        drawPaletteWindow();
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

    if(m_snesMouseGrabActive || m_arkanoidGrabActive) {
        m_imGuiWindowFocusBlocksEmulator = false;
    }
}

inline void GeraNESApp::showOverlay()
{
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(mainViewport);
    const ImVec2 overlayOrigin = mainViewport != nullptr ? mainViewport->Pos : ImVec2(0.0f, 0.0f);

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
