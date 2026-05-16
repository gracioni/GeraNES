#include "GeraNESNetplay/NetplayWindowUI.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "GeraNESApp/AppSettings.h"
#include "GeraNESApp/EmscriptenUtil.h"
#include "GeraNESApp/ImGuiTheme.h"
#include "GeraNESApp/IEmulationHost.h"
#include "GeraNESNetplay/GeraNESNetplayAdapters.h"
#include "GeraNESNetplay/GeraNESNetplayAssignmentHelpers.h"
#include "ConsoleNetplay/NetplayInputAssignment.h"
#include "ConsoleNetplay/WebRtcSignaling.h"
#include "ConsoleNetplay/WebRtcSignalingClient.h"
#include "GeraNESApp/imgui_util.h"
#include "imgui.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

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

struct WebRtcRoomBrowserUiState
{
    bool windowOpen = false;
    bool requestOpenWindow = false;
    bool requestPasswordPrompt = false;
    bool fetching = false;
    bool waitingForConnected = false;
    bool requestSent = false;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::string statusText;
    std::string browserPeerId;
    std::string browserRoomId;
    std::string pendingRoomId;
    std::string pendingPassword;
    std::vector<WebRtcSignalingRoomInfo> rooms;
    int selectedRoomIndex = -1;
    uint64_t nextPeerNonce = 1;
    std::unique_ptr<IWebRtcSignalingClient> client;

    void disconnect()
    {
        if(client) {
            client->disconnect();
            client.reset();
        }
        fetching = false;
        waitingForConnected = false;
        requestSent = false;
        deadline.reset();
        browserPeerId.clear();
        browserRoomId.clear();
    }
};

struct PendingConnectOperationUiState
{
    enum class Type
    {
        None,
        CreateRoom,
        JoinRoom
    };

    Type type = Type::None;
    bool cancelRequested = false;
    bool requestOpenModal = false;
    bool modalOpen = false;
    bool sawAttemptInFlight = false;
    std::chrono::steady_clock::time_point startedAt = {};
    std::string initialErrorText;
    std::string statusText;

    bool active() const
    {
        return type != Type::None;
    }

    void begin(Type operationType, std::string message)
    {
        type = operationType;
        cancelRequested = false;
        requestOpenModal = false;
        modalOpen = false;
        sawAttemptInFlight = false;
        startedAt = std::chrono::steady_clock::now();
        initialErrorText.clear();
        statusText = std::move(message);
    }

    void clear()
    {
        type = Type::None;
        cancelRequested = false;
        requestOpenModal = false;
        modalOpen = false;
        sawAttemptInFlight = false;
        initialErrorText.clear();
        statusText.clear();
    }
};

inline WebRtcRoomBrowserUiState& webRtcRoomBrowserUiState()
{
    static WebRtcRoomBrowserUiState state;
    return state;
}

inline PendingConnectOperationUiState& pendingConnectOperationUiState()
{
    static PendingConnectOperationUiState state;
    return state;
}

