#include "GeraNESNetplay/NetTransport.h"
#include "GeraNESNetplay/WebRtcPeerConnection.h"
#include "GeraNESNetplay/WebRtcSignalingClient.h"
#include "GeraNESNetplay/WebRtcSignalingServer.h"
#include "logger/logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <limits>
#include <optional>
#include <thread>

#if !defined(__EMSCRIPTEN__)
#include <enet/enet.h>
#endif

namespace Netplay {

namespace {

constexpr auto kWebRtcSignalingBootstrapTimeout = std::chrono::seconds(15);

const char* signalTypeLabel(WebRtcSignalType type)
{
    switch(type) {
        case WebRtcSignalType::Hello: return "Hello";
        case WebRtcSignalType::Welcome: return "Welcome";
        case WebRtcSignalType::CreateRoom: return "CreateRoom";
        case WebRtcSignalType::JoinRoom: return "JoinRoom";
        case WebRtcSignalType::RoomJoined: return "RoomJoined";
        case WebRtcSignalType::PeerJoined: return "PeerJoined";
        case WebRtcSignalType::PeerLeft: return "PeerLeft";
        case WebRtcSignalType::Offer: return "Offer";
        case WebRtcSignalType::Answer: return "Answer";
        case WebRtcSignalType::IceCandidate: return "IceCandidate";
        case WebRtcSignalType::Error: return "Error";
        case WebRtcSignalType::RoomList: return "RoomList";
        case WebRtcSignalType::LeaveRoom: return "LeaveRoom";
        default: return "Unknown";
    }
}

#if !defined(__EMSCRIPTEN__)
class ENetTransport final : public INetTransport
{
private:
    ENetHost* m_host = nullptr;
    bool m_initialized = false;
    NetTransportOptions m_options;
    std::string m_lastError;

    static PeerHandle toHandle(ENetPeer* peer)
    {
        return peer != nullptr ? static_cast<PeerHandle>(reinterpret_cast<uintptr_t>(peer)) : kInvalidPeerHandle;
    }

    static ENetPeer* fromHandle(PeerHandle peer)
    {
        return peer != kInvalidPeerHandle ? reinterpret_cast<ENetPeer*>(static_cast<uintptr_t>(peer)) : nullptr;
    }

public:
    ~ENetTransport() override
    {
        shutdown();
    }

    bool initialize() override
    {
        if(m_initialized) return true;
        if(enet_initialize() != 0) {
            m_lastError = "ENet initialization failed";
            return false;
        }
        m_lastError.clear();
        m_initialized = true;
        return true;
    }

    void shutdown() override
    {
        if(m_host != nullptr) {
            enet_host_destroy(m_host);
            m_host = nullptr;
        }

        if(m_initialized) {
            enet_deinitialize();
            m_initialized = false;
        }
    }

    void setOptions(const NetTransportOptions& options) override
    {
        m_options = options;
    }

    const NetTransportOptions& options() const override
    {
        return m_options;
    }

    const std::string& lastError() const override
    {
        return m_lastError;
    }

    const std::vector<std::string>& advertisedIceServers() const override
    {
        static const std::vector<std::string> empty;
        return empty;
    }

    bool hostSession(uint16_t port, size_t maxPeers) override
    {
        if(!initialize()) return false;

        if(m_host != nullptr) {
            enet_host_destroy(m_host);
            m_host = nullptr;
        }

        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = port;

        m_host = enet_host_create(&address, maxPeers, 3, 0, 0);
        return m_host != nullptr;
    }

    bool connectToHost(const std::string& hostName, uint16_t port, size_t channelCount = 3) override
    {
        if(!initialize()) return false;

        if(m_host != nullptr) {
            enet_host_destroy(m_host);
            m_host = nullptr;
        }

        m_host = enet_host_create(nullptr, 1, static_cast<size_t>(channelCount), 0, 0);
        if(m_host == nullptr) return false;

        ENetAddress address{};
        address.port = port;
        if(enet_address_set_host(&address, hostName.c_str()) != 0) {
            enet_host_destroy(m_host);
            m_host = nullptr;
            return false;
        }

        return enet_host_connect(m_host, &address, static_cast<size_t>(channelCount), 0) != nullptr;
    }

    void disconnectAll(uint32_t data = 0) override
    {
        if(m_host == nullptr) return;

        for(size_t i = 0; i < m_host->peerCount; ++i) {
            ENetPeer& peer = m_host->peers[i];
            if(peer.state != ENET_PEER_STATE_DISCONNECTED) {
                enet_peer_disconnect(&peer, data);
            }
        }
    }

    void disconnectPeer(PeerHandle peer, uint32_t data = 0) override
    {
        ENetPeer* rawPeer = fromHandle(peer);
        if(rawPeer == nullptr) return;
        if(rawPeer->state != ENET_PEER_STATE_DISCONNECTED) {
            enet_peer_disconnect(rawPeer, data);
        }
    }

    void flush() override
    {
        if(m_host == nullptr) return;
        enet_host_flush(m_host);
    }

    std::vector<PeerHandle> connectedPeers() const override
    {
        std::vector<PeerHandle> peers;
        if(m_host == nullptr) return peers;

        for(size_t i = 0; i < m_host->peerCount; ++i) {
            ENetPeer& peer = m_host->peers[i];
            if(peer.state == ENET_PEER_STATE_CONNECTED) {
                peers.push_back(toHandle(&peer));
            }
        }

        return peers;
    }

    std::vector<Event> poll(uint32_t timeoutMs) override
    {
        std::vector<Event> events;
        if(m_host == nullptr) return events;

        ENetEvent event{};
        while(enet_host_service(m_host, &event, timeoutMs) > 0) {
            Event out;
            out.peer = toHandle(event.peer);

            switch(event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    out.type = Event::Type::Connected;
                    events.push_back(std::move(out));
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    out.type = Event::Type::Disconnected;
                    out.data = event.data;
                    events.push_back(std::move(out));
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    out.type = Event::Type::PacketReceived;
                    out.channel = static_cast<Channel>(event.channelID);
                    out.payload.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                    events.push_back(std::move(out));
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_NONE:
                default:
                    break;
            }

            timeoutMs = 0;
        }

        return events;
    }

