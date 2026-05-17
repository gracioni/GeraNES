#include "ConsoleNetplay/NetTransport.h"
#include "ConsoleNetplay/WebRtcPeerConnection.h"
#include "ConsoleNetplay/WebRtcSignalingClient.h"
#include "ConsoleNetplay/WebRtcSignalingServer.h"
#include "ConsoleNetplay/NetplayLog.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>

#if !defined(__EMSCRIPTEN__)
#include <enet/enet.h>
#endif

namespace ConsoleNetplay {

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

std::string trimAsciiWhitespace(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    });
    if(first == value.end()) {
        return {};
    }

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }).base();
    return std::string(first, last);
}

std::string toLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isUnsupportedTurnTransport(const std::string& iceServer)
{
    const std::string normalized = toLowerAscii(iceServer);
    if(normalized.rfind("turns:", 0) == 0) {
        return true;
    }
    if(normalized.rfind("turn:", 0) != 0) {
        return false;
    }
    return normalized.find("transport=tcp") != std::string::npos ||
           normalized.find("transport=tls") != std::string::npos;
}

std::vector<std::string> sanitizeAdvertisedIceServers(const std::vector<std::string>& iceServers)
{
    std::vector<std::string> sanitized;
    sanitized.reserve(iceServers.size());

    for(const std::string& entry : iceServers) {
        const std::string trimmed = trimAsciiWhitespace(entry);
        if(trimmed.empty()) {
            continue;
        }
        if(isUnsupportedTurnTransport(trimmed)) {
            continue;
        }
        if(std::find(sanitized.begin(), sanitized.end(), trimmed) == sanitized.end()) {
            sanitized.push_back(trimmed);
        }
    }

    if(sanitized.empty()) {
        for(const std::string& entry : iceServers) {
            const std::string trimmed = trimAsciiWhitespace(entry);
            if(!trimmed.empty() &&
               std::find(sanitized.begin(), sanitized.end(), trimmed) == sanitized.end()) {
                sanitized.push_back(trimmed);
            }
        }
    }

    return sanitized;
}

#if !defined(__EMSCRIPTEN__)
std::mutex g_enetLifecycleMutex;
uint32_t g_enetLifecycleRefs = 0;

bool acquireENetLifecycle()
{
    std::scoped_lock lock(g_enetLifecycleMutex);
    if(g_enetLifecycleRefs == 0 && enet_initialize() != 0) {
        return false;
    }
    ++g_enetLifecycleRefs;
    return true;
}

