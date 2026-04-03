#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "NetProtocol.h"
#include "WebRtcSignaling.h"

namespace Netplay {

enum class NetTransportBackend : uint8_t
{
    ENet = 0,
    WebRTC = 1
};

const char* netTransportBackendLabel(NetTransportBackend backend);
std::vector<NetTransportBackend> availableNetTransportBackends();
NetTransportBackend defaultNetTransportBackend();

struct NetTransportOptions
{
#if defined(__EMSCRIPTEN__)
    bool useEmbeddedWebRtcSignalingServer = false;
#else
    bool useEmbeddedWebRtcSignalingServer = true;
#endif
    std::optional<WebRtcSignalingConfig> webRtcSignaling;
};

class INetTransport
{
public:
    using PeerHandle = uint64_t;
    static constexpr PeerHandle kInvalidPeerHandle = 0;

    struct Event
    {
        enum class Type : uint8_t
        {
            None,
            Connected,
            Disconnected,
            PacketReceived
        };

        Type type = Type::None;
        PeerHandle peer = kInvalidPeerHandle;
        Channel channel = Channel::Control;
        std::vector<uint8_t> payload;
    };

    virtual ~INetTransport() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void setOptions(const NetTransportOptions& options) = 0;
    virtual const NetTransportOptions& options() const = 0;
    virtual const std::string& lastError() const = 0;

    virtual bool hostSession(uint16_t port, size_t maxPeers) = 0;
    virtual bool connectToHost(const std::string& hostName, uint16_t port, size_t channelCount = 3) = 0;
    virtual void disconnectAll(uint32_t data = 0) = 0;
    virtual void disconnectPeer(PeerHandle peer, uint32_t data = 0) = 0;
    virtual void flush() = 0;
    virtual std::vector<PeerHandle> connectedPeers() const = 0;

    virtual std::vector<Event> poll(uint32_t timeoutMs) = 0;

    virtual bool sendReliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) = 0;
    virtual bool sendUnreliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload) = 0;
    virtual bool broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) = 0;
    virtual bool broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle) = 0;

    virtual uintptr_t peerTag(PeerHandle peer) const = 0;
    virtual void setPeerTag(PeerHandle peer, uintptr_t tag) = 0;
    virtual uint32_t peerRoundTripTime(PeerHandle peer) const = 0;
    virtual uint32_t peerRoundTripVariance(PeerHandle peer) const = 0;

    virtual bool isActive() const = 0;
};

class NetTransport
{
public:
    using PeerHandle = INetTransport::PeerHandle;
    static constexpr PeerHandle kInvalidPeerHandle = INetTransport::kInvalidPeerHandle;
    using Event = INetTransport::Event;

private:
    NetTransportBackend m_backend = NetTransportBackend::ENet;
    std::unique_ptr<INetTransport> m_impl;

    bool ensureImpl();

public:
    explicit NetTransport(NetTransportBackend backend = NetTransportBackend::ENet);
    ~NetTransport();

    bool setBackend(NetTransportBackend backend);
    NetTransportBackend backend() const;
    void setOptions(const NetTransportOptions& options);
    const NetTransportOptions& options() const;
    const std::string& lastError() const;

    bool initialize();
    void shutdown();

    bool hostSession(uint16_t port, size_t maxPeers);
    bool connectToHost(const std::string& hostName, uint16_t port, size_t channelCount = 3);
    void disconnectAll(uint32_t data = 0);
    void disconnectPeer(PeerHandle peer, uint32_t data = 0);
    void flush();
    std::vector<PeerHandle> connectedPeers() const;

    std::vector<Event> poll(uint32_t timeoutMs);

    bool sendReliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload);
    bool sendUnreliable(PeerHandle peer, Channel channel, const std::vector<uint8_t>& payload);
    bool broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle);
    bool broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, PeerHandle exceptPeer = kInvalidPeerHandle);

    uintptr_t peerTag(PeerHandle peer) const;
    void setPeerTag(PeerHandle peer, uintptr_t tag);
    uint32_t peerRoundTripTime(PeerHandle peer) const;
    uint32_t peerRoundTripVariance(PeerHandle peer) const;

    bool isActive() const;
};

} // namespace Netplay
