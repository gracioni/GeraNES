#include "GeraNESNetplay/WebRtcSignalingServer.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GeraNESNetplay/WebRtcSignaling.h"

#if !defined(__EMSCRIPTEN__)
#include <rtc/websocket.hpp>
#include <rtc/websocketserver.hpp>
#endif

namespace Netplay {

namespace {

#if !defined(__EMSCRIPTEN__)
std::string describeWebRtcStartError(const std::exception& ex)
{
    const std::string message = ex.what();
    if(message.find("binding failed") != std::string::npos) {
        return "Embedded WebRTC signaling server failed to start: address already in use";
    }
    if(message.find("Resolution failed") != std::string::npos) {
        return "Embedded WebRTC signaling server failed to start: address not available";
    }
    if(message.find("No suitable address family") != std::string::npos) {
        return "Embedded WebRTC signaling server failed to start: address family not supported";
    }
    return "Embedded WebRTC signaling server failed to start";
}

class DesktopWebRtcSignalingServer final : public IWebRtcSignalingServer
{
private:
    using Connection = std::shared_ptr<rtc::WebSocket>;

    struct CallbackContext
    {
        std::atomic<bool> alive{true};
        DesktopWebRtcSignalingServer* owner = nullptr;
    };

    struct ClientState
    {
        std::string peerId;
        std::string roomId;
    };

    struct RoomState
    {
        std::string password;
        std::size_t maxParticipants = 2;
        Connection owner;
        std::string ownerPeerId;
        std::set<Connection, std::owner_less<Connection>> members;
    };

    struct DetachResult
    {
        std::string roomId;
        std::optional<WebRtcSignalingMessage> peerLeft;
        bool ownerDisconnected = false;
        std::string ownerPeerId;
        std::vector<Connection> orphanRecipients;
    };

    std::unique_ptr<rtc::WebSocketServer> m_server;
    mutable std::mutex m_mutex;
    std::map<Connection, ClientState, std::owner_less<Connection>> m_clients;
    std::unordered_map<std::string, RoomState> m_rooms;
    bool m_running = false;
    uint16_t m_port = 0;
    std::string m_lastError;
    std::shared_ptr<CallbackContext> m_callbackContext;

    static DesktopWebRtcSignalingServer* ownerFromContext(const std::shared_ptr<CallbackContext>& context)
    {
        if(!context || !context->alive.load(std::memory_order_acquire)) {
            return nullptr;
        }
        return context->owner;
    }

    static std::string trimNonEmpty(const std::string& text)
    {
        const std::size_t begin = text.find_first_not_of(" \t\r\n");
        if(begin == std::string::npos) {
            return {};
        }
        const std::size_t end = text.find_last_not_of(" \t\r\n");
        return text.substr(begin, end - begin + 1);
    }

    static std::size_t sanitizeMaxParticipants(int requested)
    {
        return requested > 2 ? static_cast<std::size_t>(requested) : static_cast<std::size_t>(2);
    }

    void sendMessage(const Connection& connection, const WebRtcSignalingMessage& message)
    {
        if(!connection) return;
        connection->send(message.toText());
    }

    void sendError(const Connection& connection, const std::string& error)
    {
        WebRtcSignalingMessage message;
        message.type = WebRtcSignalType::Error;
        message.error = error;
        sendMessage(connection, message);
    }

    std::optional<Connection> findPeerInRoomLocked(const std::string& roomId, const std::string& peerId) const
    {
        const auto roomIt = m_rooms.find(roomId);
        if(roomIt == m_rooms.end()) {
            return std::nullopt;
        }

        for(const auto& member : roomIt->second.members) {
            const auto clientIt = m_clients.find(member);
            if(clientIt != m_clients.end() && clientIt->second.peerId == peerId) {
                return member;
            }
        }
        return std::nullopt;
    }

    void sendMessages(const std::vector<std::pair<Connection, WebRtcSignalingMessage>>& pending)
    {
        for(const auto& entry : pending) {
            sendMessage(entry.first, entry.second);
        }
    }

    void closeSpecificPeers(const std::vector<Connection>& recipients)
    {
        for(const auto& recipient : recipients) {
            if(!recipient) {
                continue;
            }
            try {
                recipient->resetCallbacks();
                recipient->forceClose();
            } catch(...) {
            }
        }
    }

    void detachClientFromCurrentRoomLocked(const Connection& connection, ClientState& client)
    {
        if(client.roomId.empty()) {
            return;
        }

        auto roomIt = m_rooms.find(client.roomId);
        if(roomIt == m_rooms.end()) {
            client.roomId.clear();
            return;
        }

        roomIt->second.members.erase(connection);
        if(roomIt->second.owner == connection || roomIt->second.members.empty()) {
            m_rooms.erase(roomIt);
        }
        client.roomId.clear();
    }