void releaseENetLifecycle()
{
    std::scoped_lock lock(g_enetLifecycleMutex);
    if(g_enetLifecycleRefs == 0) {
        return;
    }
    --g_enetLifecycleRefs;
    if(g_enetLifecycleRefs == 0) {
        enet_deinitialize();
    }
}

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
        if(!acquireENetLifecycle()) {
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
            releaseENetLifecycle();
            m_initialized = false;
        }
    }

    void shutdownForUnload() override
    {
        shutdown();
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
    static constexpr size_t kWebRtcBufferedAmountHighWaterBytes = 256 * 1024;
    static constexpr size_t kWebRtcBufferedAmountResumeBytes = 64 * 1024;
    static constexpr size_t kWebRtcMaxQueuedSendsPerPeerPerPoll = 32;

    enum class ShutdownMode : uint8_t
    {
        Graceful,
        Unload
    };

    enum class PeerHandshakeState : uint8_t
    {
        Idle,
        WaitingForOffer,
        CreatingOffer,
        WaitingForAnswer,
        Negotiating,
        Connected
    };

    enum class TransportLifecycle : uint8_t
    {
        Idle,
        Bootstrapping,
        Active,
        DegradedSignaling,
        Closing
    };

    struct WebRtcPeerState
    {
        struct QueuedOutboundPacket
        {
            std::vector<uint8_t> payload;
            bool reliable = true;
            Channel channel = Channel::Control;
        };

        PeerHandle handle = kInvalidPeerHandle;
        std::string remotePeerId;
        std::unique_ptr<IWebRtcPeerConnection> connection;
        uintptr_t tag = 0;
        uint32_t pingMs = 0;
        uint32_t jitterMs = 0;
        bool connected = false;
        bool disconnectEmitted = false;
        bool closeRequested = false;
        IWebRtcPeerConnection::ConnectionPath lastLoggedConnectionPath =
            IWebRtcPeerConnection::ConnectionPath::Unknown;
        PeerHandshakeState handshakeState = PeerHandshakeState::Idle;
        std::optional<std::chrono::steady_clock::time_point> handshakeDeadline;
        std::optional<std::chrono::steady_clock::time_point> closeDeadline;
        std::deque<QueuedOutboundPacket> pendingOutboundPackets;
    };

    struct SessionEvent
    {
        enum class Type : uint8_t
        {
            None,
            SignalingConnected,
            SignalingDisconnected,
            SignalingError,
            SignalingMessage,
            PeerLocalDescriptionReady,
            PeerIceCandidateReady,
            PeerDataChannelOpen,
            PeerDataChannelClosed,
            PeerDataMessage,
            PeerError
        };

        Type type = Type::None;
        PeerHandle peer = kInvalidPeerHandle;
        WebRtcSignalingMessage signalingMessage = {};
        std::string text;
        std::string sdp;
        bool descriptionIsOffer = false;
        std::string candidate;
        std::string mid;
        int mlineIndex = -1;
        std::vector<uint8_t> payload;
    };

    struct DetachedRuntimeResources
    {
        std::vector<std::unique_ptr<IWebRtcPeerConnection>> peerConnections;
        std::unique_ptr<IWebRtcSignalingClient> signalingClient;
        std::unique_ptr<IWebRtcSignalingServer> signalingServer;
    };

    NetTransportOptions m_options;
    std::string m_lastError;
    struct SessionRuntime
    {
        std::unique_ptr<IWebRtcSignalingServer> signalingServer;
        std::unique_ptr<IWebRtcSignalingClient> signalingClient;
        std::deque<IWebRtcSignalingClient::Event> pendingSignalingEvents;
        std::deque<SessionEvent> sessionEvents;
        std::string localPeerId;
        std::optional<WebRtcSignalingConfig> activeSignalingConfig;
        std::vector<std::string> signaledIceServers;
        std::string hostRemotePeerId;
        std::vector<WebRtcPeerState> peers;
        std::vector<PeerHandle> requestedPeerCloses;
        PeerHandle nextPeerHandle = 1;
        size_t requestedMaxPeers = 1;
        TransportLifecycle lifecycle = TransportLifecycle::Idle;
        bool signalingReady = false;
        bool hosting = false;
        bool active = false;
        bool waitingForInitialOffer = false;
        std::optional<std::chrono::steady_clock::time_point> initialOfferDeadline;
        std::optional<std::chrono::steady_clock::time_point> bootstrapDeadline;
        bool bootstrapPending = false;
        bool bootstrapHost = false;
        bool bootstrapHelloSent = false;
        bool bootstrapRoomRequestSent = false;
        bool bootstrapSawWelcome = false;
        bool bootstrapSawRoomJoined = false;
        bool closeAllPeersRequested = false;
        std::optional<std::string> pendingFatalShutdownError;

        void queueSignalingEvent(const IWebRtcSignalingClient::Event& event)
        {
            SessionEvent queuedEvent;
            switch(event.type) {
                case IWebRtcSignalingClient::Event::Type::Connected:
                    queuedEvent.type = SessionEvent::Type::SignalingConnected;
                    sessionEvents.push_back(std::move(queuedEvent));
                    return;
                case IWebRtcSignalingClient::Event::Type::Disconnected:
                    queuedEvent.type = SessionEvent::Type::SignalingDisconnected;
                    sessionEvents.push_back(std::move(queuedEvent));
                    return;
                case IWebRtcSignalingClient::Event::Type::Error:
                    queuedEvent.type = SessionEvent::Type::SignalingError;
                    queuedEvent.text = event.text;
                    sessionEvents.push_back(std::move(queuedEvent));
                    return;
                case IWebRtcSignalingClient::Event::Type::Message:
                    queuedEvent.type = SessionEvent::Type::SignalingMessage;
                    queuedEvent.signalingMessage = event.message;
                    sessionEvents.push_back(std::move(queuedEvent));
                    return;
                default:
                    return;
            }
        }

        void queuePeerEvent(PeerHandle peerHandle, const IWebRtcPeerConnection::Event& event)
        {
            SessionEvent queuedEvent;
            queuedEvent.peer = peerHandle;
            switch(event.type) {
                case IWebRtcPeerConnection::Event::Type::LocalDescriptionReady:
                    queuedEvent.type = SessionEvent::Type::PeerLocalDescriptionReady;
                    queuedEvent.sdp = event.sdp;
                    queuedEvent.descriptionIsOffer = event.descriptionIsOffer;
                    break;
                case IWebRtcPeerConnection::Event::Type::IceCandidateReady:
                    queuedEvent.type = SessionEvent::Type::PeerIceCandidateReady;
                    queuedEvent.candidate = event.candidate;
                    queuedEvent.mid = event.mid;
                    queuedEvent.mlineIndex = event.mlineIndex;
                    break;
                case IWebRtcPeerConnection::Event::Type::DataChannelOpen:
                    queuedEvent.type = SessionEvent::Type::PeerDataChannelOpen;
                    break;
                case IWebRtcPeerConnection::Event::Type::DataChannelClosed:
                    queuedEvent.type = SessionEvent::Type::PeerDataChannelClosed;
                    break;
                case IWebRtcPeerConnection::Event::Type::DataMessage:
                    queuedEvent.type = SessionEvent::Type::PeerDataMessage;
                    queuedEvent.payload = event.payload;
                    break;
                case IWebRtcPeerConnection::Event::Type::Error:
                    queuedEvent.type = SessionEvent::Type::PeerError;
                    queuedEvent.text = event.text;
                    break;
                default:
                    return;
            }
            sessionEvents.push_back(std::move(queuedEvent));
        }

        void collectSignalingEvents()
        {
            if(!signalingClient) {
                return;
            }

            std::deque<IWebRtcSignalingClient::Event> signalingEvents = std::move(pendingSignalingEvents);
            for(const auto& event : signalingClient->poll()) {
                signalingEvents.push_back(event);
            }

            for(const auto& event : signalingEvents) {
                queueSignalingEvent(event);
            }
        }

        void collectPeerEvents(TransportLifecycle lifecycle)
        {
            if(lifecycle == TransportLifecycle::Closing) {
                return;
            }

            for(WebRtcPeerState& peer : peers) {
                if(!peer.connection || peer.closeRequested) {
                    continue;
                }
                for(const auto& event : peer.connection->poll()) {
                    queuePeerEvent(peer.handle, event);
                }
            }
        }

        template<typename Fn>
        void drainQueuedEvents(Fn&& handler)
        {
            while(!sessionEvents.empty()) {
                SessionEvent event = std::move(sessionEvents.front());
                sessionEvents.pop_front();
                handler(std::move(event));
            }
        }

        void deferSignalingMessage(const WebRtcSignalingMessage& message)
        {
            IWebRtcSignalingClient::Event deferredEvent;
            deferredEvent.type = IWebRtcSignalingClient::Event::Type::Message;
            deferredEvent.message = message;
            pendingSignalingEvents.push_back(std::move(deferredEvent));
        }

        WebRtcPeerState* findPeerByHandle(PeerHandle handle)
        {
            auto it = std::find_if(peers.begin(), peers.end(),
                                   [handle](const WebRtcPeerState& peer) { return peer.handle == handle; });
            return it != peers.end() ? &*it : nullptr;
        }

        void requestFatalShutdown(std::string error)
        {
            if(!pendingFatalShutdownError.has_value()) {
                pendingFatalShutdownError = std::move(error);
            }
        }

        std::optional<std::string> takeFatalShutdownError()
        {
            if(!pendingFatalShutdownError.has_value()) {
                return std::nullopt;
            }
            std::optional<std::string> error = std::move(pendingFatalShutdownError);
            pendingFatalShutdownError.reset();
            return error;
        }

        void requestPeerClose(PeerHandle handle)
        {
            if(handle == kInvalidPeerHandle) {
                return;
            }
            if(WebRtcPeerState* peer = findPeerByHandle(handle)) {
                peer->closeRequested = true;
                peer->handshakeDeadline.reset();
                if(!peer->closeDeadline.has_value()) {
                    peer->closeDeadline = std::chrono::steady_clock::now();
                }
            }
            if(std::find(requestedPeerCloses.begin(), requestedPeerCloses.end(), handle) == requestedPeerCloses.end()) {
                requestedPeerCloses.push_back(handle);
            }
        }

        void schedulePeerClose(PeerHandle handle, std::chrono::steady_clock::time_point deadline)
        {
            if(WebRtcPeerState* peer = findPeerByHandle(handle)) {
                peer->closeRequested = true;
                peer->handshakeDeadline.reset();
                peer->closeDeadline = deadline;
            }
        }

        void requestCloseAllPeers()
        {
            closeAllPeersRequested = true;
        }

        std::vector<PeerHandle> takePendingPeerCloses(const std::chrono::steady_clock::time_point now,
                                                      bool& closeAllPeers)
        {
            closeAllPeers = closeAllPeersRequested;
            closeAllPeersRequested = false;

            std::vector<PeerHandle> handles;
            if(closeAllPeers) {
                handles.reserve(peers.size());
                for(const WebRtcPeerState& peer : peers) {
                    handles.push_back(peer.handle);
                }
                requestedPeerCloses.clear();
                return handles;
            }

            for(const WebRtcPeerState& peer : peers) {
                if(peer.closeDeadline.has_value() && now >= *peer.closeDeadline) {
                    handles.push_back(peer.handle);
                }
            }
            for(PeerHandle handle : requestedPeerCloses) {
                if(std::find(handles.begin(), handles.end(), handle) == handles.end()) {
                    handles.push_back(handle);
                }
            }
            requestedPeerCloses.clear();
            return handles;
        }

        DetachedRuntimeResources detachResources()
        {
            DetachedRuntimeResources resources;
            resources.peerConnections.reserve(peers.size());
            for(WebRtcPeerState& peer : peers) {
                if(peer.connection) {
                    resources.peerConnections.push_back(std::move(peer.connection));
                }
            }
            resources.signalingClient = std::move(signalingClient);
            resources.signalingServer = std::move(signalingServer);
            return resources;
        }

        void clear(bool closePeerConnections)
        {
            if(closePeerConnections) {
                for(WebRtcPeerState& peer : peers) {
                    if(peer.connection) {
                        peer.connection->close();
                    }
                }
            }
            peers.clear();
            lifecycle = TransportLifecycle::Idle;
            signalingReady = false;
            localPeerId.clear();
            signaledIceServers.clear();
            hostRemotePeerId.clear();
            pendingSignalingEvents.clear();
            sessionEvents.clear();
            requestedPeerCloses.clear();
            nextPeerHandle = 1;
            hosting = false;
            active = false;
            waitingForInitialOffer = false;
            initialOfferDeadline.reset();
            bootstrapDeadline.reset();
            bootstrapPending = false;
            bootstrapHost = false;
            bootstrapHelloSent = false;
            bootstrapRoomRequestSent = false;
            bootstrapSawWelcome = false;
            bootstrapSawRoomJoined = false;
            closeAllPeersRequested = false;
            pendingFatalShutdownError.reset();
        }
    };

    SessionRuntime m_session;
    std::unique_ptr<IWebRtcSignalingServer>& m_signalingServer = m_session.signalingServer;
    std::unique_ptr<IWebRtcSignalingClient>& m_signalingClient = m_session.signalingClient;
    std::deque<IWebRtcSignalingClient::Event>& m_pendingSignalingEvents = m_session.pendingSignalingEvents;
    std::deque<SessionEvent>& m_sessionEvents = m_session.sessionEvents;
    std::string& m_localPeerId = m_session.localPeerId;
    std::optional<WebRtcSignalingConfig>& m_activeSignalingConfig = m_session.activeSignalingConfig;
    std::vector<std::string>& m_signaledIceServers = m_session.signaledIceServers;
    std::string& m_hostRemotePeerId = m_session.hostRemotePeerId;
    std::vector<WebRtcPeerState>& m_peers = m_session.peers;
    PeerHandle& m_nextPeerHandle = m_session.nextPeerHandle;
    size_t& m_requestedMaxPeers = m_session.requestedMaxPeers;
    TransportLifecycle& m_lifecycle = m_session.lifecycle;
    bool& m_signalingReady = m_session.signalingReady;
    bool& m_hosting = m_session.hosting;
    bool& m_active = m_session.active;
    bool& m_waitingForInitialOffer = m_session.waitingForInitialOffer;
    std::optional<std::chrono::steady_clock::time_point>& m_initialOfferDeadline = m_session.initialOfferDeadline;
    std::optional<std::chrono::steady_clock::time_point>& m_bootstrapDeadline = m_session.bootstrapDeadline;
    bool& m_bootstrapPending = m_session.bootstrapPending;
    bool& m_bootstrapHost = m_session.bootstrapHost;
    bool& m_bootstrapHelloSent = m_session.bootstrapHelloSent;
    bool& m_bootstrapRoomRequestSent = m_session.bootstrapRoomRequestSent;
    bool& m_bootstrapSawWelcome = m_session.bootstrapSawWelcome;
    bool& m_bootstrapSawRoomJoined = m_session.bootstrapSawRoomJoined;
    void logTrace(const std::string& message) const
    {
        logNetplayMessage("[WebRTC transport] " + message, NetplayLogLevel::Info);
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

    static const char* connectionPathLabel(IWebRtcPeerConnection::ConnectionPath path)
    {
        switch(path) {
            case IWebRtcPeerConnection::ConnectionPath::Direct:
                return "direct";
            case IWebRtcPeerConnection::ConnectionPath::TurnRelay:
                return "turn_relay";
            case IWebRtcPeerConnection::ConnectionPath::Unknown:
            default:
                return "unknown";
        }
    }

    void logPeerConnectionPathIfChanged(WebRtcPeerState& peer)
    {
        if(!peer.connection) {
            return;
        }

        const IWebRtcPeerConnection::ConnectionPath path = peer.connection->connectionPath();
        if(path == peer.lastLoggedConnectionPath) {
            return;
        }

        peer.lastLoggedConnectionPath = path;
        if(path == IWebRtcPeerConnection::ConnectionPath::Unknown) {
            return;
        }

        logTrace(
            std::string("connection path for ") +
            peer.remotePeerId +
            ": " +
            connectionPathLabel(path));
    }

    bool shouldCreateOfferForPeer(const std::string& remotePeerId) const
    {
        if(remotePeerId.empty() || remotePeerId == m_localPeerId) {
            return false;
        }

        const auto isHostPeerId = [](const std::string& peerId) {
            return peerId.rfind("host-", 0) == 0;
        };

        if(!m_hosting) {
            if(!m_hostRemotePeerId.empty()) {
                return remotePeerId == m_hostRemotePeerId;
            }
            return isHostPeerId(remotePeerId);
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
        m_session.clear(true);
    }

    DetachedRuntimeResources detachRuntimeResources()
    {
        return m_session.detachResources();
    }

    void requestFatalShutdown(const std::string& error)
    {
        if(m_lifecycle == TransportLifecycle::Closing) {
            return;
        }
        m_session.requestFatalShutdown(error);
    }

    bool enqueueOrSendPeerPayload(WebRtcPeerState& peer,
                                  Channel channel,
                                  std::vector<uint8_t> wrappedPayload,
                                  bool reliable)
    {
        if(!peer.connection || peer.closeRequested) {
            return false;
        }

        const auto queuePacket = [&](std::vector<uint8_t> queuedPayload) {
            WebRtcPeerState::QueuedOutboundPacket packet;
            packet.payload = std::move(queuedPayload);
            packet.reliable = reliable;
            packet.channel = channel;

            auto priorityForChannel = [](Channel queuedChannel) {
                switch(queuedChannel) {
                    case Channel::Control:
                        return 0;
                    case Channel::Diagnostics:
                        return 1;
                    case Channel::Gameplay:
                    default:
                        return 2;
                }
            };

            const int packetPriority = priorityForChannel(channel);
            auto insertIt = peer.pendingOutboundPackets.end();
            for(auto it = peer.pendingOutboundPackets.begin(); it != peer.pendingOutboundPackets.end(); ++it) {
                if(priorityForChannel(it->channel) > packetPriority) {
                    insertIt = it;
                    break;
                }
            }
            peer.pendingOutboundPackets.insert(insertIt, std::move(packet));
        };

        bool hasQueuedHigherPriorityTraffic = false;
        for(const WebRtcPeerState::QueuedOutboundPacket& pending : peer.pendingOutboundPackets) {
            if(pending.channel == Channel::Control && channel != Channel::Control) {
                hasQueuedHigherPriorityTraffic = true;
                break;
            }
            if(pending.channel == Channel::Diagnostics && channel == Channel::Gameplay) {
                hasQueuedHigherPriorityTraffic = true;
                break;
            }
        }

        const bool shouldQueue =
            !peer.connection->isDataChannelOpen() ||
            hasQueuedHigherPriorityTraffic ||
            !peer.pendingOutboundPackets.empty() ||
            peer.connection->bufferedAmount() > kWebRtcBufferedAmountHighWaterBytes;
        if(shouldQueue) {
            queuePacket(std::move(wrappedPayload));
            return true;
        }

        const bool sent = reliable
            ? peer.connection->sendDataReliable(wrappedPayload)
            : peer.connection->sendDataUnreliable(wrappedPayload);
        if(sent) {
            return true;
        }

        const std::string sendError = peer.connection->lastError();
        if(sendError.find("not open") != std::string::npos) {
            queuePacket(std::move(wrappedPayload));
            return true;
        }

        handlePeerFailure(
            peer,
            sendError.empty()
                ? std::string("WebRTC peer send failed")
                : std::string("WebRTC peer send failed: ") + sendError
        );
        return false;
    }

    void flushQueuedPeerPackets()
    {
        for(WebRtcPeerState& peer : m_peers) {
            if(!peer.connection || peer.closeRequested || peer.pendingOutboundPackets.empty()) {
                continue;
            }
            if(!peer.connection->isDataChannelOpen()) {
                continue;
            }

            size_t sendsThisPoll = 0;
            while(!peer.pendingOutboundPackets.empty() &&
                  sendsThisPoll < kWebRtcMaxQueuedSendsPerPeerPerPoll) {
                if(peer.connection->bufferedAmount() > kWebRtcBufferedAmountResumeBytes) {
                    break;
                }

                const WebRtcPeerState::QueuedOutboundPacket& packet = peer.pendingOutboundPackets.front();
                const bool sent = packet.reliable
                    ? peer.connection->sendDataReliable(packet.payload)
                    : peer.connection->sendDataUnreliable(packet.payload);
                if(!sent) {
                    const std::string sendError = peer.connection->lastError();
                    if(sendError.find("not open") != std::string::npos) {
                        break;
                    }
                    handlePeerFailure(
                        peer,
                        sendError.empty()
                            ? std::string("WebRTC queued peer send failed")
                            : std::string("WebRTC queued peer send failed: ") + sendError
                    );
                    break;
                }

                peer.pendingOutboundPackets.pop_front();
                ++sendsThisPoll;
            }
        }
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
        peer.disconnectEmitted = false;
        peer.handshakeState = PeerHandshakeState::Connected;
        peer.handshakeDeadline.reset();
    }

    bool hasConnectedPeer() const
    {
        for(const WebRtcPeerState& peer : m_peers) {
            if(peer.connected) {
                return true;
            }
        }
        return false;
    }

    void updateHandshakeTimeout()
    {
        flushQueuedPeerPackets();
        for(WebRtcPeerState& peer : m_peers) {
            if(peer.connected) {
                logPeerConnectionPathIfChanged(peer);
            }
        }

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

            WebRtcPeerState* failedPeer = findPeerByHandle(peer.handle);
            if(failedPeer == nullptr) {
                continue;
            }

            handlePeerFailure(
                *failedPeer,
                "WebRTC peer handshake timed out (" +
                    std::string(handshakeStageLabel(failedPeer->handshakeState)) +
                    " for " + failedPeer->remotePeerId + ")"
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
                const uint16_t signalingPort = host ? m_options.embeddedWebRtcSignalingPort : port;
                if(signalingHost.empty() || signalingPort == 0) {
                    return std::nullopt;
                }
                config.url = buildWebRtcSignalingUrl(signalingHost, signalingPort);
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
        m_lifecycle = TransportLifecycle::Bootstrapping;

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

        if(!m_hosting) {
            if(!m_hostRemotePeerId.empty() && remotePeerId != m_hostRemotePeerId) {
                return false;
            }
            if(m_hostRemotePeerId.empty() && remotePeerId.rfind("host-", 0) != 0) {
                return false;
            }
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
        if(!m_hosting && m_hostRemotePeerId.empty()) {
            m_hostRemotePeerId = remotePeerId;
            logTrace("client host peer selected: " + m_hostRemotePeerId);
        }
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
            disconnected.data = static_cast<uint32_t>(it->tag);
            events->push_back(std::move(disconnected));
            it->disconnectEmitted = true;
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

    void requestPeerClose(PeerHandle handle)
    {
        m_session.requestPeerClose(handle);
    }

    void requestCloseAllPeers()
    {
        m_session.requestCloseAllPeers();
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
        m_session.schedulePeerClose(
            peer.handle,
            std::chrono::steady_clock::now() + kWebRtcGracefulDisconnectDelay
        );
    }

    void handlePeerFailure(WebRtcPeerState& peer,
                           const std::string& error,
                           std::vector<Event>* events = nullptr)
    {
        const std::string resolvedError =
            error.empty() ? std::string("WebRTC peer negotiation failed") : error;
        logTrace("peer failure for " + peer.remotePeerId + ": " + resolvedError);

        if(m_hosting) {
            (void)events;
            requestPeerClose(peer.handle);
            return;
        }

        requestFatalShutdown(resolvedError);
    }

    void processPendingPeerClose(std::vector<Event>& events)
    {
        bool closeAllRequested = false;
        std::vector<PeerHandle> handles =
            m_session.takePendingPeerCloses(std::chrono::steady_clock::now(), closeAllRequested);
        if(closeAllRequested) {
            closeAllPeers(&events);
            return;
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
            std::string sendLog = std::string("sent signaling message: ") + signalTypeLabel(message.type) +
                                  " peerId=" + m_localPeerId +
                                  " targetPeerId=" + peer.remotePeerId;
            if(!message.roomId.empty()) {
                sendLog += " roomId=" + message.roomId;
            }
            logTrace(sendLog);
            return true;
        }

        logTrace(std::string(context) + " failed for " + peer.remotePeerId + ": " + m_lastError);
        requestFatalShutdown(m_lastError.empty() ? std::string("WebRTC signaling send failed") : m_lastError);
        return false;
    }

    void processSignalingEvents(std::vector<Event>& events)
    {
        if(m_lifecycle == TransportLifecycle::Closing) return;

        m_session.drainQueuedEvents([&](SessionEvent event) {
            if(event.type == SessionEvent::Type::SignalingDisconnected) {
                logTrace("signaling disconnected");
                const bool keepSessionAlive = !m_bootstrapPending && hasConnectedPeer();
                if(!keepSessionAlive) {
                    requestCloseAllPeers();
                }
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
                if(!keepSessionAlive) {
                    m_active = false;
                } else {
                    m_lifecycle = TransportLifecycle::DegradedSignaling;
                    logTrace("keeping active WebRTC session alive despite signaling disconnect");
                }
                return;
            }

            if(event.type == SessionEvent::Type::SignalingError) {
                const std::string errorText = !event.text.empty() ? event.text : m_signalingClient->lastError();
                logTrace("signaling event error: " + errorText);
                if(!m_signalingClient->isConnected()) {
                    const bool keepSessionAlive = !m_bootstrapPending && hasConnectedPeer();
                    if(keepSessionAlive) {
                        logTrace("ignoring signaling error after peer connection is established");
                        m_lifecycle = TransportLifecycle::DegradedSignaling;
                        m_signalingReady = false;
                    } else {
                        requestFatalShutdown(errorText);
                        if(m_bootstrapPending) {
                            m_bootstrapPending = false;
                            m_bootstrapDeadline.reset();
                        }
                    }
                }
                return;
            }

            if(event.type == SessionEvent::Type::SignalingConnected) {
                if(m_bootstrapPending && !m_bootstrapHelloSent) {
                    WebRtcSignalingMessage hello;
                    hello.type = WebRtcSignalType::Hello;
                    hello.roomId = m_activeSignalingConfig ? m_activeSignalingConfig->roomId : std::string{};
                    hello.peerId = m_localPeerId;
                    if(!sendSignalingMessage(hello)) {
                        m_bootstrapPending = false;
                        return;
                    }
                    m_bootstrapHelloSent = true;
                    logTrace("sent signaling message: Hello roomId=" + hello.roomId);
                }
                return;
            }

            if(event.type == SessionEvent::Type::PeerLocalDescriptionReady) {
                WebRtcPeerState* peer = findPeerByHandle(event.peer);
                if(peer == nullptr) {
                    return;
                }
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
                return;
            }

            if(event.type == SessionEvent::Type::PeerIceCandidateReady) {
                WebRtcPeerState* peer = findPeerByHandle(event.peer);
                if(peer == nullptr) {
                    return;
                }
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
                return;
            }

            if(event.type == SessionEvent::Type::PeerDataChannelOpen) {
                WebRtcPeerState* peer = findPeerByHandle(event.peer);
                if(peer == nullptr) {
                    return;
                }
                if(!peer->connected) {
                    logTrace("data channel opened for " + peer->remotePeerId);
                    peer->connected = true;
                    markPeerConnected(*peer);
                    logPeerConnectionPathIfChanged(*peer);
                    stopInitialOfferWait();
                    Event connected;
                    connected.type = Event::Type::Connected;
                    connected.peer = peer->handle;
                    events.push_back(std::move(connected));
                }
                return;
            }

            if(event.type == SessionEvent::Type::PeerDataChannelClosed) {
                WebRtcPeerState* peer = findPeerByHandle(event.peer);
                if(peer == nullptr) {
                    return;
                }
                logTrace("data channel closed for " + peer->remotePeerId);
                if(peer->connected && !peer->disconnectEmitted) {
                    peer->connected = false;
                    peer->disconnectEmitted = true;
                    Event disconnected;
                    disconnected.type = Event::Type::Disconnected;
                    disconnected.peer = peer->handle;
                    disconnected.data = static_cast<uint32_t>(peer->tag);
                    events.push_back(std::move(disconnected));
                }
                schedulePeerClose(*peer);
                return;
            }

            if(event.type == SessionEvent::Type::PeerDataMessage) {
                if(event.payload.empty()) {
                    return;
                }
                Event packet;
                packet.type = Event::Type::PacketReceived;
                packet.peer = event.peer;
                packet.channel = static_cast<Channel>(event.payload.front());
                packet.payload.assign(event.payload.begin() + 1, event.payload.end());
                events.push_back(std::move(packet));
                return;
            }

            if(event.type == SessionEvent::Type::PeerError) {
                WebRtcPeerState* peer = findPeerByHandle(event.peer);
                if(peer == nullptr || !peer->connection) {
                    return;
                }
                const std::string error = event.text.empty() ? peer->connection->lastError() : event.text;
                logTrace("peer error for " + peer->remotePeerId + ": " + error);
                if(!shouldIgnoreExpectedPeerError(*peer, error)) {
                    handlePeerFailure(*peer, error, &events);
                    return;
                }
                return;
            }

            if(event.type != SessionEvent::Type::SignalingMessage) {
                return;
            }

            const WebRtcSignalingMessage& message = event.signalingMessage;
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
                        m_signaledIceServers = sanitizeAdvertisedIceServers(message.iceServers);
                    }
                    if(!sendBootstrapRoomRequest()) {
                        m_bootstrapPending = false;
                        m_bootstrapDeadline.reset();
                        return;
                    }
                } else if(message.type == WebRtcSignalType::RoomJoined &&
                          m_activeSignalingConfig.has_value() &&
                          message.roomId == m_activeSignalingConfig->roomId) {
                    m_bootstrapSawRoomJoined = true;
                    if(!message.iceServers.empty()) {
                        m_signaledIceServers = sanitizeAdvertisedIceServers(message.iceServers);
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
                    return;
                } else {
                    logTrace(std::string("queued signaling message during bootstrap: ") + signalTypeLabel(message.type));
                    m_session.deferSignalingMessage(message);
                }

                if(m_bootstrapSawWelcome && m_bootstrapSawRoomJoined) {
                    m_signalingReady = true;
                    m_lifecycle = TransportLifecycle::Active;
                    m_bootstrapPending = false;
                    m_bootstrapDeadline.reset();
                    if(!m_bootstrapHost) {
                        startInitialOfferWait();
                    }
                    logTrace("bootstrap complete");
                }
                return;
            }
            switch(message.type) {
                case WebRtcSignalType::PeerJoined:
                    if(message.peerId != m_localPeerId) {
                        if(!m_hosting &&
                           !m_hostRemotePeerId.empty() &&
                           message.peerId != m_hostRemotePeerId) {
                            logTrace("ignoring non-host PeerJoined from " + message.peerId);
                            break;
                        }
                        if(!m_hosting &&
                           m_hostRemotePeerId.empty() &&
                           message.peerId.rfind("host-", 0) != 0) {
                            logTrace("ignoring non-host PeerJoined before host selection: " + message.peerId);
                            break;
                        }
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
                                    handlePeerFailure(*peer, peer->connection->lastError(), &events);
                                    return;
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
                        requestPeerClose(peer->handle);
                    }
                    break;

                case WebRtcSignalType::Offer:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    if(!m_hosting) {
                        if(!m_hostRemotePeerId.empty() && message.peerId != m_hostRemotePeerId) {
                            logTrace("ignoring offer from non-host peer " + message.peerId);
                            break;
                        }
                        if(m_hostRemotePeerId.empty() && message.peerId.rfind("host-", 0) != 0) {
                            logTrace("ignoring offer from non-host peer before host selection " + message.peerId);
                            break;
                        }
                    }
                    logTrace("processing offer from " + message.peerId);
                    if(!initializePeerConnection(message.peerId, false)) {
                        break;
                    }
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        stopInitialOfferWait();
                        startPeerHandshake(*peer, PeerHandshakeState::Negotiating);
                        if(!peer->connection->setRemoteDescription(message.sdp, true)) {
                            handlePeerFailure(*peer, peer->connection->lastError(), &events);
                            return;
                        }
                        if(!peer->connection->createAnswer()) {
                            handlePeerFailure(*peer, peer->connection->lastError(), &events);
                            return;
                        }
                    }
                    break;

                case WebRtcSignalType::Answer:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    if(!m_hosting) {
                        if(!m_hostRemotePeerId.empty() && message.peerId != m_hostRemotePeerId) {
                            logTrace("ignoring answer from non-host peer " + message.peerId);
                            break;
                        }
                        if(m_hostRemotePeerId.empty() && message.peerId.rfind("host-", 0) != 0) {
                            logTrace("ignoring answer from non-host peer before host selection " + message.peerId);
                            break;
                        }
                    }
                    logTrace("processing answer from " + message.peerId);
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        noteHandshakeProgress(*peer, PeerHandshakeState::Negotiating);
                        if(!peer->connection->setRemoteDescription(message.sdp, false)) {
                            handlePeerFailure(*peer, peer->connection->lastError(), &events);
                            return;
                        }
                    }
                    break;

                case WebRtcSignalType::IceCandidate:
                    if(message.peerId == m_localPeerId) {
                        break;
                    }
                    if(!m_hosting) {
                        if(!m_hostRemotePeerId.empty() && message.peerId != m_hostRemotePeerId) {
                            break;
                        }
                        if(m_hostRemotePeerId.empty() && message.peerId.rfind("host-", 0) != 0) {
                            break;
                        }
                    }
                    logTrace("processing ICE candidate from " + message.peerId);
                    if(WebRtcPeerState* peer = findPeerByRemoteId(message.peerId)) {
                        noteHandshakeProgress(*peer, PeerHandshakeState::Negotiating);
                        if(!peer->connection->addRemoteIceCandidate(message.candidate, message.mid, message.mlineIndex)) {
                            handlePeerFailure(*peer, peer->connection->lastError(), &events);
                            return;
                        }
                    }
                    break;

                default:
                    break;
            }
        });
    }

public:
    bool initialize() override { return true; }
    void performShutdown(ShutdownMode mode)
    {
        if(m_lifecycle == TransportLifecycle::Closing &&
           !m_signalingClient &&
           !m_signalingServer &&
           m_peers.empty()) {
            return;
        }

        m_lifecycle = TransportLifecycle::Closing;

        if(mode == ShutdownMode::Graceful) {
            if(!m_lastError.empty()) {
                logTrace("shutdown due to error: " + m_lastError);
            } else {
                logTrace("shutdown");
            }
            logTrace("detaching runtime resources");
            DetachedRuntimeResources resources = detachRuntimeResources();
            clearRuntimeState();
            if(resources.signalingClient) {
                logTrace("disconnecting signaling client");
                resources.signalingClient->disconnect();
            }
            for(auto& connection : resources.peerConnections) {
                if(connection) {
                    connection->close();
                }
            }
            flushWebRtcPeerConnectionCleanup();
            if(resources.signalingServer) {
                resources.signalingServer->stop();
            }
            return;
        }

        logTrace("shutdown for unload");

        DetachedRuntimeResources resources = detachRuntimeResources();
        clearRuntimeState();

        for(auto& connection : resources.peerConnections) {
            (void)connection.release();
        }
        (void)resources.signalingClient.release();
        (void)resources.signalingServer.release();
    }

public:
    void shutdown() override
    {
        performShutdown(ShutdownMode::Graceful);
    }

    void shutdownForUnload() override
    {
        performShutdown(ShutdownMode::Unload);
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
            if(!ensureSignalingServer()) {
                m_lastError = "Embedded WebRTC signaling server could not be created";
                return false;
            }
            if(!m_signalingServer->start(m_options.embeddedWebRtcSignalingPort)) {
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
                requestPeerClose(peer);
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
            m_session.collectSignalingEvents();
            processSignalingEvents(events);
            m_session.collectPeerEvents(m_lifecycle);
            processSignalingEvents(events);
            processPendingPeerClose(events);
            if(!events.empty() || timeoutMs == 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while(std::chrono::steady_clock::now() < deadline);

        if(m_lifecycle != TransportLifecycle::Closing) {
            updateHandshakeTimeout();
        }
        if(const std::optional<std::string> fatalShutdownError = m_session.takeFatalShutdownError();
           fatalShutdownError.has_value()) {
            m_lastError = *fatalShutdownError;
            shutdown();
        }

        return events;
    }
    bool sendReliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) override
    {
        WebRtcPeerState* state = findPeerByHandle(peer);
        if(state == nullptr || !state->connection) return false;
        return enqueueOrSendPeerPayload(*state, channel, wrapPayload(channel, payload), true);
    }
    bool sendUnreliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) override
    {
        WebRtcPeerState* state = findPeerByHandle(peer);
        if(state == nullptr || !state->connection) return false;
        return enqueueOrSendPeerPayload(*state, channel, wrapPayload(channel, payload), false);
    }
    bool broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) override
    {
        bool sent = false;
        for(WebRtcPeerState& peer : m_peers) {
            if(peer.handle == exceptPeer || !peer.connection || !peer.connected) continue;
            sent = enqueueOrSendPeerPayload(peer, channel, wrapPayload(channel, payload), true) || sent;
        }
        return sent;
    }
    bool broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) override
    {
        bool sent = false;
        for(WebRtcPeerState& peer : m_peers) {
            if(peer.handle == exceptPeer || !peer.connection || !peer.connected) continue;
            sent = enqueueOrSendPeerPayload(peer, channel, wrapPayload(channel, payload), false) || sent;
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
            break;
#else
        case NetTransportBackend::ENet:
            return false;
#endif

        case NetTransportBackend::WebRTC:
            m_impl = std::make_unique<WebRTCTransport>();
            break;

        default:
            return false;
    }

    if(m_impl) {
        m_impl->setOptions(m_options);
    }

    return static_cast<bool>(m_impl);
}

