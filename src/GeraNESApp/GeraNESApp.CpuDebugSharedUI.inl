#pragma once

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
