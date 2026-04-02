#ifndef __EMSCRIPTEN__

#include "GeraNESNetplay/NetTransport.h"

#include <enet/enet.h>

namespace Netplay {

namespace {

class ENetTransport final : public INetTransport
{
private:
    ENetHost* m_host = nullptr;
    bool m_initialized = false;

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
            return false;
        }
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

} // namespace

NetTransport::NetTransport(NetTransportBackend backend)
    : m_backend(backend)
{
}

NetTransport::~NetTransport() = default;

bool NetTransport::ensureImpl()
{
    if(m_impl) return true;

    switch(m_backend) {
        case NetTransportBackend::ENet:
            m_impl = std::make_unique<ENetTransport>();
            return true;

        case NetTransportBackend::WebRTC:
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

#endif