    DetachResult detachClientLocked(const Connection& connection, bool removeClient)
    {
        DetachResult result;

        const auto clientIt = m_clients.find(connection);
        if(clientIt == m_clients.end()) {
            return result;
        }

        ClientState& client = clientIt->second;
        result.roomId = client.roomId;

        if(!client.roomId.empty()) {
            auto roomIt = m_rooms.find(client.roomId);
            if(roomIt != m_rooms.end()) {
                RoomState& room = roomIt->second;
                room.members.erase(connection);
                if(room.owner == connection) {
                    result.ownerDisconnected = true;
                    result.ownerPeerId = !room.ownerPeerId.empty() ? room.ownerPeerId : client.peerId;
                    for(const auto& member : room.members) {
                        result.orphanRecipients.push_back(member);
                    }
                    m_rooms.erase(roomIt);
                } else if(room.members.empty()) {
                    m_rooms.erase(roomIt);
                }
            }

            if(!client.peerId.empty()) {
                WebRtcSignalingMessage peerLeft;
                peerLeft.type = WebRtcSignalType::PeerLeft;
                peerLeft.roomId = client.roomId;
                peerLeft.peerId = client.peerId;
                result.peerLeft = std::move(peerLeft);
            }

            client.roomId.clear();
        }

        if(removeClient) {
            m_clients.erase(clientIt);
        }

        return result;
    }

    void handleDetachedClient(const DetachResult& detach, const Connection& departed)
    {
        if(detach.ownerDisconnected) {
            closeSpecificPeers(detach.orphanRecipients);
            return;
        }

        if(detach.peerLeft.has_value()) {
            std::vector<std::pair<Connection, WebRtcSignalingMessage>> pending;
            {
                std::scoped_lock lock(m_mutex);
                const auto roomIt = m_rooms.find(detach.roomId);
                if(roomIt != m_rooms.end()) {
                    for(const auto& member : roomIt->second.members) {
                        if(member == departed) {
                            continue;
                        }
                        pending.emplace_back(member, *detach.peerLeft);
                    }
                }
            }
            sendMessages(pending);
        }
    }

    void handleDisconnect(const Connection& connection)
    {
        DetachResult detach;
        {
            std::scoped_lock lock(m_mutex);
            detach = detachClientLocked(connection, true);
        }
        handleDetachedClient(detach, connection);
    }

    void handleHello(const Connection& connection,
                     const WebRtcSignalingMessage& message,
                     std::vector<std::pair<Connection, WebRtcSignalingMessage>>& pending)
    {
        const std::string peerId = trimNonEmpty(message.peerId);
        if(peerId.empty()) {
            sendError(connection, "Missing peer id");
            return;
        }

        {
            std::scoped_lock lock(m_mutex);
            ClientState& client = m_clients[connection];
            client.peerId = peerId;
        }

        WebRtcSignalingMessage response;
        response.type = WebRtcSignalType::Welcome;
        response.roomId = message.roomId;
        response.peerId = peerId;
        pending.emplace_back(connection, std::move(response));
    }

    void handleRoomList(const Connection& connection,
                        std::vector<std::pair<Connection, WebRtcSignalingMessage>>& pending)
    {
        WebRtcSignalingMessage response;
        response.type = WebRtcSignalType::RoomList;

        {
            std::scoped_lock lock(m_mutex);
            for(const auto& [roomId, room] : m_rooms) {
                if(room.members.empty()) {
                    continue;
                }
                WebRtcSignalingRoomInfo info;
                info.roomId = roomId;
                info.passwordProtected = !room.password.empty();
                response.rooms.push_back(std::move(info));
            }
        }

        pending.emplace_back(connection, std::move(response));
    }

    void handleCreateRoom(const Connection& connection,
                          const WebRtcSignalingMessage& message,
                          std::vector<std::pair<Connection, WebRtcSignalingMessage>>& pending)
    {
        const std::string roomId = trimNonEmpty(message.roomId);
        const std::string peerId = trimNonEmpty(message.peerId);
        if(roomId.empty() || peerId.empty()) {
            sendError(connection, "Missing room id or peer id");
            return;
        }

        {
            std::scoped_lock lock(m_mutex);
            const auto existingRoomIt = m_rooms.find(roomId);
            if(existingRoomIt != m_rooms.end() && !existingRoomIt->second.members.empty()) {
                sendError(connection, "Room already exists");
                return;
            }

            ClientState& client = m_clients[connection];
            if(!client.roomId.empty() && client.roomId != roomId) {
                detachClientFromCurrentRoomLocked(connection, client);
            }
            client.peerId = peerId;
            client.roomId = roomId;

            RoomState& room = m_rooms[roomId];
            room.password = message.password;
            room.maxParticipants = sanitizeMaxParticipants(message.maxParticipants);
            room.owner = connection;
            room.ownerPeerId = peerId;
            room.members.clear();
            room.members.insert(connection);
        }

        WebRtcSignalingMessage response;
        response.type = WebRtcSignalType::RoomJoined;
        response.roomId = roomId;
        response.peerId = peerId;
        pending.emplace_back(connection, std::move(response));
    }