void drawNetplayWindow(bool& showWindow,
                       NetplayAppRuntime& runtime,
                       const ImVec2& viewportCenter)
{
    WebRtcRoomBrowserUiState& roomBrowser = webRtcRoomBrowserUiState();
    PendingConnectOperationUiState& pendingOperation = pendingConnectOperationUiState();
    if(!showWindow) {
        roomBrowser.windowOpen = false;
        roomBrowser.requestOpenWindow = false;
        roomBrowser.requestPasswordPrompt = false;
        roomBrowser.disconnect();
        pendingOperation.clear();
        return;
    }

    auto& cfg = AppSettings::instance().data.netplay;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workSize = viewport != nullptr ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    const float maxInitialHeight = std::max(80.0f, workSize.y - 16.0f);
    const float estimatedInitialHeight = 610.0f;
    SetNextWindowSizeClamped(
        ImVec2(760.0f, std::min(estimatedInitialHeight, maxInitialHeight)),
        ImGuiCond_FirstUseEver,
        16.0f,
        ImVec2(160.0f, 80.0f)
    );
    SetNextWindowSizeConstraintsClamped(
        ImVec2(620.0f, 420.0f),
        ImVec2(FLT_MAX, maxInitialHeight),
        16.0f,
        ImVec2(160.0f, 80.0f)
    );
    ImGui::SetNextWindowPos(viewportCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(!ImGui::Begin("Netplay", &showWindow)) {
        ImGui::End();
        return;
    }

    const auto snapshot = runtime.uiSnapshot();
    const bool active = snapshot.active;
    const bool showConnectedControls = snapshot.hosting || snapshot.connected;
    const bool connecting = active && !snapshot.hosting && !snapshot.connected && !snapshot.reconnecting;
    const auto now = std::chrono::steady_clock::now();
    const auto& room = snapshot.room;
    const bool canHost = snapshot.localRomLoaded;
    auto availableBackends = availableNetTransportBackends();
    int configuredBackend = std::clamp(cfg.transportBackend, 0, 1);
    const bool configuredBackendSupported =
        std::find(availableBackends.begin(),
                  availableBackends.end(),
                  static_cast<NetTransportBackend>(configuredBackend)) != availableBackends.end();
    if(!configuredBackendSupported) {
        configuredBackend = static_cast<int>(defaultNetTransportBackend());
        cfg.transportBackend = configuredBackend;
        runtime.setTransportBackend(static_cast<NetTransportBackend>(configuredBackend));
    }
    const auto drawBackendSelector = [&]() {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::BeginDisabled(active || showConnectedControls);
        if(ImGui::BeginCombo("Backend##NetplayBackend",
                             netTransportBackendLabel(static_cast<NetTransportBackend>(configuredBackend)))) {
            for(NetTransportBackend backend : availableBackends) {
                const bool selected = configuredBackend == static_cast<int>(backend);
                if(ImGui::Selectable(netTransportBackendLabel(backend), selected)) {
                    cfg.transportBackend = static_cast<int>(backend);
                    runtime.setTransportBackend(backend);
                }
                if(selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
    };

    const auto drawWebRtcConfig = [&](bool hostMode) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("Room Id##NetplaySignalingRoomId", &cfg.signalingRoomId);
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText(hostMode ? "Room Password##NetplayHostRoomPassword" : "Room Password##NetplayJoinRoomPassword",
                         &cfg.signalingPassword);
#ifdef __EMSCRIPTEN__
        cfg.useEmbeddedSignalingServer = false;
        ImGui::TextWrapped("Web builds require an external WebRTC signaling server URL.");
#else
        if(hostMode) {
            ImGui::Checkbox("Start Embedded Signaling Server##NetplayEmbeddedSignaling", &cfg.useEmbeddedSignalingServer);
        }
#endif

        if(hostMode && cfg.useEmbeddedSignalingServer) {
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Signaling Port##NetplayEmbeddedSignalingPort", &cfg.embeddedSignalingPort);
            cfg.embeddedSignalingPort = std::clamp(cfg.embeddedSignalingPort, 1, 65535);
        } else {
            ImGui::SetNextItemWidth(320.0f);
            ImGui::InputText("Signaling URL##NetplaySignalingUrl", &cfg.signalingUrl);
        }

        const std::string effectiveSignalingUrl =
            hostMode && cfg.useEmbeddedSignalingServer
                ? buildWebRtcSignalingUrl("127.0.0.1", static_cast<uint16_t>(cfg.embeddedSignalingPort))
                : cfg.signalingUrl;
        const WebRtcSignalingConfig signalingConfig{
            effectiveSignalingUrl,
            cfg.signalingRoomId,
            cfg.signalingPassword
        };

        if(hostMode && cfg.useEmbeddedSignalingServer) {
            ImGui::TextWrapped("The owner will start the embedded signaling server on port %d.", cfg.embeddedSignalingPort);
            ImGui::TextWrapped("Leave the password empty to create a public room.");
        }
        if(!signalingConfig.valid()) {
            ImGui::TextColored(ImGuiTheme::accentActive(), "Configure signaling URL and room id for WebRTC.");
        } else if(!(hostMode && cfg.useEmbeddedSignalingServer)) {
            ImGui::TextWrapped("Manual signaling will use %s (room %s).", cfg.signalingUrl.c_str(), cfg.signalingRoomId.c_str());
        }
    };

    const auto applyWebRtcTransportOptions = [&](bool hostMode, const std::string& password) {
        NetTransportOptions transportOptions;
        transportOptions.useEmbeddedWebRtcSignalingServer = hostMode && cfg.useEmbeddedSignalingServer;
        transportOptions.embeddedWebRtcSignalingPort =
            static_cast<uint16_t>(std::clamp(cfg.embeddedSignalingPort, 1, 65535));
        transportOptions.webRtcSignaling = WebRtcSignalingConfig{cfg.signalingUrl, cfg.signalingRoomId, password};
        runtime.setTransportOptions(transportOptions);
    };

    const auto beginPendingOperation = [&](PendingConnectOperationUiState::Type type, const char* statusText) {
        if(pendingOperation.active()) {
            return false;
        }
        pendingOperation.begin(type, statusText);
        pendingOperation.initialErrorText = snapshot.lastError;
        return true;
    };

    const auto startJoinFromCurrentConfig = [&]() {
        if(!beginPendingOperation(PendingConnectOperationUiState::Type::JoinRoom, "Joining room...")) {
            return;
        }
        const bool joinUsingWebRtc =
            static_cast<NetTransportBackend>(configuredBackend) == NetTransportBackend::WebRTC;
        if(joinUsingWebRtc) {
            applyWebRtcTransportOptions(false, cfg.signalingPassword);
        }
        runtime.join(joinUsingWebRtc ? std::string{} : cfg.hostName,
                     joinUsingWebRtc ? 0 : static_cast<uint16_t>(cfg.port),
                     cfg.displayName);
    };

    const auto refreshRoomList = [&]() {
        roomBrowser.disconnect();
        roomBrowser.rooms.clear();
        roomBrowser.selectedRoomIndex = -1;
        roomBrowser.pendingRoomId.clear();
        roomBrowser.pendingPassword.clear();
        roomBrowser.statusText.clear();

        WebRtcSignalingClientOptions options;
        options.config = WebRtcSignalingConfig{cfg.signalingUrl, "__room_browser__", {}};
        options.localPeerId = "room-browser-" + std::to_string(roomBrowser.nextPeerNonce++);
        roomBrowser.browserPeerId = options.localPeerId;
        roomBrowser.browserRoomId = options.config.roomId;
        roomBrowser.client = createWebRtcSignalingClient();
        if(roomBrowser.client == nullptr) {
            roomBrowser.statusText = "WebRTC room browser is unavailable.";
            return;
        }
        if(!roomBrowser.client->connect(options)) {
            roomBrowser.statusText = roomBrowser.client->lastError();
            roomBrowser.client.reset();
            return;
        }

        roomBrowser.fetching = true;
        roomBrowser.waitingForConnected = true;
        roomBrowser.requestSent = false;
        roomBrowser.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        roomBrowser.statusText = "Fetching rooms...";
    };

    const auto sendRoomBrowserHandshake = [&]() {
        if(roomBrowser.client == nullptr || roomBrowser.requestSent) {
            return true;
        }

        WebRtcSignalingMessage hello;
        hello.type = WebRtcSignalType::Hello;
        hello.peerId = roomBrowser.browserPeerId;
        hello.roomId = roomBrowser.browserRoomId;
        if(!roomBrowser.client->send(hello)) {
            roomBrowser.statusText = roomBrowser.client->lastError();
            roomBrowser.disconnect();
            return false;
        }

        WebRtcSignalingMessage request;
        request.type = WebRtcSignalType::RoomList;
        if(!roomBrowser.client->send(request)) {
            roomBrowser.statusText = roomBrowser.client->lastError();
            roomBrowser.disconnect();
            return false;
        }

        roomBrowser.requestSent = true;
        return true;
    };

    if(pendingOperation.active() && (active || connecting || snapshot.reconnecting)) {
        pendingOperation.sawAttemptInFlight = true;
    }
    const bool hasNewError =
        !snapshot.lastError.empty() && snapshot.lastError != pendingOperation.initialErrorText;
    const bool operationSucceeded =
        (pendingOperation.type == PendingConnectOperationUiState::Type::CreateRoom && snapshot.hosting) ||
        (pendingOperation.type == PendingConnectOperationUiState::Type::JoinRoom && snapshot.connected);
    const bool operationFailed =
        pendingOperation.active() &&
        !snapshot.lastError.empty() &&
        !connecting &&
        !snapshot.reconnecting &&
        !snapshot.hosting &&
        !snapshot.connected &&
        (pendingOperation.sawAttemptInFlight || hasNewError);
    const bool operationCancelledCompleted =
        pendingOperation.active() &&
        pendingOperation.cancelRequested &&
        !active &&
        !connecting &&
        !snapshot.reconnecting &&
        !snapshot.hosting &&
        !snapshot.connected;
    const bool operationCompleted = operationSucceeded || operationFailed || operationCancelledCompleted;
    if(operationCompleted) {
        pendingOperation.clear();
    } else if(pendingOperation.active() &&
              !pendingOperation.modalOpen &&
              (now - pendingOperation.startedAt) >= std::chrono::milliseconds(100)) {
        pendingOperation.requestOpenModal = true;
        pendingOperation.modalOpen = true;
    }

    constexpr const char* kNetplayOperationPopupId = "##NetplayOperationModal";
    if(pendingOperation.requestOpenModal) {
        ImGui::OpenPopup(kNetplayOperationPopupId);
        pendingOperation.requestOpenModal = false;
    } else if(pendingOperation.active() &&
              pendingOperation.modalOpen &&
              !ImGui::IsPopupOpen(kNetplayOperationPopupId)) {
        ImGui::OpenPopup(kNetplayOperationPopupId);
    }

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Display Name##NetplayDisplayName", &cfg.displayName);
    const bool usingWebRtc = static_cast<NetTransportBackend>(configuredBackend) == NetTransportBackend::WebRTC;
    const bool blockInputs = pendingOperation.active();
    if(blockInputs) {
        ImGui::BeginDisabled();
    }
    if(roomBrowser.fetching && roomBrowser.client) {
        if(roomBrowser.waitingForConnected && roomBrowser.client->isConnected()) {
            roomBrowser.waitingForConnected = false;
            sendRoomBrowserHandshake();
        }

        for(const auto& event : roomBrowser.client->poll()) {
            switch(event.type) {
                case IWebRtcSignalingClient::Event::Type::Connected:
                    if(roomBrowser.waitingForConnected) {
                        roomBrowser.waitingForConnected = false;
                        sendRoomBrowserHandshake();
                    }
                    break;
                case IWebRtcSignalingClient::Event::Type::Message:
                    if(event.message.type == WebRtcSignalType::RoomList) {
                        roomBrowser.rooms = event.message.rooms;
                        std::sort(roomBrowser.rooms.begin(), roomBrowser.rooms.end(),
                                  [](const WebRtcSignalingRoomInfo& lhs, const WebRtcSignalingRoomInfo& rhs) {
                                      return lhs.roomId < rhs.roomId;
                                  });
                        if(roomBrowser.rooms.empty()) {
                            roomBrowser.statusText = "No active rooms.";
                        } else {
                            roomBrowser.statusText.clear();
                            if(roomBrowser.selectedRoomIndex < 0 ||
                               roomBrowser.selectedRoomIndex >= static_cast<int>(roomBrowser.rooms.size())) {
                                roomBrowser.selectedRoomIndex = 0;
                            }
                        }
                        roomBrowser.disconnect();
                    } else if(event.message.type == WebRtcSignalType::Error) {
                        roomBrowser.statusText = event.message.error.empty() ? "Failed to fetch rooms." : event.message.error;
                        roomBrowser.disconnect();
                    }
                    break;
                case IWebRtcSignalingClient::Event::Type::Error:
                    roomBrowser.statusText = event.text.empty() ? "Failed to fetch rooms." : event.text;
                    roomBrowser.disconnect();
                    break;
                case IWebRtcSignalingClient::Event::Type::Disconnected:
                    if(roomBrowser.fetching) {
                        roomBrowser.statusText = "Disconnected while fetching rooms.";
                        roomBrowser.disconnect();
                    }
                    break;
                default:
                    break;
            }
        }

        if(roomBrowser.fetching &&
           roomBrowser.deadline.has_value() &&
           std::chrono::steady_clock::now() >= *roomBrowser.deadline) {
            roomBrowser.statusText = roomBrowser.requestSent
                ? "Timed out while waiting for room list."
                : "Timed out while connecting to signaling server.";
            roomBrowser.disconnect();
        }
    }
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

    cfg.maxPeers = std::clamp(cfg.maxPeers, 1, 32);

    if(snapshot.reconnecting) {
        ImGui::TextColored(
            ImGuiTheme::accentActive(),
            "Reconnecting to host... %us",
            static_cast<unsigned>(snapshot.reconnectSecondsRemaining)
        );
        ImGui::SameLine();
        if(ImGui::Button("Cancel##NetplayCancelReconnectButton")) {
            runtime.disconnect();
        }
    } else if(connecting) {
        if(usingWebRtc) {
            ImGui::TextColored(
                ImGuiTheme::info(),
                "Joining room %s via %s...",
                cfg.signalingRoomId.c_str(),
                cfg.signalingUrl.c_str()
            );
        } else {
            ImGui::TextColored(
                ImGuiTheme::info(),
                "Trying to connect to %s:%d...",
                cfg.hostName.c_str(),
                cfg.port
            );
        }
        if(ImGui::Button("Cancel##NetplayCancelConnectButton")) {
            runtime.disconnect();
        }
    } else if(!active || !showConnectedControls) {
        if(ImGui::BeginTabBar("NetplayEntryTabs")) {
            if(ImGui::BeginTabItem("Owner")) {
                ImGui::TextDisabled("Create a room and wait for participants.");
                drawBackendSelector();
                if(usingWebRtc) {
                    drawWebRtcConfig(true);
                } else {
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::InputInt("Port##NetplayHostPort", &cfg.port);
                    cfg.port = std::clamp(cfg.port, 1, 65535);
                }
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputInt("Max Peers##NetplayMaxPeers", &cfg.maxPeers);
                cfg.maxPeers = std::clamp(cfg.maxPeers, 1, 32);
                ImGui::BeginDisabled(!canHost);
                if(ImGui::Button("Create Room##NetplayHostButton")) {
                    if(beginPendingOperation(PendingConnectOperationUiState::Type::CreateRoom, "Creating room...")) {
                        if(usingWebRtc) {
                            applyWebRtcTransportOptions(true, cfg.signalingPassword);
                        }
                        runtime.host(usingWebRtc ? 0 : static_cast<uint16_t>(cfg.port),
                                     static_cast<size_t>(cfg.maxPeers),
                                     cfg.displayName);
                    }
                }
                ImGui::EndDisabled();
                if(!canHost) {
                    ImGui::TextColored(ImGuiTheme::accentActive(), "Load a ROM before creating a room.");
                }
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Participant")) {
                ImGui::TextDisabled("Connect to an existing room.");
                drawBackendSelector();
                if(usingWebRtc) {
                    drawWebRtcConfig(false);
                } else {
                    ImGui::SetNextItemWidth(220.0f);
                    ImGui::InputText("Owner##NetplayHostName", &cfg.hostName);
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::InputInt("Port##NetplayJoinPort", &cfg.port);
                    cfg.port = std::clamp(cfg.port, 1, 65535);
                }
                if(ImGui::Button("Join Room##NetplayJoinButton")) {
                    startJoinFromCurrentConfig();
                }
                if(usingWebRtc) {
                    ImGui::SameLine();
                    if(ImGui::Button("Room List##NetplayRoomListButton")) {
                        roomBrowser.windowOpen = true;
                        roomBrowser.requestOpenWindow = true;
                        refreshRoomList();
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    } else {
        if(ImGui::Button("Disconnect##NetplayDisconnectButton")) {
            runtime.disconnect();
        }
    }

    if(roomBrowser.requestOpenWindow) {
        ImGui::OpenPopup("WebRTC Room List");
        roomBrowser.requestOpenWindow = false;
    }
    if(roomBrowser.requestPasswordPrompt) {
        ImGui::OpenPopup("Join Protected Room");
        roomBrowser.requestPasswordPrompt = false;
    }

    bool roomListWindowOpen = roomBrowser.windowOpen;
    if(ImGui::BeginPopupModal("WebRTC Room List", &roomListWindowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        roomBrowser.windowOpen = roomListWindowOpen;
        ImGui::TextWrapped("Rooms from %s", cfg.signalingUrl.c_str());
        ImGui::Spacing();

        if(ImGui::Button("Refresh##NetplayRoomListRefresh")) {
            refreshRoomList();
        }
        ImGui::SameLine();
        const bool hasSelection =
            roomBrowser.selectedRoomIndex >= 0 &&
            roomBrowser.selectedRoomIndex < static_cast<int>(roomBrowser.rooms.size());
        ImGui::BeginDisabled(!hasSelection);
        if(ImGui::Button("Join Selected##NetplayRoomListJoin")) {
            const auto& selectedRoom = roomBrowser.rooms[static_cast<size_t>(roomBrowser.selectedRoomIndex)];
            cfg.signalingRoomId = selectedRoom.roomId;
            if(selectedRoom.passwordProtected) {
                roomBrowser.pendingRoomId = selectedRoom.roomId;
                roomBrowser.pendingPassword.clear();
                roomBrowser.requestPasswordPrompt = true;
            } else {
                cfg.signalingPassword.clear();
                startJoinFromCurrentConfig();
                roomBrowser.windowOpen = false;
                roomListWindowOpen = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Close##NetplayRoomListClose")) {
            roomBrowser.windowOpen = false;
            roomListWindowOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if(!roomBrowser.statusText.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", roomBrowser.statusText.c_str());
        }

        const float roomTableHeight = ImGui::GetTextLineHeightWithSpacing() * 10.0f;
        if(ImGui::BeginTable("NetplayRoomListTable",
                             2,
                             ImGuiTableFlags_Borders |
                                 ImGuiTableFlags_RowBg |
                                 ImGuiTableFlags_ScrollY |
                                 ImGuiTableFlags_SizingStretchProp,
                             ImVec2(520.0f, roomTableHeight))) {
            ImGui::TableSetupColumn("Room");
            ImGui::TableSetupColumn("Has Password", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGuiTheme::textOnAccent());
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            for(size_t roomIndex = 0; roomIndex < roomBrowser.rooms.size(); ++roomIndex) {
                const auto& listedRoom = roomBrowser.rooms[roomIndex];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const bool selected = roomBrowser.selectedRoomIndex == static_cast<int>(roomIndex);
                if(ImGui::Selectable((listedRoom.roomId + "##NetplayRoom" + std::to_string(roomIndex)).c_str(),
                                     selected,
                                     ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    roomBrowser.selectedRoomIndex = static_cast<int>(roomIndex);
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        cfg.signalingRoomId = listedRoom.roomId;
                        if(listedRoom.passwordProtected) {
                            roomBrowser.pendingRoomId = listedRoom.roomId;
                            roomBrowser.pendingPassword.clear();
                            roomBrowser.requestPasswordPrompt = true;
                        } else {
                            cfg.signalingPassword.clear();
                            startJoinFromCurrentConfig();
                            roomBrowser.windowOpen = false;
                            roomListWindowOpen = false;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(listedRoom.passwordProtected ? "Yes" : "No");
            }

            ImGui::EndTable();
        }

        ImGui::EndPopup();
    } else if(roomBrowser.windowOpen) {
        roomBrowser.windowOpen = false;
        roomBrowser.disconnect();
    }

    if(ImGui::BeginPopupModal("Join Protected Room", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Enter the password for room %s.", roomBrowser.pendingRoomId.c_str());
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputText("Password##NetplayRoomJoinPassword", &roomBrowser.pendingPassword, ImGuiInputTextFlags_Password);
        if(ImGui::Button("Join##NetplayPasswordJoin")) {
            cfg.signalingRoomId = roomBrowser.pendingRoomId;
            cfg.signalingPassword = roomBrowser.pendingPassword;
            roomBrowser.windowOpen = false;
            roomBrowser.pendingPassword.clear();
            ImGui::CloseCurrentPopup();
            startJoinFromCurrentConfig();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel##NetplayPasswordCancel")) {
            roomBrowser.pendingPassword.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    if(ImGui::CollapsingHeader("Connection##NetplayConnection")) {
        ImGui::Text("Transport Active: %s", active ? "Yes" : "No");
        ImGui::Text("Backend: %s", netTransportBackendLabel(snapshot.transportBackend));
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
        ImGui::TextColored(ImGuiTheme::error(), "%s", snapshot.lastError.c_str());
    }

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
        int rollbackWindowFrames = AppSettings::instance().data.netplay.rollbackWindowFrames;
        ImGui::SetNextItemWidth(120.0f);
        if(ImGui::InputInt("Rollback Window (frames)##NetplayRollbackWindow", &rollbackWindowFrames)) {
            rollbackWindowFrames = std::max(0, rollbackWindowFrames);
            AppSettings::instance().data.netplay.rollbackWindowFrames = rollbackWindowFrames;
            runtime.configureRollbackWindow(static_cast<size_t>(rollbackWindowFrames));
        }

        ImGui::Separator();
        ImGui::Text("Diagnostics Enabled: %s", snapshot.runtimeDiagnostics.enabled ? "Yes" : "No");
        ImGui::Text("Current Frame: %u", snapshot.runtimeDiagnostics.currentFrame);
        ImGui::Text("Snapshot Capacity: %zu", snapshot.runtimeDiagnostics.snapshotCapacity);
        ImGui::Text("Stored Snapshots: %zu", snapshot.runtimeDiagnostics.storedSnapshots);
        ImGui::Text("Latest Snapshot CRC32: %08X", snapshot.runtimeDiagnostics.latestSnapshotCrc32);
        const auto drawTimingStats = [](const char* label,
                                        const IEmulationHost::NetplayDiagnosticsSnapshot::TimingStats& stats) {
            ImGui::Text("%s: count %llu, total %llu us, last %llu us, avg %llu us, max %llu us",
                        label,
                        static_cast<unsigned long long>(stats.count),
                        static_cast<unsigned long long>(stats.totalUs),
                        static_cast<unsigned long long>(stats.lastUs),
                        static_cast<unsigned long long>(stats.recentAverageUs),
                        static_cast<unsigned long long>(stats.maxUs));
        };
        const auto drawByteStats = [](const char* label,
                                      const IEmulationHost::NetplayDiagnosticsSnapshot::ByteStats& stats) {
            ImGui::Text("%s: count %llu, total %llu bytes, last %llu bytes, max %llu bytes",
                        label,
                        static_cast<unsigned long long>(stats.count),
                        static_cast<unsigned long long>(stats.totalBytes),
                        static_cast<unsigned long long>(stats.lastBytes),
                        static_cast<unsigned long long>(stats.maxBytes));
        };
        drawTimingStats("Netplay State Save", snapshot.runtimeDiagnostics.netplayStateSaveTiming);
        drawTimingStats("Rollback Snapshot Save", snapshot.runtimeDiagnostics.netplayRollbackSnapshotSaveTiming);
        drawTimingStats("Netplay CRC", snapshot.runtimeDiagnostics.netplayCrcTiming);
        drawTimingStats("Rollback Load", snapshot.runtimeDiagnostics.rollbackLoadTiming);
        drawByteStats("Netplay State Serialized", snapshot.runtimeDiagnostics.netplayStateSerializedBytes);
        drawByteStats("Rollback Snapshot Serialized", snapshot.runtimeDiagnostics.netplayRollbackSnapshotSerializedBytes);
        drawByteStats("Snapshot Lookup Copies", snapshot.runtimeDiagnostics.snapshotLookupCopyBytes);
        drawByteStats("Rollback Snapshot Copies", snapshot.runtimeDiagnostics.rollbackSnapshotCopyBytes);
        drawByteStats("Seeded Snapshot Copies", snapshot.runtimeDiagnostics.seededSnapshotCopyBytes);
        ImGui::Text("Frame Pacing: samples %llu, dt last/max %u/%u ms, advanced last/max %u/%u, catchup last/max %u/%u",
                    static_cast<unsigned long long>(snapshot.framePacingDiagnostics.sampleCount),
                    snapshot.framePacingDiagnostics.lastDtMs,
                    snapshot.framePacingDiagnostics.maxDtMs,
                    snapshot.framePacingDiagnostics.lastFramesAdvanced,
                    snapshot.framePacingDiagnostics.maxFramesAdvanced,
                    snapshot.framePacingDiagnostics.lastCatchupFrames,
                    snapshot.framePacingDiagnostics.maxCatchupFrames);
        ImGui::Text("Frame Pacing Totals: advanced %llu, catchup ticks %llu, netplay override %s, monitor cadence %s",
                    static_cast<unsigned long long>(snapshot.framePacingDiagnostics.totalFramesAdvanced),
                    static_cast<unsigned long long>(snapshot.framePacingDiagnostics.catchupTickCount),
                    snapshot.framePacingDiagnostics.netplayPacingOverrideActive ? "Yes" : "No",
                    snapshot.framePacingDiagnostics.presenterCadenceMatched ? "Yes" : "No");

        ImGui::Separator();
        ImGui::Text("Local Input Frames: %zu", snapshot.localInputCount);
        ImGui::Text("Remote Input Frames: %zu", snapshot.remoteInputCount);
        const auto drawTimelineLookupStats = [](const char* label, const InputTimeline::LookupStats& stats) {
            ImGui::Text("%s: find %llu, mutable %llu, hits %llu, misses %llu, scanned total %llu, last %llu, max %llu",
                        label,
                        static_cast<unsigned long long>(stats.findCalls),
                        static_cast<unsigned long long>(stats.findMutableCalls),
                        static_cast<unsigned long long>(stats.hits),
                        static_cast<unsigned long long>(stats.misses),
                        static_cast<unsigned long long>(stats.totalScannedEntries),
                        static_cast<unsigned long long>(stats.lastScannedEntries),
                        static_cast<unsigned long long>(stats.maxScannedEntries));
        };
        const auto drawLinearScanStats = [](const char* label, const NetplayCoordinator::LinearScanStats& stats) {
            ImGui::Text("%s: calls %llu, hits %llu, misses %llu, scanned total %llu, last %llu, max %llu",
                        label,
                        static_cast<unsigned long long>(stats.calls),
                        static_cast<unsigned long long>(stats.hits),
                        static_cast<unsigned long long>(stats.misses),
                        static_cast<unsigned long long>(stats.totalScannedEntries),
                        static_cast<unsigned long long>(stats.lastScannedEntries),
                        static_cast<unsigned long long>(stats.maxScannedEntries));
        };
        drawTimelineLookupStats("Local Timeline Lookup", snapshot.localInputLookupStats);
        drawTimelineLookupStats("Remote Timeline Lookup", snapshot.remoteInputLookupStats);
        drawLinearScanStats("Confirmed Frame Find", snapshot.coordinatorPerformanceDiagnostics.confirmedFrameFind);
        drawLinearScanStats("Confirmed Frame Store", snapshot.coordinatorPerformanceDiagnostics.confirmedFrameStore);
        ImGui::Text("Playback Queue Rebuilds: count %llu, total frames %llu, last %llu, max %llu, through %u",
                    static_cast<unsigned long long>(snapshot.playbackQueueStats.rebuildCount),
                    static_cast<unsigned long long>(snapshot.playbackQueueStats.totalBuiltFrames),
                    static_cast<unsigned long long>(snapshot.playbackQueueStats.lastBuiltFrames),
                    static_cast<unsigned long long>(snapshot.playbackQueueStats.maxBuiltFrames),
                    snapshot.playbackQueueStats.lastPreparedThroughFrame);
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
        ImGui::Text("Max Rollback Distance: %u", snapshot.runtimeDiagnostics.rollbackStats.maxRollbackDistance);
        ImGui::Text("Last Applied Rollback: %u -> %u",
                    snapshot.runtimeDiagnostics.rollbackStats.lastRollbackFromFrame,
                    snapshot.runtimeDiagnostics.rollbackStats.lastRollbackToFrame);
        const char* recoveryModeLabel = "Unknown";
        switch(room.recoveryInputMode) {
            case RecoveryInputMode::Normal: recoveryModeLabel = "Normal"; break;
            case RecoveryInputMode::ResyncLocked: recoveryModeLabel = "ResyncLocked"; break;
            case RecoveryInputMode::PostResyncStabilizing: recoveryModeLabel = "PostResyncStabilizing"; break;
            default: break;
        }
        ImGui::Text("Recovery Input Mode: %s", recoveryModeLabel);
        ImGui::Text("Dropped Inputs In Recovery: %u", room.inputsDroppedDuringRecovery);
        ImGui::Text("Stabilization Frames Remaining: %u", room.stabilizationFramesRemaining);
        ImGui::Text("Stabilization CRC Passes: %u", room.stabilizationCrcPassCount);
        ImGui::Text("Last Remote CRC: %08X @ frame %u", room.lastRemoteCrc32, room.lastRemoteCrcFrame);

        ImGui::Separator();
        ImGui::Text("Shared Clock: %s", room.sharedClockSynchronized ? "Synchronized" : "Unsynchronized");
        ImGui::Text("Local Shared Clock (us): %llu",
                    static_cast<unsigned long long>(room.sharedClockMicros));
        ImGui::Text("Clock Offset (us): %lld",
                    static_cast<long long>(room.sharedClockOffsetMicros));
        ImGui::Text("Clock RTT (us): %llu",
                    static_cast<unsigned long long>(room.sharedClockRttMicros));
        ImGui::Text("Last Authoritative Frame Clock: frame %u @ %llu us",
                    room.lastAuthoritativeClockFrame,
                    static_cast<unsigned long long>(room.lastAuthoritativeClockMicros));

        const uint64_t localNowMicros = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        ImGui::TextUnformatted("Participant Shared Clocks:");
        for(const ParticipantInfo& participant : room.participants) {
            const std::string participantName =
                !participant.displayName.empty()
                    ? participant.displayName
                    : std::to_string(static_cast<int>(participant.id));
            if(!participant.sharedClockSynchronized) {
                ImGui::TextDisabled("%s: unsynced", participantName.c_str());
                continue;
            }

            uint64_t projectedSharedClockMicros = participant.sharedClockMicros;
            if(participant.sharedClockSampledAtLocalMicros != 0 &&
               localNowMicros >= participant.sharedClockSampledAtLocalMicros) {
                projectedSharedClockMicros +=
                    (localNowMicros - participant.sharedClockSampledAtLocalMicros);
            }
            ImGui::Text("%s: %llu us (RTT %llu us)",
                        participantName.c_str(),
                        static_cast<unsigned long long>(projectedSharedClockMicros),
                        static_cast<unsigned long long>(participant.clockSyncRttMicros));
        }

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
            ImGui::TextColored(ImGuiTheme::accentActive(), "%s", snapshot.sessionBlockedReason.c_str());
        }
    }

    if(room.state == SessionState::Resyncing) {
        ImGui::Separator();
        ImGui::TextColored(ImGuiTheme::accentActive(),
                           "Resyncing to frame %u. Waiting for %u ACK(s).",
                           room.resyncTargetFrame,
                           room.pendingResyncAckCount);
    }

    const auto drawAssignmentTree = [&](const ParticipantInfo& participant) {
        const ParticipantId participantId = participant.id;
        const auto currentPort1 = geraNESEffectivePortDeviceFromTopology(room, kPort1PlayerSlot);
        const auto currentPort2 = geraNESEffectivePortDeviceFromTopology(room, kPort2PlayerSlot);
        const auto currentExpansionDevice = geraNESExpansionDeviceFromTopology(room);
        const auto currentNesMultitapDevice = geraNESNesMultitapDeviceFromTopology(room);
        const auto currentFamicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(room);
        const auto canAssignCandidate = [&](std::optional<Settings::Device> port1Device,
                                            std::optional<Settings::Device> port2Device,
                                            Settings::ExpansionDevice expansionDevice,
                                            Settings::NesMultitapDevice nesMultitapDevice,
                                            Settings::FamicomMultitapDevice famicomMultitapDevice,
                                            PlayerSlot slot) {
            return canAssignGeraNESInputCandidate(
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
                if(canAssignGeraNESInputCandidate(
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
            configureInputAssignments(
                runtime,
                participantId,
                port1Device,
                port2Device,
                currentExpansionDevice,
                Settings::NesMultitapDevice::NONE,
                Settings::FamicomMultitapDevice::NONE,
                buildMergedAssignments(
                    port1Device,
                    port2Device,
                    currentExpansionDevice,
                    Settings::NesMultitapDevice::NONE,
                    Settings::FamicomMultitapDevice::NONE,
                    port == Settings::Port::P_1 ? kPort1PlayerSlot : kPort2PlayerSlot
                )
            );
        };
        const auto selectExpansionDevice = [&](Settings::ExpansionDevice device) {
            const auto port1Device = std::optional<Settings::Device>(currentPort1);
            const auto port2Device = std::optional<Settings::Device>(currentPort2);
            configureInputAssignments(
                runtime,
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
            configureInputAssignments(
                runtime,
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
                currentNesMultitapDevice == Settings::NesMultitapDevice::NONE &&
                currentFamicomMultitapDevice == Settings::FamicomMultitapDevice::NONE &&
                ((port == Settings::Port::P_1 ? currentPort1 : currentPort2) == device);
            const bool enabled = canAssignCandidate(
                port1Device,
                port2Device,
                currentExpansionDevice,
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
                currentNesMultitapDevice == Settings::NesMultitapDevice::NONE &&
                currentFamicomMultitapDevice == Settings::FamicomMultitapDevice::NONE &&
                currentExpansionDevice == device;
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
                currentNesMultitapDevice == nesDevice &&
                currentFamicomMultitapDevice == famicomDevice;
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
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::CONTROLLER), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::FAMICOM_CONTROLLER), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::ZAPPER), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::SNES_MOUSE), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER), std::optional<Settings::Device>(currentPort2), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort1PlayerSlot);
        const bool anyPort2Enabled =
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::CONTROLLER), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::FAMICOM_CONTROLLER), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::ZAPPER), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::SNES_MOUSE), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot) ||
            canAssignCandidate(std::optional<Settings::Device>(currentPort1), std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER), currentExpansionDevice, Settings::NesMultitapDevice::NONE, Settings::FamicomMultitapDevice::NONE, kPort2PlayerSlot);
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

    const float participantRowHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.0f;
    const float participantsTableHeight = participantRowHeight * 4.0f;
    std::vector<const ParticipantInfo*> sortedParticipants;
    sortedParticipants.reserve(room.participants.size());
    for(const auto& participant : room.participants) {
        sortedParticipants.push_back(&participant);
    }
    std::sort(sortedParticipants.begin(), sortedParticipants.end(),
              [](const ParticipantInfo* lhs, const ParticipantInfo* rhs) {
                  return lhs->id < rhs->id;
              });
    if(ImGui::BeginTable("NetplayParticipants",
                         7,
                         ImGuiTableFlags_Borders |
                             ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_ScrollY,
                         ImVec2(0.0f, participantsTableHeight))) {
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Net", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Admin", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiTheme::textOnAccent());
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for(const ParticipantInfo* participantPtr : sortedParticipants) {
            const ParticipantInfo& participant = *participantPtr;
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
                participant.role == ParticipantRole::SessionOwner ? "Owner" :
                participant.role == ParticipantRole::SessionParticipant ? "Participant" : "Observer"
            );
            ImGui::TableNextColumn();
            if(snapshot.hosting) {
                const std::string preview = participantAssignmentsLabel(participant, room);
                const float comboHeight = ImGui::GetFrameHeight();
                for(PlayerSlot slot : participant.controllerAssignments) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiTheme::accentActive());
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiTheme::accentHovered());
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGuiTheme::accent());
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
                !participant.romLoaded ? ImGuiTheme::accentActive() :
                participant.romCompatible ? ImGuiTheme::success() :
                ImGuiTheme::error();
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
    ImGui::TextUnformatted("Log");
    std::string logText;
    for(const std::string& line : snapshot.eventLog) {
        logText += line;
        logText += '\n';
    }

    std::vector<char> logBuffer(logText.begin(), logText.end());
    logBuffer.push_back('\0');
    InputTextMultilineLog("##NetplayLogOuterChild",
                          "##NetplayLogMultilineInput",
                          logBuffer.data(),
                          logBuffer.size(),
                          ImVec2(-1.0f, 180.0f));
    ImGui::Checkbox("Show netplay debug log##NetplayDebugMode", &cfg.showNetplayDebugLog);
    ImGui::SameLine();
    if(ImGui::Button("Copy##NetplayLog")) {
#ifdef __EMSCRIPTEN__
        emcriptenCopyTextToClipboardExact(logText.c_str());
#else
        ImGui::SetClipboardText(logText.c_str());
#endif
    }
    ImGui::SameLine();
    if(ImGui::Button("Clear##NetplayLog")) {
        runtime.clearNetplayLog();
    }
    if(blockInputs) {
        ImGui::EndDisabled();
    }

    
    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 0.0f), ImVec2(360.0f, FLT_MAX));
    if(ImGui::BeginPopupModal(kNetplayOperationPopupId,
                              nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        if(!pendingOperation.active()) {
            pendingOperation.modalOpen = false;
            ImGui::CloseCurrentPopup();
        } else {
            TextCenteredWrapped(pendingOperation.statusText);
            if(pendingOperation.cancelRequested) {
                ImGui::TextDisabled("Canceling...");
            }
            const float cancelButtonWidth = 180.0f;
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            if(availableWidth > cancelButtonWidth) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availableWidth - cancelButtonWidth) * 0.5f);
            }
            ImGui::BeginDisabled(pendingOperation.cancelRequested);
            if(ImGui::Button("Cancel##NetplayOperationCancel", ImVec2(cancelButtonWidth, 0.0f))) {
                pendingOperation.cancelRequested = true;
                runtime.disconnect();
            }
            ImGui::EndDisabled();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace GeraNESNetplay