    bool sendReliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) override
    {
        ENetPeer* rawPeer = fromHandle(peer);
        if(rawPeer == nullptr || payload.empty()) return false;

        ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
        if(packet == nullptr) return false;

        return enet_peer_send(rawPeer, static_cast<enet_uint8>(channel), packet) == 0;
    }

    bool sendUnreliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) override
    {
        ENetPeer* rawPeer = fromHandle(peer);
        if(rawPeer == nullptr || payload.empty()) return false;

        ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), 0);
        if(packet == nullptr) return false;

        return enet_peer_send(rawPeer, static_cast<enet_uint8>(channel), packet) == 0;
    }

    bool broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) override
    {
        if(m_host == nullptr || payload.empty()) return false;

        bool sent = false;
        for(size_t i = 0; i < m_host->peerCount; ++i) {
            ENetPeer& peer = m_host->peers[i];
            if(toHandle(&peer) == exceptPeer || peer.state != ENET_PEER_STATE_CONNECTED) continue;
            if(sendReliable(toHandle(&peer), channel, payload)) {
                sent = true;
            }
        }

        return sent;
    }

    bool broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) override
    {
        if(m_host == nullptr || payload.empty()) return false;

        bool sent = false;
        for(size_t i = 0; i < m_host->peerCount; ++i) {
            ENetPeer& peer = m_host->peers[i];
            if(toHandle(&peer) == exceptPeer || peer.state != ENET_PEER_STATE_CONNECTED) continue;
            if(sendUnreliable(toHandle(&peer), channel, payload)) {
                sent = true;
            }
        }

        return sent;
    }

    uintptr_t peerTag(PeerHandle peer) const override
    {
        ENetPeer* rawPeer = fromHandle(peer);
        if(rawPeer == nullptr || rawPeer->data == nullptr) return 0;
        return reinterpret_cast<uintptr_t>(rawPeer->data);
    }

    void setPeerTag(PeerHandle peer, uintptr_t tag) override
    {
        ENetPeer* rawPeer = fromHandle(peer);
        if(rawPeer == nullptr) return;
        rawPeer->data = reinterpret_cast<void*>(tag);
    }

    uint32_t peerRoundTripTime(PeerHandle peer) const override
    {
        ENetPeer* rawPeer = fromHandle(peer);
        return rawPeer != nullptr ? rawPeer->roundTripTime : 0u;
    }

    uint32_t peerRoundTripVariance(PeerHandle peer) const override
    {
        ENetPeer* rawPeer = fromHandle(peer);
        return rawPeer != nullptr ? rawPeer->roundTripTimeVariance : 0u;
    }

    bool isActive() const override
    {
        return m_host != nullptr;
    }
};
#endif

class WebRTCTransport final : public INetTransport
{
private:
    static constexpr auto kWebRtcPeerHandshakeTimeout = std::chrono::seconds(12);
    static constexpr auto kWebRtcGracefulDisconnectDelay = std::chrono::milliseconds(350);

    enum class PeerHandshakeState : uint8_t
    {
        Idle,
        WaitingForOffer,
        CreatingOffer,
        WaitingForAnswer,
        Negotiating,
        Connected
    };

    struct WebRtcPeerState
    {
        PeerHandle handle = kInvalidPeerHandle;
        std::string remotePeerId;
        std::unique_ptr<IWebRtcPeerConnection> connection;
        uintptr_t tag = 0;
        uint32_t pingMs = 0;
        uint32_t jitterMs = 0;
        bool connected = false;
        PeerHandshakeState handshakeState = PeerHandshakeState::Idle;
        std::optional<std::chrono::steady_clock::time_point> handshakeDeadline;
        std::optional<std::chrono::steady_clock::time_point> closeDeadline;
    };

    NetTransportOptions m_options;
    std::string m_lastError;
    std::unique_ptr<IWebRtcSignalingServer> m_signalingServer;
    std::unique_ptr<IWebRtcSignalingClient> m_signalingClient;
    std::deque<IWebRtcSignalingClient::Event> m_pendingSignalingEvents;
    std::string m_localPeerId;
    std::optional<WebRtcSignalingConfig> m_activeSignalingConfig;
    std::vector<std::string> m_signaledIceServers;
    std::vector<WebRtcPeerState> m_peers;
    PeerHandle m_nextPeerHandle = 1;
    size_t m_requestedMaxPeers = 1;
    bool m_signalingReady = false;
    bool m_hosting = false;
    bool m_active = false;
    bool m_waitingForInitialOffer = false;
    std::optional<std::chrono::steady_clock::time_point> m_initialOfferDeadline;
    std::optional<std::chrono::steady_clock::time_point> m_bootstrapDeadline;
    bool m_bootstrapPending = false;
    bool m_bootstrapHost = false;
    bool m_bootstrapHelloSent = false;
    bool m_bootstrapRoomRequestSent = false;
    bool m_bootstrapSawWelcome = false;
    bool m_bootstrapSawRoomJoined = false;
    bool m_fatalShutdownRequested = false;
    void logTrace(const std::string& message) const
    {
        Logger::instance().log("[WebRTC transport] " + message, Logger::Type::INFO);
    }

    static const char* handshakeStageLabel(PeerHandshakeState state)
    {
        switch(state) {
            case PeerHandshakeState::WaitingForOffer:
                return "waiting for WebRTC offer from host";
            case PeerHandshakeState::CreatingOffer:
                return "creating WebRTC offer";
            case PeerHandshakeState::WaitingForAnswer:
                return "waiting for WebRTC answer from client";
            case PeerHandshakeState::Negotiating:
                return "waiting for ICE/data channel negotiation";
            case PeerHandshakeState::Idle:
            case PeerHandshakeState::Connected:
            default:
                return "waiting for peer handshake";
        }
    }

