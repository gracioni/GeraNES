#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <string>

#include "GeraNESNetplay/NetplayAppRuntime.h"
#include "imgui.h"

namespace Netplay {

inline const char* sessionStateLabel(SessionState state)
{
    switch(state) {
        case SessionState::Lobby: return "Lobby";
        case SessionState::ValidatingRom: return "Validating ROM";
        case SessionState::ReadyCheck: return "Ready Check";
        case SessionState::Starting: return "Starting";
        case SessionState::Running: return "Running";
        case SessionState::Resyncing: return "Resyncing";
        case SessionState::Paused: return "Paused";
        case SessionState::Ended: return "Ended";
        default: return "Unknown";
    }
}

inline void drawNetplayWindow(bool& showWindow,
                              NetplayAppRuntime& runtime,
                              const ImVec2& viewportCenter)
{
    if(!showWindow) return;

    auto& cfg = AppSettings::instance().data.netplay;
    ImGui::SetNextWindowSize(ImVec2(760, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(!ImGui::Begin("Netplay", &showWindow)) {
        ImGui::End();
        return;
    }

    const auto snapshot = runtime.uiSnapshot();
    const bool active = snapshot.active;
    const auto& room = snapshot.room;
    const bool canHost = snapshot.localRomLoaded;
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Display Name##NetplayDisplayName", &cfg.displayName);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Host##NetplayHostName", &cfg.hostName);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Port##NetplayPort", &cfg.port);
    cfg.port = std::clamp(cfg.port, 1, 65535);

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Max Peers##NetplayMaxPeers", &cfg.maxPeers);
    cfg.maxPeers = std::clamp(cfg.maxPeers, 1, 32);

#ifndef NDEBUG
    ImGui::Checkbox("Auto Gameplay Tuning##NetplayAutoGameplayTuning", &cfg.autoGameplayTuning);
    if(!cfg.autoGameplayTuning) {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Input Delay##NetplayInputDelay", &cfg.inputDelayFrames);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Predict Frames##NetplayPredictFrames", &cfg.predictFrames);
    } else {
        ImGui::Text("Auto Delay: %u frame(s)", static_cast<unsigned>(snapshot.autoSettings.currentRecommendedDelay));
        ImGui::Text("Auto Predict: %u frame(s)", static_cast<unsigned>(snapshot.autoSettings.currentFixedPredict));
    }
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Gameplay Lag ms##NetplayGameplayLag", &cfg.gameplayReceiveDelayMs);
#else
    cfg.autoGameplayTuning = true;
#endif
    cfg.inputDelayFrames = std::clamp(cfg.inputDelayFrames, 0, 8);
    cfg.predictFrames = std::clamp(cfg.predictFrames, 0, 8);
    cfg.gameplayReceiveDelayMs = std::clamp(cfg.gameplayReceiveDelayMs, 0, 500);

    if(!active) {
        ImGui::BeginDisabled(!canHost);
        if(ImGui::Button("Host##NetplayHostButton")) {
            runtime.host(static_cast<uint16_t>(cfg.port), static_cast<size_t>(cfg.maxPeers), cfg.displayName);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Join##NetplayJoinButton")) {
            runtime.join(cfg.hostName, static_cast<uint16_t>(cfg.port), cfg.displayName);
        }
        if(!canHost) {
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "Load a ROM before hosting.");
        }
    } else {
        if(ImGui::Button("Disconnect##NetplayDisconnectButton")) {
            runtime.disconnect();
        }
    }

    ImGui::Separator();
    if(ImGui::CollapsingHeader("Connection##NetplayConnection")) {
        ImGui::Text("Transport Active: %s", active ? "Yes" : "No");
        ImGui::Text("Hosting: %s", snapshot.hosting ? "Yes" : "No");
        ImGui::Text("Connected: %s", snapshot.connected ? "Yes" : "No");
        ImGui::Text("Local Participant: %d", static_cast<int>(snapshot.localParticipantId));
        ImGui::Text("Local ROM: %s", snapshot.localRomLoaded ? snapshot.localRomGameName.c_str() : "<none>");
        if(snapshot.localRomLoaded) {
            ImGui::Text("Local CRC32: %08X", snapshot.localRomCrc32);
        }
        ImGui::Text("Delay/Predict: %u / %u",
                    static_cast<unsigned>(room.inputDelayFrames),
                    static_cast<unsigned>(room.predictFrames));
    }

    if(!snapshot.lastError.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.3f, 1.0f), "%s", snapshot.lastError.c_str());
    }

