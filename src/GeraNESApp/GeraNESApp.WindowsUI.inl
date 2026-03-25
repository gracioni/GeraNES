#pragma once

inline void GeraNESApp::showGui()
{
    const ImVec2 viewportCenter = ImGui::GetMainViewport()->GetCenter();

    float lastMenuBarHeight = m_menuBarHeight;

    if(m_showMenuBar) menuBar();
    else m_menuBarHeight = 0;

    if(lastMenuBarHeight != m_menuBarHeight) updateBuffers();

#ifdef ENABLE_NSF_PLAYER
    if(m_emu.isNsfLoaded()) drawNsfPlayerVisualizer();
#endif

    m_inputBindingConfigWindow.update();
    m_powerPadConfigWindow.update();

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

    if(m_showArkanoidNesConfigWindow) {
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
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
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
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
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
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

#ifndef __EMSCRIPTEN__
    if(m_showNetplayWindow) {
        ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Netplay", &m_showNetplayWindow, ImGuiWindowFlags_NoResize)) {
            auto& cfg = AppSettings::instance().data.netplay;
            auto sessionStateLabel = [](Netplay::SessionState state) {
                switch(state) {
                    case Netplay::SessionState::Lobby: return "Lobby";
                    case Netplay::SessionState::ValidatingRom: return "ValidatingRom";
                    case Netplay::SessionState::ReadyCheck: return "ReadyCheck";
                    case Netplay::SessionState::Starting: return "Starting";
                    case Netplay::SessionState::Running: return "Running";
                    case Netplay::SessionState::Resyncing: return "Resyncing";
                    case Netplay::SessionState::Paused: return "Paused";
                    case Netplay::SessionState::Ended: return "Ended";
                    default: return "Unknown";
                }
            };

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

            const bool active = m_netplayCoordinator.isActive();
            const auto& room = m_netplayCoordinator.session().roomState();
            const bool canEditInputDelay =
                !active ||
                (m_netplayCoordinator.isHosting() &&
                 (room.state == Netplay::SessionState::Lobby ||
                  room.state == Netplay::SessionState::ValidatingRom ||
                  room.state == Netplay::SessionState::ReadyCheck ||
                  room.state == Netplay::SessionState::Starting));

            ImGui::BeginDisabled(!canEditInputDelay);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Input Delay##NetplayInputDelay", &cfg.inputDelayFrames);
            ImGui::EndDisabled();
            cfg.inputDelayFrames = std::clamp(cfg.inputDelayFrames, 0, 8);
            if(m_netplayCoordinator.isHosting()) {
                ImGui::Checkbox("Resume when all ready##NetplayAutoResume", &cfg.autoResumeWhenReady);
            }
            if(!active) {
                if(ImGui::Button("Host##NetplayHostButton")) {
                    m_netplayCoordinator.host(static_cast<uint16_t>(cfg.port), static_cast<size_t>(cfg.maxPeers), cfg.displayName);
                }
                ImGui::SameLine();
                if(ImGui::Button("Join##NetplayJoinButton")) {
                    m_netplayCoordinator.join(cfg.hostName, static_cast<uint16_t>(cfg.port), cfg.displayName);
                }
            }
            else {
                if(ImGui::Button("Disconnect##NetplayDisconnectButton")) {
                    m_netplayCoordinator.disconnect();
                }
            }

            ImGui::Separator();
            ImGui::Text("Transport Active: %s", active ? "Yes" : "No");
            ImGui::Text("Hosting: %s", m_netplayCoordinator.isHosting() ? "Yes" : "No");
            ImGui::Text("Connected: %s", m_netplayCoordinator.isConnected() ? "Yes" : "No");
            ImGui::Text("Local Participant: %d", static_cast<int>(m_netplayCoordinator.localParticipantId()));

            if(!m_netplayCoordinator.lastError().empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.3f, 1.0f), "%s", m_netplayCoordinator.lastError().c_str());
            }

            const auto& localInputs = m_netplayCoordinator.localInputs();
            const auto& remoteInputs = m_netplayCoordinator.remoteInputs();
            const auto& predictionStats = m_netplayCoordinator.predictionStats();
            const auto runtimeDiag = m_emu.getNetplayDiagnostics();
            std::string sessionBlockedReason = netplaySessionBlockedReason();
            Netplay::ParticipantInfo* localParticipant = nullptr;
            for(auto& participant : const_cast<std::vector<Netplay::ParticipantInfo>&>(room.participants)) {
                if(participant.id == m_netplayCoordinator.localParticipantId()) {
                    localParticipant = &participant;
                    break;
                }
            }
            ImGui::Separator();
            ImGui::Text("Session Id: %u", room.sessionId);
            ImGui::Text("State: %s", sessionStateLabel(room.state));
            ImGui::Text("Selected ROM: %s", room.selectedGameName.empty() ? "<none>" : room.selectedGameName.c_str());
            ImGui::Text("ROM CRC32: %08X", room.romValidation.romCrc32);
            ImGui::Text("Mapper/Sub: %u / %u", room.romValidation.mapperId, room.romValidation.subMapperId);
            ImGui::Text("Input Delay: %u frame(s)", static_cast<unsigned>(room.inputDelayFrames));
            ImGui::Text("Active Resync: %u", room.activeResyncId);
            ImGui::Text("Resync Acks Pending: %u", room.pendingResyncAckCount);
            ImGui::Text("Current Frame: %u", room.currentFrame);
            ImGui::Text("Confirmed Frame: %u", room.lastConfirmedFrame);
            ImGui::Text("Participants: %zu", room.participants.size());
            ImGui::Text("Local Input Frames: %zu", localInputs.size());
            ImGui::Text("Remote Input Frames: %zu", remoteInputs.size());
            ImGui::Text("Prediction Hits: %u", predictionStats.predictionHitCount);
            ImGui::Text("Prediction Misses: %u", predictionStats.predictionMissCount);
            ImGui::Text("Scheduled Rollbacks: %u", predictionStats.rollbackScheduledCount);
            ImGui::Text("Missing Input Gaps: %u", predictionStats.missingInputGapCount);
            ImGui::Text("Future Mismatches: %u", predictionStats.futureFrameMismatchCount);
            ImGui::Text("Confirmed Conflicts: %u", predictionStats.confirmedFrameConflictCount);
            ImGui::Text("Hard Resyncs: %u", predictionStats.hardResyncCount);
            ImGui::Text("Applied Rollbacks: %u", runtimeDiag.rollbackStats.rollbackCount);
            ImGui::Text("Last Applied Rollback: %u -> %u",
                        runtimeDiag.rollbackStats.lastRollbackFromFrame,
                        runtimeDiag.rollbackStats.lastRollbackToFrame);
            ImGui::Text("Last Remote CRC: %08X @ frame %u", room.lastRemoteCrc32, room.lastRemoteCrcFrame);
            if(!predictionStats.lastDecision.empty()) {
                ImGui::Text("Last Decision: %s", predictionStats.lastDecision.c_str());
                if(predictionStats.lastDecisionSlot != Netplay::kObserverPlayerSlot) {
                    ImGui::Text("Decision Frame/Slot: %u / P%u",
                                predictionStats.lastDecisionFrame,
                                static_cast<unsigned>(predictionStats.lastDecisionSlot) + 1u);
                } else {
                    ImGui::Text("Decision Frame: %u", predictionStats.lastDecisionFrame);
                }
            }
            if(const auto* latestLocal = localInputs.latest()) {
                ImGui::Text("Latest Local Input: frame %u slot %u mask %04llX",
                            latestLocal->frame,
                            static_cast<unsigned>(latestLocal->playerSlot) + 1u,
                            static_cast<unsigned long long>(latestLocal->buttonMaskLo & 0xFFFFull));
            }
            if(const auto* latestRemote = remoteInputs.latest()) {
                ImGui::Text("Latest Remote Input: frame %u slot %u mask %04llX",
                            latestRemote->frame,
                            static_cast<unsigned>(latestRemote->playerSlot) + 1u,
                            static_cast<unsigned long long>(latestRemote->buttonMaskLo & 0xFFFFull));
            }

            ImGui::Separator();
            bool localReady = localParticipant != nullptr ? localParticipant->ready : false;
            if(ImGui::Checkbox("Ready##NetplayLocalReady", &localReady)) {
                m_netplayCoordinator.setLocalReady(localReady);
            }
            if(m_netplayCoordinator.isHosting()) {
                ImGui::SameLine();
                const bool canStartSession = sessionBlockedReason.empty();
                ImGui::BeginDisabled(!canStartSession);
                if(ImGui::Button("Start Session##NetplayStartSession")) {
                    m_netplayCoordinator.startSession();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if(room.state == Netplay::SessionState::Running) {
                    if(ImGui::Button("Pause Session##NetplayPauseSession")) {
                        m_netplayCoordinator.pauseSession();
                    }
                } else if(room.state == Netplay::SessionState::Paused) {
                    ImGui::BeginDisabled(!canStartSession);
                    if(ImGui::Button("Resume Session##NetplayResumeSession")) {
                        m_netplayCoordinator.resumeSession();
                    }
                    ImGui::EndDisabled();
                } else {
                    ImGui::BeginDisabled();
                    ImGui::Button(room.state == Netplay::SessionState::Ended ? "Session Ended##NetplayPauseDisabled" : "Pause Session##NetplayPauseDisabled");
                    ImGui::EndDisabled();
                }
                ImGui::SameLine();
                const bool canForceResync =
                    m_emu.valid() &&
                    (room.state == Netplay::SessionState::Running ||
                     room.state == Netplay::SessionState::Paused);
                ImGui::BeginDisabled(!canForceResync);
                if(ImGui::Button("Force Resync##NetplayForceResync")) {
                    const std::vector<uint8_t> statePayload = m_emu.saveStateToMemory();
                    if(!statePayload.empty()) {
                        const uint32_t payloadCrc32 = Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
                        m_netplayCoordinator.beginResync(m_emu.frameCount(), statePayload, payloadCrc32);
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if(ImGui::Button("End Session##NetplayEndSession")) {
                    m_netplayCoordinator.endSession();
                }
                if(!sessionBlockedReason.empty()) {
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "%s", sessionBlockedReason.c_str());
                }
            } else if(localParticipant != nullptr && localParticipant->controllerAssignment == Netplay::kObserverPlayerSlot) {
                ImGui::Separator();
                ImGui::TextUnformatted("Request player slot");
                if(localParticipant->controllerRequestPending) {
                    ImGui::Text("Pending request: P%u",
                                static_cast<unsigned>(localParticipant->requestedControllerSlot) + 1u);
                    if(ImGui::Button("Cancel Request##NetplayCancelControllerRequest")) {
                        m_netplayCoordinator.cancelControllerRequest();
                    }
                } else {
                    for(int i = 0; i < 4; ++i) {
                        std::string buttonLabel = "Request P" + std::to_string(i + 1) + "##NetplayRequestP" + std::to_string(i + 1);
                        if(ImGui::Button(buttonLabel.c_str())) {
                            m_netplayCoordinator.requestControllerSlot(static_cast<Netplay::PlayerSlot>(i));
                        }
                        if(i < 3) ImGui::SameLine();
                    }
                }
            }

            if(room.state == Netplay::SessionState::Resyncing) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f),
                                   "Resyncing to frame %u. Waiting for %u ACK(s).",
                                   room.resyncTargetFrame,
                                   room.pendingResyncAckCount);
            } else if(m_netplayCoordinator.awaitingSpectatorSync()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f),
                                   "Waiting for initial spectator sync...");
            }

            if(ImGui::BeginTable("NetplayParticipants", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Controller", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Net", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Ready", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Admin", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();

                for(auto& participant : const_cast<std::vector<Netplay::ParticipantInfo>&>(room.participants)) {
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
                    if(m_netplayCoordinator.isHosting() && participant.id != m_netplayCoordinator.localParticipantId()) {
                        const char* preview =
                            participant.role == Netplay::ParticipantRole::Player ? "Player" : "Observer";
                        std::string comboId = "##role" + std::to_string(participant.id);
                        if(ImGui::BeginCombo(comboId.c_str(), preview)) {
                            if(ImGui::Selectable("Player", participant.role == Netplay::ParticipantRole::Player)) {
                                m_netplayCoordinator.setParticipantRole(participant.id, Netplay::ParticipantRole::Player);
                            }
                            if(ImGui::Selectable("Observer", participant.role == Netplay::ParticipantRole::Observer)) {
                                m_netplayCoordinator.setParticipantRole(participant.id, Netplay::ParticipantRole::Observer);
                            }
                            ImGui::EndCombo();
                        }
                    } else {
                        const char* roleLabel = participant.role == Netplay::ParticipantRole::Host ? "Host" :
                                                participant.role == Netplay::ParticipantRole::Player ? "Player" : "Observer";
                        ImGui::TextUnformatted(roleLabel);
                    }
                    ImGui::TableNextColumn();
                    if(m_netplayCoordinator.isHosting()) {
                        int controllerValue = participant.controllerAssignment == Netplay::kObserverPlayerSlot
                            ? -1
                            : static_cast<int>(participant.controllerAssignment);
                        const char* preview = controllerValue < 0 ? "None" : (controllerValue == 0 ? "P1" : controllerValue == 1 ? "P2" : controllerValue == 2 ? "P3" : "P4");
                        std::string comboId = "##ctrl" + std::to_string(participant.id);
                        if(ImGui::BeginCombo(comboId.c_str(), preview)) {
                            if(ImGui::Selectable("None", controllerValue < 0)) {
                                m_netplayCoordinator.assignController(participant.id, Netplay::kObserverPlayerSlot);
                            }
                            for(int i = 0; i < 4; ++i) {
                                const std::string label = "P" + std::to_string(i + 1);
                                if(ImGui::Selectable(label.c_str(), controllerValue == i)) {
                                    m_netplayCoordinator.assignController(participant.id, static_cast<Netplay::PlayerSlot>(i));
                                }
                            }
                            ImGui::EndCombo();
                        }
                        if(participant.controllerRequestPending) {
                            ImGui::TextDisabled("Req P%u", static_cast<unsigned>(participant.requestedControllerSlot) + 1u);
                        }
                    }
                    else {
                        if(participant.controllerAssignment == Netplay::kObserverPlayerSlot) {
                            ImGui::TextUnformatted("None");
                        }
                        else {
                            ImGui::Text("P%u", static_cast<unsigned>(participant.controllerAssignment) + 1u);
                        }
                        if(participant.controllerRequestPending) {
                            ImGui::TextDisabled("Req P%u", static_cast<unsigned>(participant.requestedControllerSlot) + 1u);
                        }
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
                    if(participant.id == m_netplayCoordinator.localParticipantId()) {
                        ImGui::TextUnformatted("-");
                    } else {
                        if(participant.connected) {
                            ImGui::Text("%ums / %ums", participant.pingMs, participant.jitterMs);
                        } else if(participant.reconnectReserved) {
                            ImGui::TextDisabled("reserved");
                            ImGui::Text("%us left", participant.reservationSecondsRemaining);
                        } else {
                            ImGui::TextDisabled("reconnect");
                        }
                        if(participant.rollbackScheduledCount > 0 ||
                           participant.missingInputGapCount > 0 ||
                           participant.futureFrameMismatchCount > 0 ||
                           participant.confirmedFrameConflictCount > 0) {
                            ImGui::Text("R:%u G:%u F:%u C:%u",
                                        participant.rollbackScheduledCount,
                                        participant.missingInputGapCount,
                                        participant.futureFrameMismatchCount,
                                        participant.confirmedFrameConflictCount);
                            if(!participant.lastDecision.empty()) {
                                ImGui::TextUnformatted(participant.lastDecision.c_str());
                            }
                        }
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(participant.ready ? "Yes" : "No");
                    ImGui::TableNextColumn();
                    if(m_netplayCoordinator.isHosting() && participant.id != m_netplayCoordinator.localParticipantId()) {
                        if(participant.reconnectReserved) {
                            std::string buttonId = "Remove##" + std::to_string(participant.id);
                            if(ImGui::SmallButton(buttonId.c_str())) {
                                m_netplayCoordinator.removeReconnectReservation(participant.id);
                            }
                        } else if(participant.controllerRequestPending) {
                            std::string approveId = "Approve##" + std::to_string(participant.id);
                            if(ImGui::SmallButton(approveId.c_str())) {
                                m_netplayCoordinator.approveControllerRequest(participant.id);
                            }
                            ImGui::SameLine();
                            std::string denyId = "Deny##" + std::to_string(participant.id);
                            if(ImGui::SmallButton(denyId.c_str())) {
                                m_netplayCoordinator.denyControllerRequest(participant.id);
                            }
                        } else {
                            std::string buttonId = "Kick##" + std::to_string(participant.id);
                            if(ImGui::SmallButton(buttonId.c_str())) {
                                m_netplayCoordinator.kickParticipant(participant.id);
                            }
                        }
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                }

                ImGui::EndTable();
            }

            ImGui::Separator();
            if(ImGui::BeginChild("NetplayLog", ImVec2(0.0f, 180.0f), true)) {
                for(const std::string& line : m_netplayCoordinator.eventLog()) {
                    ImGui::TextWrapped("%s", line.c_str());
                }
                if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }
#endif

    if(m_showNetplayDiagnosticsWindow) {
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin("Netplay Diagnostics", &m_showNetplayDiagnosticsWindow, ImGuiWindowFlags_NoResize)) {
            auto diag = m_emu.getNetplayDiagnostics();

            int rollbackWindowFrames = AppSettings::instance().data.netplay.rollbackWindowFrames;
            ImGui::SetNextItemWidth(120.0f);
            if(ImGui::InputInt("Rollback Window (frames)", &rollbackWindowFrames)) {
                rollbackWindowFrames = std::max(0, rollbackWindowFrames);
                AppSettings::instance().data.netplay.rollbackWindowFrames = rollbackWindowFrames;
                m_emu.configureNetplaySnapshots(static_cast<size_t>(rollbackWindowFrames));
            }

            ImGui::Separator();
            ImGui::Text("Enabled: %s", diag.enabled ? "Yes" : "No");
            ImGui::Text("Current Frame: %u", diag.currentFrame);
            ImGui::Text("Snapshot Capacity: %zu", diag.snapshotCapacity);
            ImGui::Text("Stored Snapshots: %zu", diag.storedSnapshots);
            ImGui::Text("Latest Snapshot CRC32: %08X", diag.latestSnapshotCrc32);

            ImGui::Separator();
            ImGui::Text("Rollback Count: %u", diag.rollbackStats.rollbackCount);
            ImGui::Text("Max Rollback Distance: %u", diag.rollbackStats.maxRollbackDistance);
            ImGui::Text("Prediction Hits: %u", diag.rollbackStats.predictionHitCount);
            ImGui::Text("Prediction Misses: %u", diag.rollbackStats.predictionMissCount);
            ImGui::Text("Hard Resync Count: %u", diag.rollbackStats.hardResyncCount);
            ImGui::Text("Last Rollback: %u -> %u", diag.rollbackStats.lastRollbackFromFrame, diag.rollbackStats.lastRollbackToFrame);
        }

        ImGui::End();
    }

    if(m_showRomDatabaseWindow) {
        ImGui::SetNextWindowSize(ImVec2(720, 0), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Rom Database");
    }

    if(ImGui::BeginPopupModal("Rom Database", &m_showRomDatabaseWindow)) {
        if(!m_romDbEditor.loaded) {
            ImGui::TextWrapped("%s", m_romDbEditor.statusMessage.c_str());
        }
        else {
                ImVec4 color = m_romDbEditor.foundInDatabase ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
                ImGui::TextColored(color, "%s", m_romDbEditor.statusMessage.c_str());
                ImGui::Separator();

                const bool compareWithSaved = m_romDbEditor.foundInDatabase;
                auto isChanged = [&](const std::string& a, const std::string& b) {
                    return compareWithSaved && a != b;
                };
                auto pushChangedStyle = [](bool changed) {
                    if(!changed) return;
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.08f, 0.08f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.55f, 0.16f, 0.16f, 1.0f));
                };
                auto popChangedStyle = [](bool changed) {
                    if(changed) ImGui::PopStyleColor(3);
                };

                auto drawStringEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<const char*, const char*>>& options, bool changed) {
                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(value == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

                    pushChangedStyle(changed);
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
                    popChangedStyle(changed);
                };

                auto drawIntEnumCombo = [&](const char* id, std::string& value, const std::vector<std::pair<int, const char*>>& options, bool changed) {
                    int parsed = 0;
                    try { parsed = value.empty() ? options.front().first : std::stoi(value); } catch(...) { parsed = options.front().first; }

                    int current = 0;
                    for(size_t i = 0; i < options.size(); ++i) {
                        if(parsed == options[i].first) {
                            current = static_cast<int>(i);
                            break;
                        }
                    }

                    pushChangedStyle(changed);
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
                    popChangedStyle(changed);
                };

                bool ch = false;
                ch = isChanged(m_romDbEditor.PrgChrCrc32, m_romDbSaved.PrgChrCrc32);
                pushChangedStyle(ch);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("PrgChrCrc32");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText("##PrgChrCrc32", &m_romDbEditor.PrgChrCrc32, ImGuiInputTextFlags_ReadOnly);
                popChangedStyle(ch);

                auto drawLabelCell = [&](const char* label) {
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                };
                auto drawInputTextCell = [&](const char* id, std::string& value, const std::string& savedValue) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    bool changed = isChanged(value, savedValue);
                    pushChangedStyle(changed);
                    ImGui::InputText(id, &value);
                    popChangedStyle(changed);
                };
                auto drawStringEnumCell = [&](const char* id, std::string& value, const std::string& savedValue, const std::vector<std::pair<const char*, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawStringEnumCombo(id, value, options, isChanged(value, savedValue));
                };
                auto drawIntEnumCell = [&](const char* id, std::string& value, const std::string& savedValue, const std::vector<std::pair<int, const char*>>& options) {
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    drawIntEnumCombo(id, value, options, isChanged(value, savedValue));
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
                    drawLabelCell("System");
                    drawStringEnumCell("##System", m_romDbEditor.System, m_romDbSaved.System, {
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
                    drawLabelCell("Mapper");
                    drawInputTextCell("##Mapper", m_romDbEditor.Mapper, m_romDbSaved.Mapper);

                    ImGui::TableNextRow();
                    drawLabelCell("Board");
                    drawInputTextCell("##Board", m_romDbEditor.Board, m_romDbSaved.Board);
                    drawLabelCell("SubMapperId");
                    drawInputTextCell("##SubMapperId", m_romDbEditor.SubMapperId, m_romDbSaved.SubMapperId);

                    ImGui::TableNextRow();
                    drawLabelCell("PCB");
                    drawInputTextCell("##PCB", m_romDbEditor.PCB, m_romDbSaved.PCB);
                    drawLabelCell("Mirroring");
                    drawStringEnumCell("##Mirroring", m_romDbEditor.Mirroring, m_romDbSaved.Mirroring, {
                        {"", "Default"},
                        {"h", "Horizontal"},
                        {"v", "Vertical"},
                        {"4", "Four Screen"},
                        {"0", "Single Screen A"},
                        {"1", "Single Screen B"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("Chip");
                    drawInputTextCell("##Chip", m_romDbEditor.Chip, m_romDbSaved.Chip);
                    drawLabelCell("InputType");
                    drawIntEnumCell("##InputType", m_romDbEditor.InputType, m_romDbSaved.InputType, {
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
                    drawLabelCell("PrgRomSize");
                    drawInputTextCell("##PrgRomSize", m_romDbEditor.PrgRomSize, m_romDbSaved.PrgRomSize);
                    drawLabelCell("VsSystemType");
                    drawIntEnumCell("##VsSystemType", m_romDbEditor.VsSystemType, m_romDbSaved.VsSystemType, {
                        {0, "Default"},
                        {1, "RbiBaseballProtection"},
                        {2, "TkoBoxingProtection"},
                        {3, "SuperXeviousProtection"},
                        {4, "IceClimberProtection"},
                        {5, "VsDualSystem"},
                        {6, "RaidOnBungelingBayProtection"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRomSize");
                    drawInputTextCell("##ChrRomSize", m_romDbEditor.ChrRomSize, m_romDbSaved.ChrRomSize);
                    drawLabelCell("VsPpuModel");
                    drawIntEnumCell("##VsPpuModel", m_romDbEditor.VsPpuModel, m_romDbSaved.VsPpuModel, {
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
                    drawLabelCell("SaveRamSize");
                    drawInputTextCell("##SaveRamSize", m_romDbEditor.SaveRamSize, m_romDbSaved.SaveRamSize);
                    drawLabelCell("BusConflicts");
                    drawStringEnumCell("##BusConflicts", m_romDbEditor.BusConflicts, m_romDbSaved.BusConflicts, {
                        {"", "Default"},
                        {"Y", "Yes"},
                        {"N", "No"},
                    });

                    ImGui::TableNextRow();
                    drawLabelCell("ChrRamSize");
                    drawInputTextCell("##ChrRamSize", m_romDbEditor.ChrRamSize, m_romDbSaved.ChrRamSize);
                    drawEmptyPair();

                    ImGui::TableNextRow();
                    drawLabelCell("WorkRamSize");
                    drawInputTextCell("##WorkRamSize", m_romDbEditor.WorkRamSize, m_romDbSaved.WorkRamSize);
                    drawLabelCell("HasBattery");
                    ImGui::TableNextColumn();
                    {
                        bool hasBattery = m_romDbEditor.HasBattery == "1";
                        const bool changed = isChanged(m_romDbEditor.HasBattery, m_romDbSaved.HasBattery);
                        pushChangedStyle(changed);
                        if(ImGui::Checkbox("##HasBattery", &hasBattery)) {
                            m_romDbEditor.HasBattery = hasBattery ? "1" : "0";
                        }
                        popChangedStyle(changed);
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

#ifdef ENABLE_NSF_PLAYER
inline void GeraNESApp::drawNsfPlayerVisualizer()
{
    m_nsfVisualizer.draw(
        m_audioOutput.getRecentMixedSamples(),
        m_audioOutput.outputSampleRate(),
        m_menuBarHeight,
        width(),
        height(),
        m_emu.nsfCurrentSong(),
        m_emu.nsfTotalSongs(),
        m_emu.nsfIsPlaying(),
        m_emu.nsfIsPaused(),
        m_emu.nsfHasEnded(),
        m_fontNsfTitle,
        m_fontNsfSubtitle
    );
}
#endif

inline void GeraNESApp::showOverlay()
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if(AppSettings::instance().data.debug.showFps) {
        const int fontSize = 32;
        ImFont* fpsFont = m_fontFps != nullptr ? m_fontFps : ImGui::GetFont();

        std::string fpsText = std::to_string(m_fps);
        ImVec2 fpsTextSize = fpsFont->CalcTextSizeA(fontSize, FLT_MAX, 0, fpsText.c_str());

        const ImVec2 pos = ImVec2(width() - fpsTextSize.x - 32, 40);

        DrawTextOutlined(drawList, fpsFont, fontSize, pos, 0xFFFFFFFF, 0xFF000000, fpsText.c_str());
    }
    m_userToast.draw(drawList, static_cast<float>(width()), static_cast<float>(height()), m_fontToast);

    m_touch->draw(drawList);
}