    bool shouldCreateOfferForPeer(const std::string& remotePeerId) const
    {
        if(remotePeerId.empty() || remotePeerId == m_localPeerId) {
            return false;
        }

        // Use a deterministic offerer so both peers can react to PeerJoined
        // without creating glare. Current peer ids are prefixed host-/peer-,
        // which makes the joining peer the offerer in host/client sessions.
        return m_localPeerId > remotePeerId;
    }

    void clearRuntimeState()
    {
        if(!m_peers.empty() || m_signalingReady || m_active) {
            logTrace("clearing runtime state");
        }
        for(WebRtcPeerState& peer : m_peers) {
            if(peer.connection) {
                peer.connection->close();
            }
        }
        m_peers.clear();
        m_signalingReady = false;
        m_localPeerId.clear();
        m_signaledIceServers.clear();
        m_pendingSignalingEvents.clear();
        m_nextPeerHandle = 1;
        m_hosting = false;
        m_active = false;
        m_waitingForInitialOffer = false;
        m_initialOfferDeadline.reset();
        m_bootstrapDeadline.reset();
        m_bootstrapPending = false;
        m_bootstrapHost = false;
        m_bootstrapHelloSent = false;
        m_bootstrapRoomRequestSent = false;
        m_bootstrapSawWelcome = false;
        m_bootstrapSawRoomJoined = false;
        m_fatalShutdownRequested = false;
    }

    void requestFatalShutdown(const std::string& error)
    {
        m_lastError = error;
        m_fatalShutdownRequested = true;
    }

    bool sendBootstrapRoomRequest()
    {
        if(!m_bootstrapPending || m_bootstrapRoomRequestSent) {
            return true;
        }

        WebRtcSignalingMessage roomMessage;
        roomMessage.type = m_bootstrapHost ? WebRtcSignalType::CreateRoom : WebRtcSignalType::JoinRoom;
        roomMessage.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
        roomMessage.peerId = m_localPeerId;
        roomMessage.password = m_activeSignalingConfig ? m_activeSignalingConfig->password : std::string{};
        if(m_bootstrapHost) {
            roomMessage.maxParticipants = maxParticipantsForSignal();
        }
        if(!sendSignalingMessage(roomMessage)) {
            return false;
        }

        m_bootstrapRoomRequestSent = true;
        logTrace(std::string("sent signaling message: ") +
                 (m_bootstrapHost ? "CreateRoom" : "JoinRoom") +
                 " roomId=" + roomMessage.roomId);
        return true;
    }

    void startInitialOfferWait()
    {
        m_waitingForInitialOffer = true;
        m_initialOfferDeadline = std::chrono::steady_clock::now() + kWebRtcPeerHandshakeTimeout;
    }

    void stopInitialOfferWait()
    {
        m_waitingForInitialOffer = false;
        m_initialOfferDeadline.reset();
    }

    void startPeerHandshake(WebRtcPeerState& peer, PeerHandshakeState state)
    {
        peer.handshakeState = state;
        peer.handshakeDeadline = std::chrono::steady_clock::now() + kWebRtcPeerHandshakeTimeout;
    }

    void noteHandshakeProgress(WebRtcPeerState& peer, PeerHandshakeState state)
    {
        peer.handshakeState = state;
        if(peer.handshakeDeadline.has_value()) {
            peer.handshakeDeadline = std::chrono::steady_clock::now() + kWebRtcPeerHandshakeTimeout;
        }
    }

    void markPeerConnected(WebRtcPeerState& peer)
    {
        peer.handshakeState = PeerHandshakeState::Connected;
        peer.handshakeDeadline.reset();
    }

    void updateHandshakeTimeout()
    {
        if(m_bootstrapPending &&
           m_bootstrapDeadline.has_value() &&
           m_lastError.empty() &&
           std::chrono::steady_clock::now() >= *m_bootstrapDeadline) {
            requestFatalShutdown("WebRTC signaling handshake timed out");
            return;
        }

        if(!m_active || !m_signalingReady || !m_lastError.empty()) {
            return;
        }

        if(m_waitingForInitialOffer &&
           m_initialOfferDeadline.has_value() &&
           std::chrono::steady_clock::now() >= *m_initialOfferDeadline) {
            requestFatalShutdown(
                "WebRTC peer handshake timed out (" +
                std::string(handshakeStageLabel(PeerHandshakeState::WaitingForOffer)) + ")"
            );
            return;
        }

        for(const WebRtcPeerState& peer : m_peers) {
            if(!peer.handshakeDeadline.has_value()) {
                continue;
            }
            if(peer.handshakeState == PeerHandshakeState::Idle ||
               peer.handshakeState == PeerHandshakeState::Connected) {
                continue;
            }
            if(std::chrono::steady_clock::now() < *peer.handshakeDeadline) {
                continue;
            }

            requestFatalShutdown(
                "WebRTC peer handshake timed out (" +
                std::string(handshakeStageLabel(peer.handshakeState)) +
                " for " + peer.remotePeerId + ")"
            );
            return;
        }
    }

    bool ensureSignalingServer()
    {
        if(m_signalingServer) return true;
        m_signalingServer = createWebRtcSignalingServer();
        return static_cast<bool>(m_signalingServer);
    }

    bool ensureSignalingClient()
    {
        if(m_signalingClient) return true;
        m_signalingClient = createWebRtcSignalingClient();
        return static_cast<bool>(m_signalingClient);
    }

    static std::string generatePeerId(bool host)
    {
        static std::atomic<uint64_t> nextNonce{1};

        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const uint64_t ticks = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        const uint64_t nonce = nextNonce.fetch_add(1, std::memory_order_relaxed);
        return std::string(host ? "host-" : "peer-") +
               std::to_string(static_cast<unsigned long long>(ticks)) +
               "-" +
               std::to_string(static_cast<unsigned long long>(nonce));
    }