    ImGui::Separator();
    if(ImGui::CollapsingHeader("Session##NetplaySession")) {
        ImGui::Text("Session Id: %u", room.sessionId);
        ImGui::Text("State: %s", sessionStateLabel(room.state));
        ImGui::Text("Selected ROM: %s", room.selectedGameName.empty() ? "<none>" : room.selectedGameName.c_str());
        ImGui::Text("ROM CRC32: %08X", room.romValidation.romCrc32);
        ImGui::Text("Mapper/Sub: %u / %u", room.romValidation.mapperId, room.romValidation.subMapperId);
        ImGui::Text("Input Delay: %u frame(s)", static_cast<unsigned>(room.inputDelayFrames));
        ImGui::Text("Predict Frames: %u frame(s)", static_cast<unsigned>(room.predictFrames));
        ImGui::Text("Gameplay Lag: %d ms", cfg.gameplayReceiveDelayMs);
#ifndef NDEBUG
        ImGui::Text("Gameplay Tuning: %s", snapshot.autoSettings.enabled ? "Auto" : "Manual");
        if(!snapshot.autoSettings.lastDecisionReason.empty()) {
            ImGui::TextWrapped("Auto Decision: %s", snapshot.autoSettings.lastDecisionReason.c_str());
        }
#endif
        ImGui::Text("Active Resync: %u", room.activeResyncId);
        ImGui::Text("Resync Acks Pending: %u", room.pendingResyncAckCount);
        ImGui::Text("Current Frame: %u", room.currentFrame);
        ImGui::Text("Confirmed Frame: %u", room.lastConfirmedFrame);
        ImGui::Text("Participants: %zu", room.participants.size());
    }
    if(ImGui::CollapsingHeader("Diagnostics##NetplayDiagnostics")) {
        ImGui::Text("Local Input Frames: %zu", snapshot.localInputCount);
        ImGui::Text("Remote Input Frames: %zu", snapshot.remoteInputCount);
        ImGui::Text("Predicted Frames Used: %u", snapshot.predictionStats.predictedFrameUseCount);
        ImGui::Text("Prediction Hits: %u", snapshot.predictionStats.predictionHitCount);
        ImGui::Text("Prediction Misses: %u", snapshot.predictionStats.predictionMissCount);
        ImGui::Text("Playback Stops: %u", snapshot.predictionStats.playbackStopCount);
        ImGui::Text("Stops By Missing Input: %u", snapshot.predictionStats.stopDueToMissingInputCount);
        ImGui::Text("Stops By Prediction Limit: %u", snapshot.predictionStats.stopDueToPredictionLimitCount);
        ImGui::Text("Last Stop: frame %u", snapshot.predictionStats.lastStopFrame);
        if(!snapshot.predictionStats.lastStopReason.empty()) {
            ImGui::Text("Last Stop Reason: %s", snapshot.predictionStats.lastStopReason.c_str());
        }
        ImGui::Text("Unresolved Predicted Frames: %u", snapshot.unresolvedPredictedRemoteFrameCount);
        ImGui::Text("Latest Predicted Frame: %u", snapshot.latestPredictedRemoteFrame);
        ImGui::Text("Scheduled Rollbacks: %u", snapshot.predictionStats.rollbackScheduledCount);
        ImGui::Text("Missing Input Gaps: %u", snapshot.predictionStats.missingInputGapCount);
        ImGui::Text("Future Mismatches: %u", snapshot.predictionStats.futureFrameMismatchCount);
        ImGui::Text("Confirmed Conflicts: %u", snapshot.predictionStats.confirmedFrameConflictCount);
        ImGui::Text("Hard Resyncs: %u", snapshot.predictionStats.hardResyncCount);
        ImGui::Text("Applied Rollbacks: %u", snapshot.runtimeDiagnostics.rollbackStats.rollbackCount);
        ImGui::Text("Last Applied Rollback: %u -> %u",
                    snapshot.runtimeDiagnostics.rollbackStats.lastRollbackFromFrame,
                    snapshot.runtimeDiagnostics.rollbackStats.lastRollbackToFrame);
        ImGui::Text("Last Remote CRC: %08X @ frame %u", room.lastRemoteCrc32, room.lastRemoteCrcFrame);

        if(snapshot.latestLocalInput.has_value()) {
            ImGui::Text("Latest Local Input: frame %u slot %u mask %04llX",
                        snapshot.latestLocalInput->frame,
                        static_cast<unsigned>(snapshot.latestLocalInput->playerSlot) + 1u,
                        static_cast<unsigned long long>(snapshot.latestLocalInput->buttonMaskLo & 0xFFFFull));
        }
        if(snapshot.latestRemoteInput.has_value()) {
            ImGui::Text("Latest Remote Input: frame %u slot %u mask %04llX",
                        snapshot.latestRemoteInput->frame,
                        static_cast<unsigned>(snapshot.latestRemoteInput->playerSlot) + 1u,
                        static_cast<unsigned long long>(snapshot.latestRemoteInput->buttonMaskLo & 0xFFFFull));
        }
    }

