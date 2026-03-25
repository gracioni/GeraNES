#pragma once

#ifndef __EMSCRIPTEN__

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <enet/enet.h>

#include "NetProtocol.h"

namespace Netplay {

class NetTransport
{
public:
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
        ENetPeer* peer = nullptr;
        Channel channel = Channel::Control;
        std::vector<uint8_t> payload;
    };

private:
    ENetHost* m_host = nullptr;
    bool m_initialized = false;

public:
    ~NetTransport();

    bool initialize();
    void shutdown();

    bool hostSession(uint16_t port, size_t maxPeers);
    bool connectToHost(const std::string& hostName, uint16_t port, size_t channelCount = 3);
    void disconnectAll(uint32_t data = 0);
    void disconnectPeer(ENetPeer* peer, uint32_t data = 0);
    std::vector<ENetPeer*> connectedPeers() const;

    std::vector<Event> poll(uint32_t timeoutMs);

    bool sendReliable(ENetPeer* peer, Channel channel, const std::vector<uint8_t>& payload);
    bool sendUnreliable(ENetPeer* peer, Channel channel, const std::vector<uint8_t>& payload);
    bool broadcastReliable(Channel channel, const std::vector<uint8_t>& payload, ENetPeer* exceptPeer = nullptr);
    bool broadcastUnreliable(Channel channel, const std::vector<uint8_t>& payload, ENetPeer* exceptPeer = nullptr);

    bool isActive() const;
};

} // namespace Netplay

#endif