    void handleJoinRoom(const Connection& connection,
                        const WebRtcSignalingMessage& message,
                        std::vector<std::pair<Connection, WebRtcSignalingMessage>>& pending)
    {
        const std::string roomId = trimNonEmpty(message.roomId);
        const std::string peerId = trimNonEmpty(message.peerId);
        if(roomId.empty() || peerId.empty()) {
            sendError(connection, "Missing room id or peer id");
            return;
        }

        std::vector<std::string> existingPeerIds;
        std::vector<Connection> recipients;

        {
            std::scoped_lock lock(m_mutex);
            auto roomIt = m_rooms.find(roomId);
            if(roomIt == m_rooms.end() || roomIt->second.members.empty()) {
                sendError(connection, "Room does not exist");
                return;
            }

            RoomState& room = roomIt->second;
            if(!room.owner || room.members.find(room.owner) == room.members.end()) {
                m_rooms.erase(roomIt);
                sendError(connection, "Room does not exist");
                return;
            }
            if(room.password != message.password) {
                sendError(connection, "Invalid room password");
                return;
            }
            if(room.members.size() >= room.maxParticipants) {
                sendError(connection, "Room is full");
                return;
            }

            ClientState& client = m_clients[connection];
            if(!client.roomId.empty() && client.roomId != roomId) {
                detachClientFromCurrentRoomLocked(connection, client);
            }
            client.peerId = peerId;
            client.roomId = roomId;

            for(const auto& member : room.members) {
                if(member == connection) {
                    continue;
                }
                recipients.push_back(member);
                const auto clientIt = m_clients.find(member);
                if(clientIt != m_clients.end() && !clientIt->second.peerId.empty()) {
                    existingPeerIds.push_back(clientIt->second.peerId);
                }
            }

            room.members.insert(connection);
        }

        WebRtcSignalingMessage joined;
        joined.type = WebRtcSignalType::RoomJoined;
        joined.roomId = roomId;
        joined.peerId = peerId;
        pending.emplace_back(connection, joined);

        for(const std::string& existingPeerId : existingPeerIds) {
            WebRtcSignalingMessage peerJoined;
            peerJoined.type = WebRtcSignalType::PeerJoined;
            peerJoined.roomId = roomId;
            peerJoined.peerId = existingPeerId;
            pending.emplace_back(connection, std::move(peerJoined));
        }

        for(const auto& recipient : recipients) {
            WebRtcSignalingMessage peerJoined;
            peerJoined.type = WebRtcSignalType::PeerJoined;
            peerJoined.roomId = roomId;
            peerJoined.peerId = peerId;
            pending.emplace_back(recipient, std::move(peerJoined));
        }
    }

    void handleLeaveRoom(const Connection& connection)
    {
        DetachResult detach;
        {
            std::scoped_lock lock(m_mutex);
            detach = detachClientLocked(connection, false);
        }
        handleDetachedClient(detach, connection);
    }

    void handleDirectSignal(const Connection& connection,
                            const WebRtcSignalingMessage& message,
                            std::vector<std::pair<Connection, WebRtcSignalingMessage>>& pending)
    {
        const std::string targetPeerId = trimNonEmpty(message.targetPeerId);
        if(targetPeerId.empty()) {
            sendError(connection, "Missing target peer id");
            return;
        }

        std::optional<Connection> target;
        std::string roomId;
        std::string senderPeerId;

        {
            std::scoped_lock lock(m_mutex);
            const auto clientIt = m_clients.find(connection);
            if(clientIt == m_clients.end() || clientIt->second.roomId.empty()) {
                sendError(connection, "Join a room before sending signaling data");
                return;
            }

            roomId = clientIt->second.roomId;
            senderPeerId = clientIt->second.peerId;
            target = findPeerInRoomLocked(roomId, targetPeerId);
        }

        if(!target.has_value()) {
            sendError(connection, "Target peer is not connected");
            return;
        }

        WebRtcSignalingMessage forward = message;
        forward.roomId = roomId;
        forward.peerId = senderPeerId;
        forward.targetPeerId = targetPeerId;
        pending.emplace_back(*target, std::move(forward));
    }