    if(snapshot.hosting) {
        const bool canForceResync =
            room.state == SessionState::Running || room.state == SessionState::Paused;
        ImGui::BeginDisabled(!canForceResync);
        if(ImGui::Button("Force Resync##NetplayForceResync")) {
            runtime.requestForceResync();
        }
        ImGui::EndDisabled();
        if(!snapshot.sessionBlockedReason.empty()) {
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "%s", snapshot.sessionBlockedReason.c_str());
        }
    }

    if(room.state == SessionState::Resyncing) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f),
                           "Resyncing to frame %u. Waiting for %u ACK(s).",
                           room.resyncTargetFrame,
                           room.pendingResyncAckCount);
    }

    if(ImGui::BeginTable("NetplayParticipants", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Controller", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Net", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Admin", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableHeadersRow();

        for(const auto& participant : room.participants) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", static_cast<int>(participant.id));
            ImGui::TableNextColumn();
            if(participant.connected) {
                ImGui::TextUnformatted(participant.displayName.c_str());
            } else if(participant.reconnectReserved) {
                ImGui::TextDisabled("%s (reserved)", participant.displayName.c_str());
            } else {
                ImGui::TextDisabled("%s (disconnected)", participant.displayName.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                participant.role == ParticipantRole::Host ? "Host" :
                participant.role == ParticipantRole::Player ? "Player" : "Observer"
            );
            ImGui::TableNextColumn();
            if(snapshot.hosting) {
                int controllerValue = participant.controllerAssignment == kObserverPlayerSlot
                    ? -1
                    : static_cast<int>(participant.controllerAssignment);
                const char* preview =
                    controllerValue < 0 ? "Observer" :
                    controllerValue == 0 ? "P1" :
                    controllerValue == 1 ? "P2" :
                    controllerValue == 2 ? "P3" : "P4";
                std::string comboId = "##ctrl" + std::to_string(participant.id);
                if(ImGui::BeginCombo(comboId.c_str(), preview)) {
                    if(ImGui::Selectable("Observer", controllerValue < 0)) {
                        runtime.assignController(participant.id, kObserverPlayerSlot);
                    }
                    for(int i = 0; i < 4; ++i) {
                        const std::string label = "P" + std::to_string(i + 1);
                        if(ImGui::Selectable(label.c_str(), controllerValue == i)) {
                            runtime.assignController(participant.id, static_cast<PlayerSlot>(i));
                        }
                    }
                    ImGui::EndCombo();
                }
            } else if(participant.controllerAssignment == kObserverPlayerSlot) {
                ImGui::TextUnformatted("Observer");
            } else {
                ImGui::Text("P%u", static_cast<unsigned>(participant.controllerAssignment) + 1u);
            }
            ImGui::TableNextColumn();
            const char* romLabel =
                !participant.romLoaded ? "Not loaded" :
                participant.romCompatible ? "ROM OK" : "Mismatch";
            ImVec4 romColor =
                !participant.romLoaded ? ImVec4(0.85f, 0.65f, 0.25f, 1.0f) :
                participant.romCompatible ? ImVec4(0.35f, 0.9f, 0.35f, 1.0f) :
                ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
            ImGui::TextColored(romColor, "%s", romLabel);
            ImGui::TableNextColumn();
            if(participant.id == snapshot.localParticipantId) {
                ImGui::TextUnformatted("-");
            } else if(participant.connected) {
                ImGui::Text("%ums / %ums", participant.pingMs, participant.jitterMs);
            } else if(participant.reconnectReserved) {
                ImGui::TextDisabled("reserved");
            } else {
                ImGui::TextDisabled("reconnect");
            }
            ImGui::TableNextColumn();
            if(snapshot.hosting && participant.id != snapshot.localParticipantId) {
                if(ImGui::SmallButton(("Kick##" + std::to_string(participant.id)).c_str())) {
                    runtime.kickParticipant(participant.id);
                }
            } else {
                ImGui::TextUnformatted("-");
            }
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    if(ImGui::BeginChild("NetplayLog", ImVec2(0.0f, 180.0f), true)) {
        for(const std::string& line : snapshot.eventLog) {
            ImGui::TextUnformatted(line.c_str());
        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace Netplay

#endif