    std::optional<WebRtcSignalingConfig> resolveSignalingConfig(bool host,
                                                                const std::string& hostName,
                                                                uint16_t port) const
    {
        WebRtcSignalingConfig config;
        if(m_options.webRtcSignaling.has_value()) {
            config = *m_options.webRtcSignaling;
        }
        if(config.roomId.empty()) {
            config.roomId = "default";
        }

        if(m_options.useEmbeddedWebRtcSignalingServer) {
            if(config.url.empty()) {
                const std::string signalingHost = host ? "127.0.0.1" : hostName;
                if(signalingHost.empty() || port == 0) {
                    return std::nullopt;
                }
                config.url = buildWebRtcSignalingUrl(signalingHost, port);
            }
        }
        else if(config.url.empty()) {
            return std::nullopt;
        }

        return config.valid() ? std::optional<WebRtcSignalingConfig>(std::move(config)) : std::nullopt;
    }

    bool ensurePeerConnection(std::unique_ptr<IWebRtcPeerConnection>& connection)
    {
        if(connection) return true;
        connection = createWebRtcPeerConnection();
        return static_cast<bool>(connection);
    }

    int maxParticipantsForSignal() const
    {
        const size_t totalParticipants = std::max<size_t>(1, m_requestedMaxPeers) + 1;
        return static_cast<int>(std::min<size_t>(totalParticipants, static_cast<size_t>(std::numeric_limits<int>::max())));
    }

    bool bootstrapSignaling(bool host)
    {
        clearRuntimeState();
        logTrace(std::string("bootstrap begin as ") + (host ? "host" : "client"));

        if(m_signalingClient) {
            m_signalingClient->disconnect();
            m_signalingClient.reset();
        }

        if(!ensureSignalingClient()) {
            m_lastError = "WebRTC signaling client could not be created";
            logTrace(m_lastError);
            return false;
        }

        m_localPeerId = generatePeerId(host);
        WebRtcSignalingClientOptions options;
        if(!m_activeSignalingConfig.has_value()) {
            m_lastError = "Configure signaling URL and room id for WebRTC";
            return false;
        }
        options.config = *m_activeSignalingConfig;
        options.localPeerId = m_localPeerId;
        options.host = host;

        if(!m_signalingClient->connect(options)) {
            m_lastError = m_signalingClient->lastError();
            logTrace("signaling connect failed: " + m_lastError);
            return false;
        }

        m_bootstrapPending = true;
        m_bootstrapHost = host;
        m_bootstrapHelloSent = false;
        m_bootstrapRoomRequestSent = false;
        m_bootstrapSawWelcome = false;
        m_bootstrapSawRoomJoined = false;
        m_bootstrapDeadline = std::chrono::steady_clock::now() + kWebRtcSignalingBootstrapTimeout;
        return true;
    }

    WebRtcPeerState* findPeerByHandle(PeerHandle handle)
    {
        auto it = std::find_if(m_peers.begin(), m_peers.end(),
                               [handle](const WebRtcPeerState& peer) { return peer.handle == handle; });
        return it != m_peers.end() ? &*it : nullptr;
    }

    const WebRtcPeerState* findPeerByHandle(PeerHandle handle) const
    {
        auto it = std::find_if(m_peers.begin(), m_peers.end(),
                               [handle](const WebRtcPeerState& peer) { return peer.handle == handle; });
        return it != m_peers.end() ? &*it : nullptr;
    }

    WebRtcPeerState* findPeerByRemoteId(const std::string& remotePeerId)
    {
        auto it = std::find_if(m_peers.begin(), m_peers.end(),
                               [&remotePeerId](const WebRtcPeerState& peer) { return peer.remotePeerId == remotePeerId; });
        return it != m_peers.end() ? &*it : nullptr;
    }

    bool initializePeerConnection(const std::string& remotePeerId, bool host)
    {
        if(remotePeerId.empty() || remotePeerId == m_localPeerId) {
            return false;
        }

        if(findPeerByRemoteId(remotePeerId) != nullptr) {
            return true;
        }

        WebRtcPeerState state;
        state.handle = m_nextPeerHandle++;
        state.remotePeerId = remotePeerId;

        if(!ensurePeerConnection(state.connection)) {
            m_lastError = "WebRTC peer connection could not be created";
            logTrace(m_lastError);
            return false;
        }

        WebRtcPeerConnectionOptions options;
        options.localPeerId = m_localPeerId;
        options.remotePeerId = remotePeerId;
        options.iceServers = m_signaledIceServers;
        options.host = host;

        if(!state.connection->open(options)) {
            m_lastError = state.connection->lastError();
            logTrace("peer open failed for " + remotePeerId + ": " + m_lastError);
            return false;
        }

        logTrace("peer connection opened for " + remotePeerId);
        m_peers.push_back(std::move(state));
        return true;
    }

    void closePeer(PeerHandle handle, std::vector<Event>* events = nullptr)
    {
        auto it = std::find_if(m_peers.begin(), m_peers.end(),
                               [handle](const WebRtcPeerState& peer) { return peer.handle == handle; });
        if(it == m_peers.end()) {
            return;
        }

        if(it->connected && events != nullptr) {
            Event disconnected;
            disconnected.type = Event::Type::Disconnected;
            disconnected.peer = it->handle;
            events->push_back(std::move(disconnected));
        }
        if(it->connection) {
            it->connection->close();
        }
        m_peers.erase(it);
        if(m_peers.empty()) {
            stopInitialOfferWait();
        }
        m_lastError.clear();
    }

    void closeAllPeers(std::vector<Event>* events = nullptr)
    {
        std::vector<PeerHandle> handles;
        handles.reserve(m_peers.size());
        for(const WebRtcPeerState& peer : m_peers) {
            handles.push_back(peer.handle);
        }
        for(PeerHandle handle : handles) {
            closePeer(handle, events);
        }
    }

    void schedulePeerClose(WebRtcPeerState& peer)
    {
        peer.closeDeadline = std::chrono::steady_clock::now() + kWebRtcGracefulDisconnectDelay;
    }

    void processPendingPeerClose(std::vector<Event>& events)
    {
        const auto now = std::chrono::steady_clock::now();
        std::vector<PeerHandle> handles;
        for(const WebRtcPeerState& peer : m_peers) {
            if(peer.closeDeadline.has_value() && now >= *peer.closeDeadline) {
                handles.push_back(peer.handle);
            }
        }
        for(PeerHandle handle : handles) {
            closePeer(handle, &events);
        }
    }