    void handleMessage(const Connection& connection, const std::string& payload)
    {
        const auto parsed = WebRtcSignalingMessage::fromText(payload);
        if(!parsed.has_value()) {
            sendError(connection, "Invalid signaling payload");
            return;
        }

        std::vector<std::pair<Connection, WebRtcSignalingMessage>> pending;
        switch(parsed->type) {
            case WebRtcSignalType::Hello:
                handleHello(connection, *parsed, pending);
                break;
            case WebRtcSignalType::RoomList:
                handleRoomList(connection, pending);
                break;
            case WebRtcSignalType::CreateRoom:
                handleCreateRoom(connection, *parsed, pending);
                break;
            case WebRtcSignalType::JoinRoom:
                handleJoinRoom(connection, *parsed, pending);
                break;
            case WebRtcSignalType::LeaveRoom:
                handleLeaveRoom(connection);
                break;
            case WebRtcSignalType::Offer:
            case WebRtcSignalType::Answer:
            case WebRtcSignalType::IceCandidate:
                handleDirectSignal(connection, *parsed, pending);
                break;
            default:
                sendError(connection, "Unsupported signaling message");
                break;
        }

        sendMessages(pending);
    }

    void attachClientCallbacks(const Connection& connection)
    {
        {
            std::scoped_lock lock(m_mutex);
            m_clients.try_emplace(connection);
        }

        const std::weak_ptr<rtc::WebSocket> weakConnection = connection;
        const std::shared_ptr<CallbackContext> callbackContext = m_callbackContext;

        connection->onMessage([weakConnection, callbackContext](rtc::message_variant data) {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            const auto connection = weakConnection.lock();
            if(!connection) {
                return;
            }
            const auto* text = std::get_if<std::string>(&data);
            if(text == nullptr) {
                return;
            }
            try {
                self->handleMessage(connection, *text);
            } catch(...) {
            }
        });

        connection->onClosed([weakConnection, callbackContext]() {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            const auto connection = weakConnection.lock();
            if(!connection) {
                return;
            }
            try {
                self->handleDisconnect(connection);
            } catch(...) {
            }
        });

        connection->onError([weakConnection, callbackContext](std::string) {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            const auto connection = weakConnection.lock();
            if(!connection) {
                return;
            }
            try {
                self->handleDisconnect(connection);
            } catch(...) {
            }
        });
    }

public:
    ~DesktopWebRtcSignalingServer() override
    {
        stop();
    }

    bool start(uint16_t port) override
    {
        stop();

        try {
            rtc::WebSocketServer::Configuration config;
            config.port = port;

            auto server = std::make_unique<rtc::WebSocketServer>(config);
            auto callbackContext = std::make_shared<CallbackContext>();
            callbackContext->owner = this;
            server->onClient([callbackContext](Connection connection) {
                auto* self = ownerFromContext(callbackContext);
                if(self == nullptr) {
                    return;
                }
                self->attachClientCallbacks(std::move(connection));
            });

            {
                std::scoped_lock lock(m_mutex);
                m_lastError.clear();
                m_clients.clear();
                m_rooms.clear();
                m_server = std::move(server);
                m_callbackContext = std::move(callbackContext);
                m_port = m_server->port();
                m_running = true;
            }
            return true;
        } catch(const std::exception& ex) {
            m_lastError = describeWebRtcStartError(ex);
        } catch(...) {
            m_lastError = "Embedded WebRTC signaling server failed to start";
        }

        stop();
        return false;
    }

    void stop() override
    {
        std::unique_ptr<rtc::WebSocketServer> server;
        std::vector<Connection> connections;
        std::shared_ptr<CallbackContext> callbackContext;

        {
            std::scoped_lock lock(m_mutex);
            for(const auto& [connection, state] : m_clients) {
                (void)state;
                connections.push_back(connection);
            }
            server = std::move(m_server);
            callbackContext = std::move(m_callbackContext);
            m_clients.clear();
            m_rooms.clear();
            m_running = false;
            m_port = 0;
        }

        if(callbackContext) {
            callbackContext->alive.store(false, std::memory_order_release);
            callbackContext->owner = nullptr;
        }

        closeSpecificPeers(connections);

        if(server) {
            server->onClient(nullptr);
            server->stop();
        }
    }

    bool isRunning() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_running;
    }

    uint16_t port() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_port;
    }

    std::string lastError() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_lastError;
    }
};
#endif

class StubWebRtcSignalingServer final : public IWebRtcSignalingServer
{
private:
    std::string m_lastError;

public:
    bool start(uint16_t) override
    {
        m_lastError = "Embedded WebRTC signaling server is not implemented on this platform";
        return false;
    }

    void stop() override {}

    bool isRunning() const override
    {
        return false;
    }

    uint16_t port() const override
    {
        return 0;
    }

    std::string lastError() const override
    {
        return m_lastError;
    }
};

} // namespace

std::unique_ptr<IWebRtcSignalingServer> createWebRtcSignalingServer()
{
#if !defined(__EMSCRIPTEN__)
    return std::make_unique<DesktopWebRtcSignalingServer>();
#else
    return std::make_unique<StubWebRtcSignalingServer>();
#endif
}

} // namespace Netplay
