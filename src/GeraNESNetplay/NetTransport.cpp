#include "GeraNESNetplay/NetTransport.h"
#include "GeraNESNetplay/WebRtcPeerConnection.h"
#include "GeraNESNetplay/WebRtcSignalingClient.h"
#include "GeraNESNetplay/WebRtcSignalingServer.h"

#include <chrono>
#include <deque>
#include <optional>
#include <thread>

#if !defined(__EMSCRIPTEN__)
#include <enet/enet.h>
#endif

namespace Netplay {

namespace {

constexpr auto kWebRtcSignalingBootstrapTimeout = std::chrono::milliseconds(1500);

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
    static constexpr auto kWebRtcPeerHandshakeTimeout = std::chrono::seconds(5);
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
    };

    NetTransportOptions m_options;
    std::string m_lastError;
    std::unique_ptr<IWebRtcSignalingServer> m_signalingServer;
    std::unique_ptr<IWebRtcSignalingClient> m_signalingClient;
    std::deque<IWebRtcSignalingClient::Event> m_pendingSignalingEvents;
    std::string m_localPeerId;
    std::optional<WebRtcSignalingConfig> m_activeSignalingConfig;
    std::vector<std::string> m_signaledIceServers;
    std::optional<WebRtcPeerState> m_peer;
    PeerHandle m_nextPeerHandle = 1;
    bool m_signalingReady = false;
    bool m_hosting = false;
    bool m_active = false;
    std::optional<std::chrono::steady_clock::time_point> m_pendingPeerCloseDeadline;
#if defined(__EMSCRIPTEN__)
    bool m_bootstrapPending = false;
    bool m_bootstrapHost = false;
    bool m_bootstrapHelloSent = false;
    bool m_bootstrapSawWelcome = false;
    bool m_bootstrapSawRoomJoined = false;
#endif
    PeerHandshakeState m_peerHandshakeState = PeerHandshakeState::Idle;
    std::optional<std::chrono::steady_clock::time_point> m_peerHandshakeDeadline;

    void clearRuntimeState()
    {
        if(m_peer.has_value() && m_peer->connection) {
            m_peer->connection->close();
        }
        m_peer.reset();
        m_signalingReady = false;
        m_localPeerId.clear();
        m_signaledIceServers.clear();
        m_pendingSignalingEvents.clear();
        m_nextPeerHandle = 1;
        m_hosting = false;
        m_active = false;
        m_pendingPeerCloseDeadline.reset();
#if defined(__EMSCRIPTEN__)
        m_bootstrapPending = false;
        m_bootstrapHost = false;
        m_bootstrapHelloSent = false;
        m_bootstrapSawWelcome = false;
        m_bootstrapSawRoomJoined = false;
#endif
        m_peerHandshakeState = PeerHandshakeState::Idle;
        m_peerHandshakeDeadline.reset();
    }

    void startPeerHandshake(PeerHandshakeState state)
    {
        m_peerHandshakeState = state;
        m_peerHandshakeDeadline = std::chrono::steady_clock::now() + kWebRtcPeerHandshakeTimeout;
    }

    void resetPeerHandshakeState()
    {
        m_peerHandshakeState = PeerHandshakeState::Idle;
        m_peerHandshakeDeadline.reset();
    }

    void noteHandshakeProgress(PeerHandshakeState state)
    {
        m_peerHandshakeState = state;
        if(m_peerHandshakeDeadline.has_value()) {
            m_peerHandshakeDeadline = std::chrono::steady_clock::now() + kWebRtcPeerHandshakeTimeout;
        }
    }