    bool shouldIgnoreExpectedPeerError(const WebRtcPeerState& peer, const std::string& error) const
    {
        if(error.empty() || !peer.closeDeadline.has_value()) {
            return false;
        }

        return error.find("User-Initiated Abort") != std::string::npos ||
               error.find("detail=sctp-failure") != std::string::npos ||
               error.find("sctpCauseCode=12") != std::string::npos;
    }

    std::vector<uint8_t> wrapPayload(Channel channel, const std::vector<uint8_t>& payload) const
    {
        std::vector<uint8_t> wrapped;
        wrapped.reserve(1 + payload.size());
        wrapped.push_back(static_cast<uint8_t>(channel));
        wrapped.insert(wrapped.end(), payload.begin(), payload.end());
        return wrapped;
    }

    bool sendSignalingMessage(const WebRtcSignalingMessage& message)
    {
        if(!m_signalingClient) {
            m_lastError = "WebRTC signaling client is not connected";
            return false;
        }
        if(!m_signalingClient->send(message)) {
            m_lastError = m_signalingClient->lastError();
            return false;
        }
        return true;
    }

    bool sendPeerSignalingMessage(const WebRtcSignalingMessage& message,
                                  const WebRtcPeerState& peer,
                                  const char* context)
    {
        if(sendSignalingMessage(message)) {
            return true;
        }

        logTrace(std::string(context) + " failed for " + peer.remotePeerId + ": " + m_lastError);
        requestFatalShutdown(m_lastError.empty() ? std::string("WebRTC signaling send failed") : m_lastError);
        return false;
    }

    void sendLeaveRoomBestEffort()
    {
        if(!m_signalingClient || !m_signalingClient->isConnected()) {
            return;
        }
        if(!m_activeSignalingConfig.has_value() || m_localPeerId.empty()) {
            return;
        }
        if(!m_signalingReady && !m_bootstrapSawRoomJoined) {
            return;
        }

        WebRtcSignalingMessage leaveMessage;
        leaveMessage.type = WebRtcSignalType::LeaveRoom;
        leaveMessage.roomId = m_activeSignalingConfig->roomId;
        leaveMessage.peerId = m_localPeerId;
        logTrace("sent signaling message: LeaveRoom roomId=" + leaveMessage.roomId);
        (void)m_signalingClient->send(leaveMessage);
    }

