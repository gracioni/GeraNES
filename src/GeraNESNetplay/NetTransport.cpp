#ifndef __EMSCRIPTEN__

#include "GeraNESNetplay/NetTransport.h"

namespace Netplay {

NetTransport::~NetTransport()
{
    shutdown();
}

bool NetTransport::initialize()
{
    if(m_initialized) return true;
    if(enet_initialize() != 0) {
        return false;
    }
    m_initialized = true;
    return true;
}

void NetTransport::shutdown()
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

bool NetTransport::hostSession(uint16_t port, size_t maxPeers)
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

bool NetTransport::connectToHost(const std::string& hostName, uint16_t port, size_t channelCount)
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

void NetTransport::disconnectAll(uint32_t data)
{
    if(m_host == nullptr) return;

    for(size_t i = 0; i < m_host->peerCount; ++i) {
        ENetPeer& peer = m_host->peers[i];
        if(peer.state != ENET_PEER_STATE_DISCONNECTED) {
            enet_peer_disconnect(&peer, data);
        }
    }
}

void NetTransport::disconnectPeer(ENetPeer* peer, uint32_t data)
{
    if(peer == nullptr) return;
    if(peer->state != ENET_PEER_STATE_DISCONNECTED) {
        enet_peer_disconnect(peer, data);
    }
}

std::vector<ENetPeer*> NetTransport::connectedPeers() const
{
    std::vector<ENetPeer*> peers;
    if(m_host == nullptr) return peers;

    for(size_t i = 0; i < m_host->peerCount; ++i) {
        ENetPeer& peer = m_host->peers[i];
        if(peer.state == ENET_PEER_STATE_CONNECTED) {
            peers.push_back(&peer);
        }
    }

    return peers;
}

std::vector<NetTransport::Event> NetTransport::poll(uint32_t timeoutMs)
{
    std::vector<Event> events;
    if(m_host == nullptr) return events;

    ENetEvent event{};
    while(enet_host_service(m_host, &event, timeoutMs) > 0) {
        Event out;
        out.peer = event.peer;

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

bool NetTransport::sendReliable(ENetPeer* peer, Channel channel, const std::vector<uint8_t>& payload)
{
    if(peer == nullptr || payload.empty()) return false;

    ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
    if(packet == nullptr) return false;

    return enet_peer_send(peer, static_cast<enet_uint8>(channel), packet) == 0;
}

bool NetTransport::sendUnreliable(ENetPeer* peer, Channel channel, const std::vector<uint8_t>& payload)
{
    if(peer == nullptr || payload.empty()) return false;

    ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), 0);
    if(packet == nullptr) return false;

    return enet_peer_send(peer, static_cast<enet_uint8>(channel), packet) == 0;
}

bool NetTransport::broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, ENetPeer* exceptPeer)
{
    if(m_host == nullptr || payload.empty()) return false;

    bool sent = false;
    for(size_t i = 0; i < m_host->peerCount; ++i) {
        ENetPeer& peer = m_host->peers[i];
        if(&peer == exceptPeer || peer.state != ENET_PEER_STATE_CONNECTED) continue;
        if(sendReliable(&peer, channel, payload)) {
            sent = true;
        }
    }

    return sent;
}

bool NetTransport::broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, ENetPeer* exceptPeer)
{
    if(m_host == nullptr || payload.empty()) return false;

    bool sent = false;
    for(size_t i = 0; i < m_host->peerCount; ++i) {
        ENetPeer& peer = m_host->peers[i];
        if(&peer == exceptPeer || peer.state != ENET_PEER_STATE_CONNECTED) continue;
        if(sendUnreliable(&peer, channel, payload)) {
            sent = true;
        }
    }

    return sent;
}

bool NetTransport::isActive() const
{
    return m_host != nullptr;
}

} // namespace Netplay

#endif
