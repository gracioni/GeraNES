#pragma once

inline void GeraNESApp::drawCpuBreakpointsWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(620.0f, 620.0f));
    if(m_cpuBreakpointsRequestFocus) {
        ImGui::SetNextWindowFocus();
        m_cpuBreakpointsRequestFocus = false;
    }

    if(!ImGui::Begin("CPU Breakpoints", &m_showCpuBreakpointsWindow)) {
        AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;
        m_cpuBreakpointsFocused = false;
        ImGui::End();
        return;
    }

    AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;
    m_cpuBreakpointsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    m_imGuiWindowFocusBlocksEmulator |= m_cpuBreakpointsFocused;

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
    configChanged |= ImGui::Checkbox("CPU accepts NMI", &config.breakOnCpuNmiAccepted);
    configChanged |= ImGui::Checkbox("CPU accepts IRQ", &config.breakOnCpuIrqAccepted);
    configChanged |= ImGui::Checkbox("NMI hijacks BRK", &config.breakOnNmiHijacksBrk);
    configChanged |= ImGui::Checkbox("NMI hijacks IRQ", &config.breakOnNmiHijacksIrq);

    ImGui::Separator();
    ImGui::TextUnformatted("PPU Events");
    configChanged |= ImGui::Checkbox("VBlank start", &config.breakOnVBlankStart);
    configChanged |= ImGui::Checkbox("VBlank end", &config.breakOnVBlankEnd);
    configChanged |= ImGui::Checkbox("Sprite overflow", &config.breakOnSpriteOverflow);
    configChanged |= ImGui::Checkbox("OAM2 overflow latch set", &config.breakOnOam2OverflowSet);
    configChanged |= ImGui::Checkbox("OAM2 overflow latch cleared", &config.breakOnOam2OverflowClear);
    configChanged |= ImGui::Checkbox("Rendering enabled", &config.breakOnRenderingEnabled);
    configChanged |= ImGui::Checkbox("Rendering disabled", &config.breakOnRenderingDisabled);
    configChanged |= ImGui::Checkbox("Exact PPU position", &config.breakOnPpuPosition);
    if(config.breakOnPpuPosition) {
        ImGui::Indent();
        SetNextItemWidthScaledClamped(90.0f);
        configChanged |= ImGui::InputInt("Scanline##PpuBreakpoint", &config.ppuPositionScanline);
        SetNextItemWidthScaledClamped(90.0f);
        configChanged |= ImGui::InputInt("Dot##PpuBreakpoint", &config.ppuPositionCycle);
        config.ppuPositionScanline = std::clamp(config.ppuPositionScanline, 0, 311);
        config.ppuPositionCycle = std::clamp(config.ppuPositionCycle, 0, 340);
        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("IRQ Sources");
    configChanged |= ImGui::Checkbox("APU frame IRQ start", &config.breakOnApuFrameIrqStart);
    configChanged |= ImGui::Checkbox("APU frame IRQ end", &config.breakOnApuFrameIrqEnd);
    configChanged |= ImGui::Checkbox("DMC IRQ start", &config.breakOnDmcIrqStart);
    configChanged |= ImGui::Checkbox("DMC IRQ end", &config.breakOnDmcIrqEnd);
    configChanged |= ImGui::Checkbox("Mapper IRQ start", &config.breakOnMapperIrqStart);
    configChanged |= ImGui::Checkbox("Mapper IRQ end", &config.breakOnMapperIrqEnd);

    ImGui::Separator();
    ImGui::TextUnformatted("DMA Events");
    configChanged |= ImGui::Checkbox("OAM DMA start ($4014)", &config.breakOnOamDmaStart);
    configChanged |= ImGui::Checkbox("OAM DMA end", &config.breakOnOamDmaEnd);
    configChanged |= ImGui::Checkbox("DMC DMA start", &config.breakOnDmcDmaStart);
    configChanged |= ImGui::Checkbox("DMC DMA end", &config.breakOnDmcDmaEnd);
    configChanged |= ImGui::Checkbox("DMC DMA abort", &config.breakOnDmcDmaAbort);

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
    SetNextItemWidthScaledClamped(90.0f);
    configChanged |= ImGui::InputScalar("##ExactCpuReadAddress", ImGuiDataType_U16, &config.exactCpuReadAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
    configChanged |= ImGui::Checkbox("Break on CPU write", &config.breakOnExactCpuWrite);
    ImGui::SameLine();
    SetNextItemWidthScaledClamped(90.0f);
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