    void drainPeerEvents(PeerHandle handle, std::vector<Event>& events)
    {
        WebRtcPeerState* peer = findPeerByHandle(handle);
        if(peer == nullptr || !peer->connection) return;

        for(const auto& event : peer->connection->poll()) {
            switch(event.type) {
                case IWebRtcPeerConnection::Event::Type::LocalDescriptionReady: {
                    logTrace(std::string("local description ready for ") + peer->remotePeerId +
                             (event.descriptionIsOffer ? " (offer)" : " (answer)"));
                    WebRtcSignalingMessage message;
                    message.type = event.descriptionIsOffer ? WebRtcSignalType::Offer : WebRtcSignalType::Answer;
                    if(event.descriptionIsOffer) {
                        noteHandshakeProgress(*peer, PeerHandshakeState::WaitingForAnswer);
                    } else {
                        noteHandshakeProgress(*peer, PeerHandshakeState::Negotiating);
                    }
                    message.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    message.peerId = m_localPeerId;
                    message.targetPeerId = peer->remotePeerId;
                    message.sdp = event.sdp;
                    if(!sendPeerSignalingMessage(
                           message,
                           *peer,
                           event.descriptionIsOffer ? "sending WebRTC offer" : "sending WebRTC answer")) {
                        return;
                    }
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::IceCandidateReady: {
                    logTrace("local ICE candidate ready for " + peer->remotePeerId);
                    WebRtcSignalingMessage message;
                    message.type = WebRtcSignalType::IceCandidate;
                    noteHandshakeProgress(*peer, PeerHandshakeState::Negotiating);
                    message.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    message.peerId = m_localPeerId;
                    message.targetPeerId = peer->remotePeerId;
                    message.candidate = event.candidate;
                    message.mid = event.mid;
                    message.mlineIndex = event.mlineIndex;
                    if(!sendPeerSignalingMessage(message, *peer, "sending WebRTC ICE candidate")) {
                        return;
                    }
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::DataChannelOpen: {
                    if(!peer->connected) {
                        logTrace("data channel opened for " + peer->remotePeerId);
                        peer->connected = true;
                        markPeerConnected(*peer);
                        stopInitialOfferWait();
                        Event connected;
                        connected.type = Event::Type::Connected;
                        connected.peer = peer->handle;
                        events.push_back(std::move(connected));
                    }
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::DataChannelClosed: {
                    logTrace("data channel closed for " + peer->remotePeerId);
                    if(peer->connected) {
                        peer->connected = false;
                        Event disconnected;
                        disconnected.type = Event::Type::Disconnected;
                        disconnected.peer = peer->handle;
                        events.push_back(std::move(disconnected));
                    }
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::DataMessage: {
                    if(event.payload.empty()) {
                        break;
                    }
                    Event packet;
                    packet.type = Event::Type::PacketReceived;
                    packet.peer = peer->handle;
                    packet.channel = static_cast<Channel>(event.payload.front());
                    packet.payload.assign(event.payload.begin() + 1, event.payload.end());
                    events.push_back(std::move(packet));
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::Error:
                {
                    const std::string error = event.text.empty() ? peer->connection->lastError() : event.text;
                    logTrace("peer error for " + peer->remotePeerId + ": " + error);
                    if(!shouldIgnoreExpectedPeerError(*peer, error)) {
                        m_lastError = error;
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    void processSignalingEvents(std::vector<Event>& events)
    {
        if(!m_signalingClient) return;

        std::deque<IWebRtcSignalingClient::Event> signalingEvents = std::move(m_pendingSignalingEvents);
        for(const auto& event : m_signalingClient->poll()) {
            signalingEvents.push_back(event);
        }

        for(const auto& event : signalingEvents) {
            if(event.type == IWebRtcSignalingClient::Event::Type::Disconnected) {
                logTrace("signaling disconnected");
                closeAllPeers(&events);
                if(m_bootstrapPending && m_lastError.empty()) {
                    requestFatalShutdown("WebRTC signaling disconnected during connection setup");
                }
                m_bootstrapPending = false;
                m_bootstrapHelloSent = false;
                m_bootstrapRoomRequestSent = false;
                m_bootstrapSawWelcome = false;
                m_bootstrapSawRoomJoined = false;
                m_bootstrapDeadline.reset();
                stopInitialOfferWait();
                m_signalingReady = false;
                m_active = false;
                continue;
            }

            if(event.type == IWebRtcSignalingClient::Event::Type::Error) {
                const std::string errorText = !event.text.empty() ? event.text : m_signalingClient->lastError();
                logTrace("signaling event error: " + errorText);
                if(!m_signalingClient->isConnected()) {
                    requestFatalShutdown(errorText);
                    if(m_bootstrapPending) {
                        m_bootstrapPending = false;
                        m_bootstrapDeadline.reset();
                    }
                }
                continue;
            }

            if(event.type == IWebRtcSignalingClient::Event::Type::Connected) {
                if(m_bootstrapPending && !m_bootstrapHelloSent) {
                    WebRtcSignalingMessage hello;
                    hello.type = WebRtcSignalType::Hello;
                    hello.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    hello.peerId = m_localPeerId;
                    if(!sendSignalingMessage(hello)) {
                        m_bootstrapPending = false;
                        continue;
                    }
                    m_bootstrapHelloSent = true;
                    logTrace("sent signaling message: Hello roomId=" + hello.roomId);
                }
                continue;
            }

            if(event.type != IWebRtcSignalingClient::Event::Type::Message) {
                continue;
            }

            const WebRtcSignalingMessage& message = event.message;
            std::string signalLog = std::string("received signaling message: ") + signalTypeLabel(message.type);
            if(!message.peerId.empty()) {
                signalLog += " peerId=" + message.peerId;
            }
            if(!message.targetPeerId.empty()) {
                signalLog += " targetPeerId=" + message.targetPeerId;
            }
            if(!message.roomId.empty()) {
                signalLog += " roomId=" + message.roomId;
            }
            logTrace(signalLog);
            if(m_bootstrapPending) {
                if(message.type == WebRtcSignalType::Welcome) {
                    m_bootstrapSawWelcome = true;
                    if(!message.iceServers.empty()) {
                        m_signaledIceServers = message.iceServers;
                    }
                    if(!sendBootstrapRoomRequest()) {
                        m_bootstrapPending = false;
                        m_bootstrapDeadline.reset();
                        continue;
                    }
                } else if(message.type == WebRtcSignalType::RoomJoined &&
                          m_activeSignalingConfig.has_value() &&
                          message.roomId == m_activeSignalingConfig->roomId) {
                    m_bootstrapSawRoomJoined = true;
                    if(!message.iceServers.empty()) {
                        m_signaledIceServers = message.iceServers;
                    }
                    if(m_bootstrapHost) {
                        logTrace("host room registered on signaling server: " + message.roomId);
                    } else {
                        logTrace("joined signaling room: " + message.roomId);
                    }
                } else if(message.type == WebRtcSignalType::Error) {
                    requestFatalShutdown(!message.error.empty() ? message.error : "WebRTC signaling reported an error");
                    m_bootstrapPending = false;
                    m_bootstrapDeadline.reset();
                    continue;
                } else {
                    logTrace(std::string("queued signaling message during bootstrap: ") + signalTypeLabel(message.type));
                    m_pendingSignalingEvents.push_back(event);
                }

                if(m_bootstrapSawWelcome && m_bootstrapSawRoomJoined) {
                    m_signalingReady = true;
                    m_bootstrapPending = false;
                    m_bootstrapDeadline.reset();
                    if(!m_bootstrapHost) {
                        startInitialOfferWait();
                    }
                    logTrace("bootstrap complete");
                }
                continue;
            }
            switch(message.type) {
                case WebRtcSignalType::PeerJoined:
                    if(message.peerId != m_localPeerId) {
                        const bool createOffer = shouldCreateOfferForPeer(message.peerId);
                        logTrace(
                            std::string("peer joined room: ") + message.peerId +
                            " createOffer=" + (createOffer ? "true" : "false")
                        );
                        if(createOffer && initializePeerConnection(message.peerId, true)) {
                            WebRtcPeerState* peer = findPeerByRemoteId(message.peerId);
                            if(peer &&
                               peer->connection &&
                               !peer->connected &&
                               peer->handshakeState == PeerHandshakeState::Idle) {
                                stopInitialOfferWait();
                                startPeerHandshake(*peer, PeerHandshakeState::CreatingOffer);
                                logTrace("starting createOffer for " + message.peerId);
                                if(!peer->connection->createOffer()) {
                                    m_lastError = peer->connection->lastError();
                                    logTrace("createOffer failed for " + message.peerId + ": " + m_lastError);
                                } else {
                                    logTrace("createOffer submitted for " + message.peerId);
                                }
                            }
                        }
                    } else {
                        logTrace(
                            std::string("ignoring PeerJoined for ") + message.peerId +
                            " localPeerId=" + m_localPeerId
                        );
                    }
                    break;

                case WebRtcSignalType::PeerLeft:
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        closePeer(peer->handle, &events);
                    }
                    break;

                case WebRtcSignalType::Offer:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    logTrace("processing offer from " + message.peerId);
                    if(!initializePeerConnection(message.peerId, false)) {
                        break;
                    }
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        stopInitialOfferWait();
                        startPeerHandshake(*peer, PeerHandshakeState::Negotiating);
                        if(!peer->connection->setRemoteDescription(message.sdp, true)) {
                            m_lastError = peer->connection->lastError();
                            logTrace("setRemoteDescription(offer) failed for " + message.peerId + ": " + m_lastError);
                            break;
                        }
                        if(!peer->connection->createAnswer()) {
                            m_lastError = peer->connection->lastError();
                            logTrace("createAnswer failed for " + message.peerId + ": " + m_lastError);
                        }
                    }
                    break;

                case WebRtcSignalType::Answer:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    logTrace("processing answer from " + message.peerId);
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        noteHandshakeProgress(*peer, PeerHandshakeState::Negotiating);
                        if(!peer->connection->setRemoteDescription(message.sdp, false)) {
                            m_lastError = peer->connection->lastError();
                            logTrace("setRemoteDescription(answer) failed for " + message.peerId + ": " + m_lastError);
                        }
                    }
                    break;

                case WebRtcSignalType::IceCandidate:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    logTrace("processing ICE candidate from " + message.peerId);
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        noteHandshakeProgress(*peer, PeerHandshakeState::Negotiating);
                        if(!peer->connection->addRemoteIceCandidate(message.candidate, message.mid, message.mlineIndex)) {
                            m_lastError = peer->connection->lastError();
                            logTrace("addRemoteIceCandidate failed for " + message.peerId + ": " + m_lastError);
                        }
                    }
                    break;

                default:
                    break;
            }
        }
    }

public:
    bool initialize() override { return true; }
    void shutdown() override
    {
        if(!m_lastError.empty()) {
            logTrace("shutdown due to error: " + m_lastError);
        } else {
            logTrace("shutdown");
        }
        sendLeaveRoomBestEffort();
        if(m_signalingClient) {
            m_signalingClient->disconnect();
        }
        if(m_signalingServer) {
            m_signalingServer->stop();
        }
        m_signalingClient.reset();
        m_signalingServer.reset();
        clearRuntimeState();
    }
    void setOptions(const NetTransportOptions& options) override { m_options = options; }
    const NetTransportOptions& options() const override { return m_options; }
    const std::string& lastError() const override { return m_lastError; }
    const std::vector<std::string>& advertisedIceServers() const override { return m_signaledIceServers; }
    bool hostSession(uint16_t port, size_t maxPeers) override
    {
        shutdown();
        m_requestedMaxPeers = std::max<size_t>(1, maxPeers);
        m_activeSignalingConfig = resolveSignalingConfig(true, {}, port);
        if(!m_activeSignalingConfig.has_value()) {
            m_lastError = "Configure signaling URL and room id for WebRTC";
            return false;
        }

#if defined(__EMSCRIPTEN__)
        if(m_options.useEmbeddedWebRtcSignalingServer) {
            m_lastError = "Embedded WebRTC signaling server is not available in the web build";
            return false;
        }
#endif

        if(m_options.useEmbeddedWebRtcSignalingServer) {
            const std::optional<uint16_t> signalingPort =
                parseWebRtcSignalingUrlPort(m_activeSignalingConfig->url);
            if(!signalingPort.has_value()) {
                m_lastError = "Embedded WebRTC signaling requires a signaling URL with an explicit port";
                return false;
            }
            if(!ensureSignalingServer()) {
                m_lastError = "Embedded WebRTC signaling server could not be created";
                return false;
            }
            if(!m_signalingServer->start(*signalingPort)) {
                m_lastError = m_signalingServer->lastError();
                return false;
            }
        }

        if(!bootstrapSignaling(true)) {
            shutdown();
            return false;
        }

        m_hosting = true;
        m_active = true;
        m_lastError.clear();
        return true;
    }
    bool connectToHost(const std::string& hostName, uint16_t port, size_t = 3) override
    {
        shutdown();
        m_requestedMaxPeers = 1;
        m_activeSignalingConfig = resolveSignalingConfig(false, hostName, port);
        if(!m_activeSignalingConfig.has_value()) {
            m_lastError = "Configure signaling URL and room id for WebRTC";
            logTrace(m_lastError);
            return false;
        }

        if(!bootstrapSignaling(false)) {
            shutdown();
            return false;
        }

        m_hosting = false;
        m_active = true;
        m_lastError.clear();
        return true;
    }
    void disconnectAll(uint32_t = 0) override { shutdown(); }
    void disconnectPeer(PeerHandle peer, uint32_t = 0) override
    {
        if(WebRtcPeerState* state = findPeerByHandle(peer)) {
            if(state->connected) {
                schedulePeerClose(*state);
            } else {
                closePeer(peer);
            }
        }
    }
    void flush() override {}
    std::vector<PeerHandle> connectedPeers() const override
    {
        std::vector<PeerHandle> peers;
        for(const WebRtcPeerState& peer : m_peers) {
            if(peer.connected) {
                peers.push_back(peer.handle);
            }
        }
        return peers;
    }
    std::vector<Event> poll(uint32_t timeoutMs) override
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        std::vector<Event> events;

        do {
            processSignalingEvents(events);
            std::vector<PeerHandle> handles;
            handles.reserve(m_peers.size());
            for(const WebRtcPeerState& peer : m_peers) {
                handles.push_back(peer.handle);
            }
            for(PeerHandle handle : handles) {
                drainPeerEvents(handle, events);
            }
            processPendingPeerClose(events);
            if(!events.empty() || timeoutMs == 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while(std::chrono::steady_clock::now() < deadline);

        updateHandshakeTimeout();
        if(m_fatalShutdownRequested) {
            shutdown();
        }

        return events;
    }
    bool sendReliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) override
    {
        WebRtcPeerState* state = findPeerByHandle(peer);
        if(state == nullptr || !state->connection) return false;
        return state->connection->sendDataReliable(wrapPayload(channel, payload));
    }
    bool sendUnreliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) override
    {
        WebRtcPeerState* state = findPeerByHandle(peer);
        if(state == nullptr || !state->connection) return false;
        return state->connection->sendDataUnreliable(wrapPayload(channel, payload));
    }
    bool broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) override
    {
        bool sent = false;
        const auto wrapped = wrapPayload(channel, payload);
        for(const WebRtcPeerState& peer : m_peers) {
            if(peer.handle == exceptPeer || !peer.connection || !peer.connected) continue;
            sent = peer.connection->sendDataReliable(wrapped) || sent;
        }
        return sent;
    }
    bool broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) override
    {
        bool sent = false;
        const auto wrapped = wrapPayload(channel, payload);
        for(const WebRtcPeerState& peer : m_peers) {
            if(peer.handle == exceptPeer || !peer.connection || !peer.connected) continue;
            sent = peer.connection->sendDataUnreliable(wrapped) || sent;
        }
        return sent;
    }
    uintptr_t peerTag(PeerHandle peer) const override
    {
        if(const WebRtcPeerState* state = findPeerByHandle(peer)) {
            return state->tag;
        }
        return 0;
    }
    void setPeerTag(PeerHandle peer, uintptr_t tag) override
    {
        if(WebRtcPeerState* state = findPeerByHandle(peer)) {
            state->tag = tag;
        }
    }
    uint32_t peerRoundTripTime(PeerHandle peer) const override
    {
        if(const WebRtcPeerState* state = findPeerByHandle(peer)) {
            return state->pingMs;
        }
        return 0u;
    }
    uint32_t peerRoundTripVariance(PeerHandle peer) const override
    {
        if(const WebRtcPeerState* state = findPeerByHandle(peer)) {
            return state->jitterMs;
        }
        return 0u;
    }
    bool isActive() const override { return m_active; }
};

} // namespace

const char* netTransportBackendLabel(NetTransportBackend backend)
{
    switch(backend) {
        case NetTransportBackend::ENet: return "ENet";
        case NetTransportBackend::WebRTC: return "WebRTC";
        default: return "Unknown";
    }
}

std::vector<NetTransportBackend> availableNetTransportBackends()
{
#if defined(__EMSCRIPTEN__)
    return {NetTransportBackend::WebRTC};
#else
    return {NetTransportBackend::ENet, NetTransportBackend::WebRTC};
#endif
}

NetTransportBackend defaultNetTransportBackend()
{
#if defined(__EMSCRIPTEN__)
    return NetTransportBackend::WebRTC;
#else
    return NetTransportBackend::ENet;
#endif
}

NetTransport::NetTransport(NetTransportBackend backend)
    : m_backend(backend)
{
}

NetTransport::~NetTransport() = default;

bool NetTransport::ensureImpl()
{
    if(m_impl) return true;

    switch(m_backend) {
#if !defined(__EMSCRIPTEN__)
        case NetTransportBackend::ENet:
            m_impl = std::make_unique<ENetTransport>();
            return true;
#else
        case NetTransportBackend::ENet:
            return false;
#endif

        case NetTransportBackend::WebRTC:
            m_impl = std::make_unique<WebRTCTransport>();
            return true;

        default:
            return false;
    }
}

bool NetTransport::setBackend(NetTransportBackend backend)
{
    if(m_impl && m_impl->isActive()) return false;
    if(m_impl) {
        m_impl->shutdown();
        m_impl.reset();
    }
    m_backend = backend;
    return true;
}

NetTransportBackend NetTransport::backend() const
{
    return m_backend;
}

void NetTransport::setOptions(const NetTransportOptions& options)
{
    if(ensureImpl()) {
        m_impl->setOptions(options);
    }
}

const NetTransportOptions& NetTransport::options() const
{
    static const NetTransportOptions emptyOptions;
    return m_impl ? m_impl->options() : emptyOptions;
}

const std::string& NetTransport::lastError() const
{
    static const std::string emptyError;
    return m_impl ? m_impl->lastError() : emptyError;
}

const std::vector<std::string>& NetTransport::advertisedIceServers() const
{
    static const std::vector<std::string> empty;
    return m_impl ? m_impl->advertisedIceServers() : empty;
}

bool NetTransport::initialize()
{
    return ensureImpl() && m_impl->initialize();
}

void NetTransport::shutdown()
{
    if(m_impl) {
        m_impl->shutdown();
    }
}

bool NetTransport::hostSession(uint16_t port, size_t maxPeers)
{
    return ensureImpl() && m_impl->hostSession(port, maxPeers);
}

bool NetTransport::connectToHost(const std::string& hostName, uint16_t port, size_t channelCount)
{
    return ensureImpl() && m_impl->connectToHost(hostName, port, channelCount);
}

void NetTransport::disconnectAll(uint32_t data)
{
    if(m_impl) {
        m_impl->disconnectAll(data);
    }
}

void NetTransport::disconnectPeer(PeerHandle peer, uint32_t data)
{
    if(m_impl) {
        m_impl->disconnectPeer(peer, data);
    }
}

void NetTransport::flush()
{
    if(m_impl) {
        m_impl->flush();
    }
}

std::vector<NetTransport::PeerHandle> NetTransport::connectedPeers() const
{
    return m_impl ? m_impl->connectedPeers() : std::vector<PeerHandle>{};
}

std::vector<NetTransport::Event> NetTransport::poll(uint32_t timeoutMs)
{
    return m_impl ? m_impl->poll(timeoutMs) : std::vector<Event>{};
}

bool NetTransport::sendReliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload)
{
    return m_impl && m_impl->sendReliable(peer, channel, payload);
}

bool NetTransport::sendUnreliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload)
{
    return m_impl && m_impl->sendUnreliable(peer, channel, payload);
}

bool NetTransport::broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer)
{
    return m_impl && m_impl->broadcastReliable(channel, payload, exceptPeer);
}

bool NetTransport::broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer)
{
    return m_impl && m_impl->broadcastUnreliable(channel, payload, exceptPeer);
}

uintptr_t NetTransport::peerTag(PeerHandle peer) const
{
    return m_impl ? m_impl->peerTag(peer) : 0;
}

void NetTransport::setPeerTag(PeerHandle peer, uintptr_t tag)
{
    if(m_impl) {
        m_impl->setPeerTag(peer, tag);
    }
}

uint32_t NetTransport::peerRoundTripTime(PeerHandle peer) const
{
    return m_impl ? m_impl->peerRoundTripTime(peer) : 0u;
}

uint32_t NetTransport::peerRoundTripVariance(PeerHandle peer) const
{
    return m_impl ? m_impl->peerRoundTripVariance(peer) : 0u;
}

bool NetTransport::isActive() const
{
    return m_impl && m_impl->isActive();
}

} // namespace Netplay
