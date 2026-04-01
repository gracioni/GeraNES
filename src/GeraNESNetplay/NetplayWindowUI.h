#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <string>

#include "GeraNESNetplay/NetplayAppRuntime.h"
#include "GeraNESNetplay/NetplayInputAssignment.h"
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
    const bool showConnectedControls = snapshot.hosting || snapshot.connected;
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

    if(snapshot.reconnecting) {
        ImGui::TextColored(
            ImVec4(0.95f, 0.8f, 0.35f, 1.0f),
            "Reconnecting to host... %us",
            static_cast<unsigned>(snapshot.reconnectSecondsRemaining)
        );
        ImGui::SameLine();
        if(ImGui::Button("Cancel##NetplayCancelReconnectButton")) {
            runtime.disconnect();
        }
    } else if(!active || !showConnectedControls) {
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
        ImGui::Text("Reconnecting: %s", snapshot.reconnecting ? "Yes" : "No");
        if(snapshot.reconnecting) {
            ImGui::Text("Reconnect Window: %us", static_cast<unsigned>(snapshot.reconnectSecondsRemaining));
        }
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
            const std::string assignment = inputAssignmentLabel(snapshot.latestLocalInput->playerSlot, room);
            ImGui::Text("Latest Local Input: frame %u %s mask %04llX",
                        snapshot.latestLocalInput->frame,
                        assignment.c_str(),
                        static_cast<unsigned long long>(snapshot.latestLocalInput->buttonMaskLo & 0xFFFFull));
        }
        if(snapshot.latestRemoteInput.has_value()) {
            const std::string assignment = inputAssignmentLabel(snapshot.latestRemoteInput->playerSlot, room);
            ImGui::Text("Latest Remote Input: frame %u %s mask %04llX",
                        snapshot.latestRemoteInput->frame,
                        assignment.c_str(),
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

    const auto drawAssignmentTree = [&](const ParticipantInfo& participant) {
        const ParticipantId participantId = participant.id;
        const auto currentPort1 = room.port1Device.value_or(Settings::Device::NONE);
        const auto currentPort2 = room.port2Device.value_or(Settings::Device::NONE);
        const auto canAssignCandidate = [&](std::optional<Settings::Device> port1Device,
                                            std::optional<Settings::Device> port2Device,
                                            Settings::ExpansionDevice expansionDevice,
                                            Settings::NesMultitapDevice nesMultitapDevice,
                                            Settings::FamicomMultitapDevice famicomMultitapDevice,
                                            PlayerSlot slot) {
            return canAssignInputCandidate(
                room,
                participantId,
                port1Device,
                port2Device,
                expansionDevice,
                nesMultitapDevice,
                famicomMultitapDevice,
                slot
            );
        };
        const auto buildMergedAssignments = [&](std::optional<Settings::Device> port1Device,
                                                std::optional<Settings::Device> port2Device,
                                                Settings::ExpansionDevice expansionDevice,
                                                Settings::NesMultitapDevice nesMultitapDevice,
                                                Settings::FamicomMultitapDevice famicomMultitapDevice,
                                                PlayerSlot requestedSlot) {
            std::vector<PlayerSlot> slots;
            for(PlayerSlot existingSlot : participant.controllerAssignments) {
                if(existingSlot == kObserverPlayerSlot || existingSlot == requestedSlot) continue;
                if(canAssignInputCandidate(
                       room,
                       participantId,
                       port1Device,
                       port2Device,
                       expansionDevice,
                       nesMultitapDevice,
                       famicomMultitapDevice,
                       existingSlot
                   )) {
                    slots.push_back(existingSlot);
                }
            }
            if(requestedSlot != kObserverPlayerSlot) {
                slots.push_back(requestedSlot);
            }
            return slots;
        };
        const auto selectPortDevice = [&](Settings::Port port, Settings::Device device) {
            const auto port1Device = std::optional<Settings::Device>(
                port == Settings::Port::P_1 ? device : currentPort1
            );
            const auto port2Device = std::optional<Settings::Device>(
                port == Settings::Port::P_2 ? device : currentPort2
            );
            runtime.configureInputAssignments(
                participantId,
                port1Device,
                port2Device,
                room.expansionDevice,
                Settings::NesMultitapDevice::NONE,
                Settings::FamicomMultitapDevice::NONE,
                buildMergedAssignments(
                    port1Device,
                    port2Device,
                    room.expansionDevice,
                    Settings::NesMultitapDevice::NONE,
                    Settings::FamicomMultitapDevice::NONE,
                    port == Settings::Port::P_1 ? kPort1PlayerSlot : kPort2PlayerSlot
                )
            );
        };
        const auto selectExpansionDevice = [&](Settings::ExpansionDevice device) {
            const auto port1Device = std::optional<Settings::Device>(currentPort1);
            const auto port2Device = std::optional<Settings::Device>(currentPort2);
            runtime.configureInputAssignments(
                participantId,
                port1Device,
                port2Device,
                device,
                Settings::NesMultitapDevice::NONE,
                Settings::FamicomMultitapDevice::NONE,
                buildMergedAssignments(
                    port1Device,
                    port2Device,
                    device,
                    Settings::NesMultitapDevice::NONE,
                    Settings::FamicomMultitapDevice::NONE,
                    kExpansionPlayerSlot
                )
            );
        };
        const auto selectMultitapAssignment = [&](Settings::NesMultitapDevice nesDevice,
                                                  Settings::FamicomMultitapDevice famicomDevice,
                                                  PlayerSlot slot) {
            const auto port1Device = std::optional<Settings::Device>(Settings::Device::CONTROLLER);
            const auto port2Device = std::optional<Settings::Device>(Settings::Device::CONTROLLER);
            runtime.configureInputAssignments(
                participantId,
                port1Device,
                port2Device,
                Settings::ExpansionDevice::NONE,
                nesDevice,
                famicomDevice,
                buildMergedAssignments(
                    port1Device,
                    port2Device,
                    Settings::ExpansionDevice::NONE,
                    nesDevice,
                    famicomDevice,
                    slot
                )
            );
        };
        const auto drawPortOption = [&](const char* label, Settings::Port port, Settings::Device device, PlayerSlot slot) {
            const auto port1Device = std::optional<Settings::Device>(
                port == Settings::Port::P_1 ? device : currentPort1
            );
            const auto port2Device = std::optional<Settings::Device>(
                port == Settings::Port::P_2 ? device : currentPort2
            );
            const bool selected =
                participantHasAssignment(participant, slot) &&
                room.nesMultitapDevice == Settings::NesMultitapDevice::NONE &&
                room.famicomMultitapDevice == Settings::FamicomMultitapDevice::NONE &&
                ((port == Settings::Port::P_1 ? currentPort1 : currentPort2) == device);
            const bool enabled = canAssignCandidate(
                port1Device,
                port2Device,
                room.expansionDevice,
                Settings::NesMultitapDevice::NONE,
                Settings::FamicomMultitapDevice::NONE,
                slot
            );
            ImGui::BeginDisabled(!enabled);
            if(ImGui::Selectable(label, selected)) {
                selectPortDevice(port, device);
            }
            ImGui::EndDisabled();
        };
        const auto drawExpansionOption = [&](const char* label, Settings::ExpansionDevice device) {
            const bool selected =
                participantHasAssignment(participant, kExpansionPlayerSlot) &&
                room.nesMultitapDevice == Settings::NesMultitapDevice::NONE &&
                room.famicomMultitapDevice == Settings::FamicomMultitapDevice::NONE &&
                room.expansionDevice == device;
            const bool enabled = canAssignCandidate(
                std::optional<Settings::Device>(currentPort1),
                std::optional<Settings::Device>(currentPort2),
                device,
                Settings::NesMultitapDevice::NONE,
                Settings::FamicomMultitapDevice::NONE,
                kExpansionPlayerSlot
            );
            ImGui::BeginDisabled(!enabled);
            if(ImGui::Selectable(label, selected)) {
                selectExpansionDevice(device);
            }
            ImGui::EndDisabled();
        };
        const auto drawMultitapOption = [&](const char* label,
                                            Settings::NesMultitapDevice nesDevice,
                                            Settings::FamicomMultitapDevice famicomDevice,
                                            PlayerSlot slot) {
            const bool selected =
                participantHasAssignment(participant, slot) &&
                room.nesMultitapDevice == nesDevice &&
                room.famicomMultitapDevice == famicomDevice;
            const bool enabled = canAssignCandidate(
                std::optional<Settings::Device>(Settings::Device::CONTROLLER),
                std::optional<Settings::Device>(Settings::Device::CONTROLLER),
                Settings::ExpansionDevice::NONE,
                nesDevice,
                famicomDevice,
                slot
            );
            ImGui::BeginDisabled(!enabled);
            if(ImGui::Selectable(label, selected)) {
                selectMultitapAssignment(nesDevice, famicomDevice, slot);
            }
            ImGui::EndDisabled();
        };

        const ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_None;
        const bool anyPort1Enabled =
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::FAMICOM_CONTROLLER), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::ZAPPER), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::SNES_MOUSE), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER), std::optional<Settings::Device>(currentPort2), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot);
        const bool anyPort2Enabled =
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::CONTROLLER), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::FAMICOM_CONTROLLER), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::ZAPPER), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::SNES_MOUSE), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER), room.expansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot);
        const bool anyExpansionEnabled =
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::BANDAI_HYPERSHOT, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::KONAMI_HYPERSHOT, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::SUBOR_KEYBOARD, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(currentPort2), Settings::ExpansionDevice::ARKANOID_CONTROLLER, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kExpansionPlayerSlot);
        const bool anyFourScoreEnabled =
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP3PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP4PlayerSlot);
        const bool anyHoriEnabled =
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP3PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(Settings::Device::CONTROLLER), Settings::ExpansionDevice::NONE, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP4PlayerSlot);
        const bool anyMultitapEnabled = anyFourScoreEnabled || anyHoriEnabled;

        ImGui::BeginDisabled(!anyPort1Enabled);
        if(ImGui::TreeNodeEx("Port 1", treeFlags)) {
            drawPortOption("Standard Controller", Settings::Port::P_1, Settings::Device::CONTROLLER, kPort1PlayerSlot);
            drawPortOption("Famicom Controller", Settings::Port::P_1, Settings::Device::FAMICOM_CONTROLLER, kPort1PlayerSlot);
            drawPortOption("Zapper", Settings::Port::P_1, Settings::Device::ZAPPER, kPort1PlayerSlot);
            drawPortOption("Power Pad (Side A)", Settings::Port::P_1, Settings::Device::POWER_PAD_SIDE_A, kPort1PlayerSlot);
            drawPortOption("Power Pad (Side B)", Settings::Port::P_1, Settings::Device::POWER_PAD_SIDE_B, kPort1PlayerSlot);
            drawPortOption("SNES Mouse", Settings::Port::P_1, Settings::Device::SNES_MOUSE, kPort1PlayerSlot);
            drawPortOption("Subor Mouse", Settings::Port::P_1, Settings::Device::SUBOR_MOUSE, kPort1PlayerSlot);
            drawPortOption("SNES Controller", Settings::Port::P_1, Settings::Device::SNES_CONTROLLER, kPort1PlayerSlot);
            drawPortOption("Virtual Boy Controller", Settings::Port::P_1, Settings::Device::VIRTUAL_BOY_CONTROLLER, kPort1PlayerSlot);
            drawPortOption("Arkanoid Controller", Settings::Port::P_1, Settings::Device::ARKANOID_CONTROLLER, kPort1PlayerSlot);
            ImGui::TreePop();
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!anyPort2Enabled);
        if(ImGui::TreeNodeEx("Port 2", treeFlags)) {
            drawPortOption("Standard Controller", Settings::Port::P_2, Settings::Device::CONTROLLER, kPort2PlayerSlot);
            drawPortOption("Famicom Controller", Settings::Port::P_2, Settings::Device::FAMICOM_CONTROLLER, kPort2PlayerSlot);
            drawPortOption("Zapper", Settings::Port::P_2, Settings::Device::ZAPPER, kPort2PlayerSlot);
            drawPortOption("Power Pad (Side A)", Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_A, kPort2PlayerSlot);
            drawPortOption("Power Pad (Side B)", Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_B, kPort2PlayerSlot);
            drawPortOption("SNES Mouse", Settings::Port::P_2, Settings::Device::SNES_MOUSE, kPort2PlayerSlot);
            drawPortOption("Subor Mouse", Settings::Port::P_2, Settings::Device::SUBOR_MOUSE, kPort2PlayerSlot);
            drawPortOption("SNES Controller", Settings::Port::P_2, Settings::Device::SNES_CONTROLLER, kPort2PlayerSlot);
            drawPortOption("Virtual Boy Controller", Settings::Port::P_2, Settings::Device::VIRTUAL_BOY_CONTROLLER, kPort2PlayerSlot);
            drawPortOption("Arkanoid Controller", Settings::Port::P_2, Settings::Device::ARKANOID_CONTROLLER, kPort2PlayerSlot);
            ImGui::TreePop();
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!anyExpansionEnabled);
        if(ImGui::TreeNodeEx("Expansion Port", treeFlags)) {
            drawExpansionOption("Standard Controller (Famicom)", Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM);
            drawExpansionOption("Bandai Hyper Shot", Settings::ExpansionDevice::BANDAI_HYPERSHOT);
            drawExpansionOption("Konami Hyper Shot", Settings::ExpansionDevice::KONAMI_HYPERSHOT);
            drawExpansionOption("Family Trainer (Side A)", Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A);
            drawExpansionOption("Family Trainer (Side B)", Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B);
            drawExpansionOption("Subor Keyboard", Settings::ExpansionDevice::SUBOR_KEYBOARD);
            drawExpansionOption("Family Basic Keyboard", Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD);
            drawExpansionOption("Arkanoid Controller (Famicom)", Settings::ExpansionDevice::ARKANOID_CONTROLLER);
            ImGui::TreePop();
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!anyMultitapEnabled);
        if(ImGui::TreeNodeEx("Multitap", treeFlags)) {
            ImGui::BeginDisabled(!anyFourScoreEnabled);
            if(ImGui::TreeNodeEx("Four Score", treeFlags)) {
                drawMultitapOption("P1", Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP1PlayerSlot);
                drawMultitapOption("P2", Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP2PlayerSlot);
                drawMultitapOption("P3", Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP3PlayerSlot);
                drawMultitapOption("P4", Settings::NesMultitapDevice::FOUR_SCORE, Settings::FamicomMultitapDevice::NONE, kMultitapP4PlayerSlot);
                ImGui::TreePop();
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!anyHoriEnabled);
            if(ImGui::TreeNodeEx("Hori Adapter", treeFlags)) {
                drawMultitapOption("P1", Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP1PlayerSlot);
                drawMultitapOption("P2", Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP2PlayerSlot);
                drawMultitapOption("P3", Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP3PlayerSlot);
                drawMultitapOption("P4", Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::HORI_ADAPTER, kMultitapP4PlayerSlot);
                ImGui::TreePop();
            }
            ImGui::EndDisabled();
            ImGui::TreePop();
        }
        ImGui::EndDisabled();
    };

    if(ImGui::BeginTable("NetplayParticipants", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Net", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Admin", ImGuiTableColumnFlags_WidthFixed, 90.0f);
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
                const std::string preview = participantAssignmentsLabel(participant, room);
                const float comboHeight = ImGui::GetFrameHeight();
                for(PlayerSlot slot : participant.controllerAssignments) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.12f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.18f, 0.18f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.22f, 0.22f, 1.0f));
                    if(ImGui::Button(("X##assignment" + std::to_string(participant.id) + "_" + std::to_string(slot)).c_str(), ImVec2(comboHeight, 0.0f))) {
                        runtime.removeControllerAssignment(participant.id, slot);
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(inputAssignmentLabel(slot, room).c_str());
                }
                if(!participant.controllerAssignments.empty()) {
                    ImGui::Spacing();
                }
                std::string comboId = "##ctrl" + std::to_string(participant.id);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if(ImGui::BeginCombo(comboId.c_str(), preview.c_str())) {
                    drawAssignmentTree(participant);
                    ImGui::EndCombo();
                }
            } else if(participant.controllerAssignments.empty()) {
                ImGui::TextUnformatted("Observer");
            } else {
                const std::string label = participantAssignmentsLabel(participant, room);
                ImGui::TextUnformatted(label.c_str());
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
                ImGui::TextDisabled("%us left", static_cast<unsigned>(participant.reservationSecondsRemaining));
            } else {
                ImGui::TextDisabled("reconnect");
            }
            ImGui::TableNextColumn();
            if(snapshot.hosting && participant.id != snapshot.localParticipantId) {
                if(participant.reconnectReserved) {
                    if(ImGui::SmallButton(("Remove Reservation##" + std::to_string(participant.id)).c_str())) {
                        runtime.removeReconnectReservation(participant.id);
                    }
                } else if(ImGui::SmallButton(("Kick##" + std::to_string(participant.id)).c_str())) {
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