void NetTransport::discardImpl(bool invokeShutdown)
{
    if(!m_impl) {
        return;
    }

    m_lastErrorCache = m_impl->lastError();
    m_advertisedIceServersCache = m_impl->advertisedIceServers();

    if(invokeShutdown) {
        m_impl->shutdown();
        m_lastErrorCache = m_impl->lastError();
        m_advertisedIceServersCache = m_impl->advertisedIceServers();
    }

    m_impl.reset();
}

bool NetTransport::setBackend(NetTransportBackend backend)
{
    if(m_impl && m_impl->isActive()) return false;
    if(m_impl) {
        discardImpl(true);
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
    m_options = options;
    if(ensureImpl()) {
        m_impl->setOptions(options);
    }
}

const NetTransportOptions& NetTransport::options() const
{
    return m_impl ? m_impl->options() : m_options;
}

const std::string& NetTransport::lastError() const
{
    return m_impl ? m_impl->lastError() : m_lastErrorCache;
}

const std::vector<std::string>& NetTransport::advertisedIceServers() const
{
    return m_impl ? m_impl->advertisedIceServers() : m_advertisedIceServersCache;
}

bool NetTransport::initialize()
{
    return ensureImpl() && m_impl->initialize();
}

void NetTransport::shutdown()
{
    if(m_impl) {
        discardImpl(true);
    }
}

void NetTransport::shutdownForUnload()
{
    if(m_impl) {
        m_lastErrorCache = m_impl->lastError();
        m_advertisedIceServersCache = m_impl->advertisedIceServers();
        m_impl->shutdownForUnload();
        m_impl.reset();
    }
}

bool NetTransport::hostSession(uint16_t port, size_t maxPeers)
{
    m_lastErrorCache.clear();
    m_advertisedIceServersCache.clear();
    if(!ensureImpl()) {
        return false;
    }

    const bool hosted = m_impl->hostSession(port, maxPeers);
    m_lastErrorCache = m_impl->lastError();
    m_advertisedIceServersCache = m_impl->advertisedIceServers();
    if(!hosted) {
        discardImpl(true);
    }
    return hosted;
}

bool NetTransport::connectToHost(const std::string& hostName, uint16_t port, size_t channelCount)
{
    m_lastErrorCache.clear();
    m_advertisedIceServersCache.clear();
    if(!ensureImpl()) {
        return false;
    }

    const bool connected = m_impl->connectToHost(hostName, port, channelCount);
    m_lastErrorCache = m_impl->lastError();
    m_advertisedIceServersCache = m_impl->advertisedIceServers();
    if(!connected) {
        discardImpl(true);
    }
    return connected;
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
    if(!m_impl) {
        return {};
    }

    std::vector<Event> events = m_impl->poll(timeoutMs);
    m_lastErrorCache = m_impl->lastError();
    m_advertisedIceServersCache = m_impl->advertisedIceServers();
    if(!m_impl->isActive()) {
        m_impl.reset();
    }
    return events;
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

} // namespace ConsoleNetplay