    void updateHandshakeTimeout()
    {
        if(!m_active || !m_signalingReady || !m_peerHandshakeDeadline.has_value()) {
            return;
        }
        if(m_peerHandshakeState == PeerHandshakeState::Idle ||
           m_peerHandshakeState == PeerHandshakeState::Connected) {
            return;
        }
        if(std::chrono::steady_clock::now() < *m_peerHandshakeDeadline) {
            return;
        }
        if(!m_lastError.empty()) {
            return;
        }

        std::string stage;
        switch(m_peerHandshakeState) {
            case PeerHandshakeState::WaitingForOffer:
                stage = "waiting for WebRTC offer from host";
                break;
            case PeerHandshakeState::CreatingOffer:
                stage = "creating WebRTC offer";
                break;
            case PeerHandshakeState::WaitingForAnswer:
                stage = "waiting for WebRTC answer from client";
                break;
            case PeerHandshakeState::Negotiating:
                stage = "waiting for ICE/data channel negotiation";
                break;
            case PeerHandshakeState::Idle:
            case PeerHandshakeState::Connected:
            default:
                stage = "waiting for peer handshake";
                break;
        }

        m_lastError = "WebRTC peer handshake timed out (" + stage + ")";
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
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
        return std::string(host ? "host-" : "peer-") + std::to_string(static_cast<long long>(ticks));
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

    bool bootstrapSignaling(bool host)
    {
        clearRuntimeState();

        if(!ensureSignalingClient()) {
            m_lastError = "WebRTC signaling client could not be created";
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

#if defined(__EMSCRIPTEN__)
        if(!m_signalingClient->connect(options)) {
            m_lastError = m_signalingClient->lastError();
            return false;
        }

        m_bootstrapPending = true;
        m_bootstrapHost = host;
        m_bootstrapHelloSent = false;
        m_bootstrapSawWelcome = false;
        m_bootstrapSawRoomJoined = false;
        return true;
#else
        if(!m_signalingClient->connect(options)) {
            m_lastError = m_signalingClient->lastError();
            return false;
        }

        WebRtcSignalingMessage hello;
        hello.type = WebRtcSignalType::Hello;
        hello.roomId = options.config.roomId;
        hello.peerId = m_localPeerId;
        if(!m_signalingClient->send(hello)) {
            m_lastError = m_signalingClient->lastError();
            m_signalingClient->disconnect();
            return false;
        }

        WebRtcSignalingMessage joinRoom;
        joinRoom.type = WebRtcSignalType::JoinRoom;
        joinRoom.roomId = options.config.roomId;
        joinRoom.peerId = m_localPeerId;
        if(!m_signalingClient->send(joinRoom)) {
            m_lastError = m_signalingClient->lastError();
            m_signalingClient->disconnect();
            return false;
        }

        const auto deadline = std::chrono::steady_clock::now() + kWebRtcSignalingBootstrapTimeout;
        bool sawWelcome = false;
        bool sawRoomJoined = false;

        while(std::chrono::steady_clock::now() < deadline && (!sawWelcome || !sawRoomJoined)) {
            for(const auto& event : m_signalingClient->poll()) {
                if(event.type == IWebRtcSignalingClient::Event::Type::Error) {
                    m_lastError = !event.text.empty() ? event.text : m_signalingClient->lastError();
                    m_signalingClient->disconnect();
                    return false;
                }

                if(event.type != IWebRtcSignalingClient::Event::Type::Message) {
                    continue;
                }

                if(event.message.type == WebRtcSignalType::Welcome) {
                    sawWelcome = true;
                    if(!event.message.iceServers.empty()) {
                        m_signaledIceServers = event.message.iceServers;
                    }
                } else if(event.message.type == WebRtcSignalType::RoomJoined &&
                          event.message.roomId == options.config.roomId) {
                    sawRoomJoined = true;
                    if(!event.message.iceServers.empty()) {
                        m_signaledIceServers = event.message.iceServers;
                    }
                } else if(event.message.type == WebRtcSignalType::Error) {
                    m_lastError = !event.message.error.empty() ? event.message.error : "WebRTC signaling reported an error";
                    m_signalingClient->disconnect();
                    return false;
                } else {
                    m_pendingSignalingEvents.push_back(event);
                }
            }

            if(!sawWelcome || !sawRoomJoined) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        if(!sawWelcome || !sawRoomJoined) {
            m_lastError = "WebRTC signaling handshake timed out";
            m_signalingClient->disconnect();
            return false;
        }

        m_signalingReady = true;
        if(!host) {
            startPeerHandshake(PeerHandshakeState::WaitingForOffer);
        }
        return true;
#endif
    }

    WebRtcPeerState* findPeerByHandle(PeerHandle handle)
    {
        if(!m_peer.has_value() || m_peer->handle != handle) return nullptr;
        return &*m_peer;
    }

    WebRtcPeerState* findPeerByRemoteId(const std::string& remotePeerId)
    {
        if(!m_peer.has_value() || m_peer->remotePeerId != remotePeerId) return nullptr;
        return &*m_peer;
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
            return false;
        }

        WebRtcPeerConnectionOptions options;
        options.localPeerId = m_localPeerId;
        options.remotePeerId = remotePeerId;
        options.iceServers = m_signaledIceServers;
        options.host = host;

        if(!state.connection->open(options)) {
            m_lastError = state.connection->lastError();
            return false;
        }

        m_peer = std::move(state);
        return true;
    }

    void closePeer(WebRtcPeerState& peer, std::vector<Event>* events = nullptr)
    {
        if(peer.connected && events != nullptr) {
            Event disconnected;
            disconnected.type = Event::Type::Disconnected;
            disconnected.peer = peer.handle;
            events->push_back(std::move(disconnected));
        }
        if(peer.connection) {
            peer.connection->close();
        }
        m_peer.reset();
        resetPeerHandshakeState();
        m_lastError.clear();
        m_pendingPeerCloseDeadline.reset();
    }

    void schedulePeerClose()
    {
        if(!m_peer.has_value()) {
            return;
        }
        m_pendingPeerCloseDeadline = std::chrono::steady_clock::now() + kWebRtcGracefulDisconnectDelay;
    }

    void processPendingPeerClose(std::vector<Event>& events)
    {
        if(!m_pendingPeerCloseDeadline.has_value() || !m_peer.has_value()) {
            return;
        }
        if(std::chrono::steady_clock::now() < *m_pendingPeerCloseDeadline) {
            return;
        }
        closePeer(*m_peer, &events);
    }

    bool shouldIgnoreExpectedPeerError(const std::string& error) const
    {
        if(error.empty()) {
            return false;
        }

        if(!m_pendingPeerCloseDeadline.has_value()) {
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

    void drainPeerEvents(WebRtcPeerState& peer, std::vector<Event>& events)
    {
        if(!peer.connection) return;

        for(const auto& event : peer.connection->poll()) {
            switch(event.type) {
                case IWebRtcPeerConnection::Event::Type::LocalDescriptionReady: {
                    WebRtcSignalingMessage message;
                    message.type = event.descriptionIsOffer ? WebRtcSignalType::Offer : WebRtcSignalType::Answer;
                    if(event.descriptionIsOffer) {
                        noteHandshakeProgress(PeerHandshakeState::WaitingForAnswer);
                    } else {
                        noteHandshakeProgress(PeerHandshakeState::Negotiating);
                    }
                    message.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    message.peerId = m_localPeerId;
                    message.targetPeerId = peer.remotePeerId;
                    message.sdp = event.sdp;
                    sendSignalingMessage(message);
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::IceCandidateReady: {
                    WebRtcSignalingMessage message;
                    message.type = WebRtcSignalType::IceCandidate;
                    noteHandshakeProgress(PeerHandshakeState::Negotiating);
                    message.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    message.peerId = m_localPeerId;
                    message.targetPeerId = peer.remotePeerId;
                    message.candidate = event.candidate;
                    message.mid = event.mid;
                    message.mlineIndex = event.mlineIndex;
                    sendSignalingMessage(message);
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::DataChannelOpen: {
                    if(!peer.connected) {
                        peer.connected = true;
                        m_peerHandshakeState = PeerHandshakeState::Connected;
                        m_peerHandshakeDeadline.reset();
                        Event connected;
                        connected.type = Event::Type::Connected;
                        connected.peer = peer.handle;
                        events.push_back(std::move(connected));
                    }
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::DataChannelClosed: {
                    if(peer.connected) {
                        peer.connected = false;
                        Event disconnected;
                        disconnected.type = Event::Type::Disconnected;
                        disconnected.peer = peer.handle;
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
                    packet.peer = peer.handle;
                    packet.channel = static_cast<Channel>(event.payload.front());
                    packet.payload.assign(event.payload.begin() + 1, event.payload.end());
                    events.push_back(std::move(packet));
                    break;
                }

                case IWebRtcPeerConnection::Event::Type::Error:
                {
                    const std::string error = event.text.empty() ? peer.connection->lastError() : event.text;
                    if(!shouldIgnoreExpectedPeerError(error)) {
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
                if(m_peer.has_value()) {
                    closePeer(*m_peer, &events);
                }
#if defined(__EMSCRIPTEN__)
                if(m_bootstrapPending && m_lastError.empty()) {
                    m_lastError = "WebRTC signaling disconnected during connection setup";
                }
                m_bootstrapPending = false;
                m_bootstrapHelloSent = false;
                m_bootstrapSawWelcome = false;
                m_bootstrapSawRoomJoined = false;
#endif
                m_signalingReady = false;
                m_active = false;
                continue;
            }

            if(event.type == IWebRtcSignalingClient::Event::Type::Error) {
                m_lastError = !event.text.empty() ? event.text : m_signalingClient->lastError();
                continue;
            }

            if(event.type == IWebRtcSignalingClient::Event::Type::Connected) {
#if defined(__EMSCRIPTEN__)
                if(m_bootstrapPending && !m_bootstrapHelloSent) {
                    WebRtcSignalingMessage hello;
                    hello.type = WebRtcSignalType::Hello;
                    hello.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    hello.peerId = m_localPeerId;
                    if(!sendSignalingMessage(hello)) {
                        m_bootstrapPending = false;
                        continue;
                    }

                    WebRtcSignalingMessage joinRoom;
                    joinRoom.type = WebRtcSignalType::JoinRoom;
                    joinRoom.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    joinRoom.peerId = m_localPeerId;
                    if(!sendSignalingMessage(joinRoom)) {
                        m_bootstrapPending = false;
                        continue;
                    }

                    m_bootstrapHelloSent = true;
                }
#endif
                continue;
            }

            if(event.type != IWebRtcSignalingClient::Event::Type::Message) {
                continue;
            }

            const WebRtcSignalingMessage& message = event.message;
#if defined(__EMSCRIPTEN__)
            if(m_bootstrapPending) {
                if(message.type == WebRtcSignalType::Welcome) {
                    m_bootstrapSawWelcome = true;
                    if(!message.iceServers.empty()) {
                        m_signaledIceServers = message.iceServers;
                    }
                } else if(message.type == WebRtcSignalType::RoomJoined &&
                          m_activeSignalingConfig.has_value() &&
                          message.roomId == m_activeSignalingConfig->roomId) {
                    m_bootstrapSawRoomJoined = true;
                    if(!message.iceServers.empty()) {
                        m_signaledIceServers = message.iceServers;
                    }
                } else if(message.type == WebRtcSignalType::Error) {
                    m_lastError = !message.error.empty() ? message.error : "WebRTC signaling reported an error";
                    m_bootstrapPending = false;
                    continue;
                } else {
                    m_pendingSignalingEvents.push_back(event);
                }

                if(m_bootstrapSawWelcome && m_bootstrapSawRoomJoined) {
                    m_signalingReady = true;
                    m_bootstrapPending = false;
                    if(!m_bootstrapHost) {
                        startPeerHandshake(PeerHandshakeState::WaitingForOffer);
                    }
                }
                continue;
            }
#endif
            switch(message.type) {
                case WebRtcSignalType::PeerJoined:
                    if(m_hosting && message.peerId != m_localPeerId) {
                        startPeerHandshake(PeerHandshakeState::CreatingOffer);
                        if(initializePeerConnection(message.peerId, true)) {
                            WebRtcPeerState* peer = findPeerByRemoteId(message.peerId);
                            if(peer && peer->connection && !peer->connection->createOffer()) {
                                m_lastError = peer->connection->lastError();
                            }
                        }
                    }
                    break;

                case WebRtcSignalType::PeerLeft:
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        closePeer(*peer, &events);
                    }
                    break;

                case WebRtcSignalType::Offer:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    noteHandshakeProgress(PeerHandshakeState::Negotiating);
                    if(!initializePeerConnection(message.peerId, false)) {
                        break;
                    }
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        if(!peer->connection->setRemoteDescription(message.sdp, true)) {
                            m_lastError = peer->connection->lastError();
                            break;
                        }
                        if(!peer->connection->createAnswer()) {
                            m_lastError = peer->connection->lastError();
                        }
                    }
                    break;

                case WebRtcSignalType::Answer:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    noteHandshakeProgress(PeerHandshakeState::Negotiating);
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        if(!peer->connection->setRemoteDescription(message.sdp, false)) {
                            m_lastError = peer->connection->lastError();
                        }
                    }
                    break;

                case WebRtcSignalType::IceCandidate:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    noteHandshakeProgress(PeerHandshakeState::Negotiating);
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        if(!peer->connection->addRemoteIceCandidate(message.candidate, message.mid, message.mlineIndex)) {
                            m_lastError = peer->connection->lastError();
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
        if(m_signalingClient) {
            m_signalingClient->disconnect();
        }
        if(m_signalingServer) {
            m_signalingServer->stop();
        }
        clearRuntimeState();
    }
    void setOptions(const NetTransportOptions& options) override { m_options = options; }
    const NetTransportOptions& options() const override { return m_options; }
    const std::string& lastError() const override { return m_lastError; }
    bool hostSession(uint16_t port, size_t) override
    {
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
            if(m_signalingServer) {
                m_signalingServer->stop();
            }
            return false;
        }

        m_hosting = true;
        m_active = true;
        m_lastError.clear();
        return true;
    }
    bool connectToHost(const std::string& hostName, uint16_t port, size_t = 3) override
    {
        m_activeSignalingConfig = resolveSignalingConfig(false, hostName, port);
        if(!m_activeSignalingConfig.has_value()) {
            m_lastError = "Configure signaling URL and room id for WebRTC";
            return false;
        }

        if(!bootstrapSignaling(false)) {
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
                schedulePeerClose();
            } else {
                closePeer(*state);
            }
        }
    }
    void flush() override {}
    std::vector<PeerHandle> connectedPeers() const override
    {
        if(m_peer.has_value() && m_peer->connected) {
            return {m_peer->handle};
        }
        return {};
    }
    std::vector<Event> poll(uint32_t timeoutMs) override
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        std::vector<Event> events;

        do {
            processSignalingEvents(events);
            if(m_peer.has_value()) {
                drainPeerEvents(*m_peer, events);
            }
            processPendingPeerClose(events);
            if(!events.empty() || timeoutMs == 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while(std::chrono::steady_clock::now() < deadline);

        updateHandshakeTimeout();

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
        if(!m_peer.has_value() || m_peer->handle == exceptPeer) return false;
        return sendReliable(m_peer->handle, channel, payload);
    }
    bool broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) override
    {
        if(!m_peer.has_value() || m_peer->handle == exceptPeer) return false;
        return sendUnreliable(m_peer->handle, channel, payload);
    }
    uintptr_t peerTag(PeerHandle peer) const override
    {
        return (m_peer.has_value() && m_peer->handle == peer) ? m_peer->tag : 0;
    }
    void setPeerTag(PeerHandle peer, uintptr_t tag) override
    {
        if(m_peer.has_value() && m_peer->handle == peer) {
            m_peer->tag = tag;
        }
    }
    uint32_t peerRoundTripTime(PeerHandle peer) const override
    {
        return (m_peer.has_value() && m_peer->handle == peer) ? m_peer->pingMs : 0u;
    }
    uint32_t peerRoundTripVariance(PeerHandle peer) const override
    {
        return (m_peer.has_value() && m_peer->handle == peer) ? m_peer->jitterMs : 0u;
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
