#include "GeraNESNetplay/WebRtcSignalingServer.h"

#include <map>
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
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
        size_t maxParticipants = 2;
        Connection owner;
        std::string ownerPeerId;
        std::set<Connection, std::owner_less<Connection>> members;

        bool passwordProtected() const
        {
            return !password.empty();
        }
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

    std::optional<Connection> findPeerInRoom(const std::string& roomId,
                                             const std::string& peerId) const
    {
        const auto roomIt = m_rooms.find(roomId);
        if(roomIt == m_rooms.end()) return std::nullopt;

        for(const auto& connection : roomIt->second.members) {
            const auto clientIt = m_clients.find(connection);
            if(clientIt != m_clients.end() && clientIt->second.peerId == peerId) {
                return connection;
            }
        }

        return std::nullopt;
    }

    void broadcastToRoom(const std::string& roomId,
                         const WebRtcSignalingMessage& message,
                         const Connection& except = {})
    {
        std::vector<Connection> recipients;
        {
            std::scoped_lock lock(m_mutex);
            const auto roomIt = m_rooms.find(roomId);
            if(roomIt == m_rooms.end()) return;

            for(const auto& connection : roomIt->second.members) {
                if(except && connection == except) {
                    continue;
                }
                recipients.push_back(connection);
            }
        }

        for(const auto& connection : recipients) {
            sendMessage(connection, message);
        }
    }

    void removeClientLocked(const Connection& connection,
                            std::optional<WebRtcSignalingMessage>& peerLeft)
    {
        const auto clientIt = m_clients.find(connection);
        if(clientIt == m_clients.end()) return;

        const std::string roomId = clientIt->second.roomId;
        const std::string peerId = clientIt->second.peerId;
        m_clients.erase(clientIt);

        if(!roomId.empty()) {
            auto roomIt = m_rooms.find(roomId);
            if(roomIt != m_rooms.end()) {
                roomIt->second.members.erase(connection);
                if(roomIt->second.members.empty()) {
                    m_rooms.erase(roomIt);
                }
            }

            if(!peerId.empty()) {
                peerLeft = WebRtcSignalingMessage{};
                peerLeft->type = WebRtcSignalType::PeerLeft;
                peerLeft->roomId = roomId;
                peerLeft->peerId = peerId;
            }
        }
    }

    void handleDisconnect(const Connection& connection)
    {
        std::optional<WebRtcSignalingMessage> peerLeft;
        {
            std::scoped_lock lock(m_mutex);
            removeClientLocked(connection, peerLeft);
        }

        if(peerLeft.has_value()) {
            broadcastToRoom(peerLeft->roomId, *peerLeft, connection);
        }
    }

    void handleMessage(const Connection& connection, const std::string& payload)
    {
        const auto parsed = WebRtcSignalingMessage::fromText(payload);
        if(!parsed.has_value()) {
            sendError(connection, "Invalid signaling payload");
            return;
        }

        std::optional<WebRtcSignalingMessage> directMessage;
        std::optional<WebRtcSignalingMessage> broadcastMessage;
        std::optional<Connection> directTarget;
        std::string broadcastRoomId;

        {
            std::scoped_lock lock(m_mutex);
            ClientState& client = m_clients[connection];
            const WebRtcSignalingMessage& message = *parsed;

            switch(message.type) {
                case WebRtcSignalType::Hello: {
                    if(message.peerId.empty()) {
                        sendError(connection, "Missing peer id");
                        return;
                    }
                    client.peerId = message.peerId;

                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::Welcome;
                    directMessage->roomId = message.roomId;
                    directMessage->peerId = message.peerId;
                    directTarget = connection;
                    break;
                }

                case WebRtcSignalType::RoomList: {
                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::RoomList;
                    directTarget = connection;
                    for(const auto& [roomId, room] : m_rooms) {
                        if(room.members.empty()) continue;
                        WebRtcSignalingRoomInfo info;
                        info.roomId = roomId;
                        info.passwordProtected = room.passwordProtected();
                        directMessage->rooms.push_back(std::move(info));
                    }
                    break;
                }

                case WebRtcSignalType::CreateRoom: {
                    if(message.roomId.empty() || message.peerId.empty()) {
                        sendError(connection, "Missing room id or peer id");
                        return;
                    }

                    const auto existingRoom = m_rooms.find(message.roomId);
                    if(existingRoom != m_rooms.end() && !existingRoom->second.members.empty()) {
                        sendError(connection, "Room already exists");
                        return;
                    }

                    client.peerId = message.peerId;
                    client.roomId = message.roomId;
                    RoomState& room = m_rooms[message.roomId];
                    room.password = message.password;
                    room.maxParticipants = message.maxParticipants > 0
                        ? static_cast<size_t>(message.maxParticipants)
                        : static_cast<size_t>(2);
                    room.owner = connection;
                    room.ownerPeerId = message.peerId;
                    room.members.insert(connection);

                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::RoomJoined;
                    directMessage->roomId = message.roomId;
                    directMessage->peerId = message.peerId;
                    directTarget = connection;
                    break;
                }

                case WebRtcSignalType::JoinRoom: {
                    if(message.roomId.empty() || message.peerId.empty()) {
                        sendError(connection, "Missing room id or peer id");
                        return;
                    }

                    auto roomIt = m_rooms.find(message.roomId);
                    if(roomIt == m_rooms.end() || roomIt->second.members.empty()) {
                        sendError(connection, "Room does not exist");
                        return;
                    }
                    if(roomIt->second.maxParticipants > 0 &&
                       roomIt->second.members.size() >= roomIt->second.maxParticipants) {
                        sendError(connection, "Room is full");
                        return;
                    }
                    if(roomIt->second.password != message.password) {
                        sendError(connection, "Invalid room password");
                        return;
                    }

                    client.peerId = message.peerId;
                    client.roomId = message.roomId;
                    roomIt->second.members.insert(connection);

                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::RoomJoined;
                    directMessage->roomId = message.roomId;
                    directMessage->peerId = message.peerId;
                    directTarget = connection;

                    broadcastMessage = WebRtcSignalingMessage{};
                    broadcastMessage->type = WebRtcSignalType::PeerJoined;
                    broadcastMessage->roomId = message.roomId;
                    broadcastMessage->peerId = message.peerId;
                    broadcastRoomId = message.roomId;
                    break;
                }

                case WebRtcSignalType::LeaveRoom: {
                    if(client.roomId.empty()) {
                        break;
                    }

                    const std::string roomId = client.roomId;
                    const std::string peerId = client.peerId;
                    auto roomIt = m_rooms.find(roomId);
                    if(roomIt != m_rooms.end()) {
                        roomIt->second.members.erase(connection);
                        if(roomIt->second.owner == connection) {
                            std::vector<Connection> orphanConnections;
                            orphanConnections.reserve(roomIt->second.members.size());
                            for(const auto& member : roomIt->second.members) {
                                orphanConnections.push_back(member);
                            }
                            m_rooms.erase(roomIt);
                            client.roomId.clear();
                            for(const auto& orphan : orphanConnections) {
                                try {
                                    orphan->resetCallbacks();
                                    orphan->forceClose();
                                } catch(...) {
                                }
                            }
                            break;
                        }
                        if(roomIt->second.members.empty()) {
                            m_rooms.erase(roomIt);
                        }
                    }

                    client.roomId.clear();
                    broadcastMessage = WebRtcSignalingMessage{};
                    broadcastMessage->type = WebRtcSignalType::PeerLeft;
                    broadcastMessage->roomId = roomId;
                    broadcastMessage->peerId = peerId;
                    broadcastRoomId = roomId;
                    break;
                }

                case WebRtcSignalType::Offer:
                case WebRtcSignalType::Answer:
                case WebRtcSignalType::IceCandidate: {
                    if(client.roomId.empty()) {
                        sendError(connection, "Join a room before sending signaling data");
                        return;
                    }
                    if(message.targetPeerId.empty()) {
                        sendError(connection, "Missing target peer id");
                        return;
                    }

                    directTarget = findPeerInRoom(client.roomId, message.targetPeerId);
                    if(!directTarget.has_value()) {
                        sendError(connection, "Target peer is not connected");
                        return;
                    }

                    directMessage = message;
                    directMessage->roomId = client.roomId;
                    directMessage->peerId = client.peerId;
                    break;
                }

                default:
                    sendError(connection, "Unsupported signaling message");
                    return;
            }
        }

        if(directMessage.has_value() && directTarget.has_value()) {
            sendMessage(*directTarget, *directMessage);
        }

        if(broadcastMessage.has_value() && !broadcastRoomId.empty()) {
            broadcastToRoom(broadcastRoomId, *broadcastMessage, connection);
        }
    }

    void attachClientCallbacks(const Connection& connection)
    {
        {
            std::scoped_lock lock(m_mutex);
            m_clients.try_emplace(connection);
        }

        std::weak_ptr<rtc::WebSocket> weakConnection = connection;
        const std::shared_ptr<CallbackContext> callbackContext = m_callbackContext;
        connection->onMessage([weakConnection, callbackContext](rtc::message_variant data) {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            const auto connection = weakConnection.lock();
            if(!connection) return;
            const auto* text = std::get_if<std::string>(&data);
            if(text == nullptr) return;
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
            if(const auto connection = weakConnection.lock(); connection) {
                try {
                    self->handleDisconnect(connection);
                } catch(...) {
                }
            }
        });
        connection->onError([weakConnection, callbackContext](std::string) {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            if(const auto connection = weakConnection.lock(); connection) {
                try {
                    self->handleDisconnect(connection);
                } catch(...) {
                }
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
            m_callbackContext = std::make_shared<CallbackContext>();
            m_callbackContext->owner = this;
            const std::shared_ptr<CallbackContext> callbackContext = m_callbackContext;
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

        for(const auto& connection : connections) {
            if(connection) {
                connection->resetCallbacks();
                connection->forceClose();
            }
        }

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
