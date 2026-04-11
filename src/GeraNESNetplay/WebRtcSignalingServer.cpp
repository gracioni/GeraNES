#include "GeraNESNetplay/WebRtcSignalingServer.h"

#include <chrono>
#include <condition_variable>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#include "GeraNESNetplay/WebRtcSignaling.h"

#if !defined(__EMSCRIPTEN__)
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#endif

namespace Netplay {

namespace {

#if !defined(__EMSCRIPTEN__)
std::string describeWebRtcStartError(const websocketpp::lib::error_code& ec)
{
    using websocketpp::lib::asio::error::access_denied;
    using websocketpp::lib::asio::error::address_family_not_supported;
    using websocketpp::lib::asio::error::address_in_use;
    using websocketpp::lib::asio::error::connection_refused;
    using websocketpp::lib::asio::error::host_unreachable;
    using websocketpp::lib::asio::error::network_down;
    using websocketpp::lib::asio::error::network_unreachable;

    if(ec == address_in_use) {
        return "Embedded WebRTC signaling server failed to start: address already in use";
    }
    if(ec == access_denied) {
        return "Embedded WebRTC signaling server failed to start: access denied";
    }
    if(ec == std::make_error_code(std::errc::address_not_available)) {
        return "Embedded WebRTC signaling server failed to start: address not available";
    }
    if(ec == address_family_not_supported) {
        return "Embedded WebRTC signaling server failed to start: address family not supported";
    }
    if(ec == network_unreachable) {
        return "Embedded WebRTC signaling server failed to start: network unreachable";
    }
    if(ec == network_down) {
        return "Embedded WebRTC signaling server failed to start: network down";
    }
    if(ec == host_unreachable) {
        return "Embedded WebRTC signaling server failed to start: host unreachable";
    }
    if(ec == connection_refused) {
        return "Embedded WebRTC signaling server failed to start: connection refused";
    }
    if(ec) {
        return "Embedded WebRTC signaling server failed to start: socket error " + std::to_string(ec.value());
    }
    return "Embedded WebRTC signaling server failed to start";
}

class DesktopWebRtcSignalingServer final : public IWebRtcSignalingServer
{
private:
    using Server = websocketpp::server<websocketpp::config::asio>;

    struct ClientState
    {
        std::string peerId;
        std::string roomId;
    };

    struct RoomState
    {
        std::string password;
        size_t maxParticipants = 2;
        std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> members;

        bool passwordProtected() const
        {
            return !password.empty();
        }
    };

    std::unique_ptr<Server> m_server;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_startCondition;
    std::map<websocketpp::connection_hdl, ClientState, std::owner_less<websocketpp::connection_hdl>> m_clients;
    std::unordered_map<std::string, RoomState> m_rooms;
    bool m_startResolved = false;
    bool m_startSucceeded = false;
    bool m_running = false;
    uint16_t m_port = 0;
    std::string m_lastError;

    static bool sameConnection(const websocketpp::connection_hdl& lhs, const websocketpp::connection_hdl& rhs)
    {
        return !lhs.owner_before(rhs) && !rhs.owner_before(lhs);
    }

    void resolveStart(bool success, const std::string& error = {})
    {
        {
            std::scoped_lock lock(m_mutex);
            m_startResolved = true;
            m_startSucceeded = success;
            if(!error.empty()) {
                m_lastError = error;
            }
        }
        m_startCondition.notify_all();
    }

    void sendMessage(const websocketpp::connection_hdl& hdl, const WebRtcSignalingMessage& message)
    {
        if(!m_server) return;
        websocketpp::lib::error_code ec;
        m_server->send(hdl, message.toText(), websocketpp::frame::opcode::text, ec);
    }

    void sendError(const websocketpp::connection_hdl& hdl, const std::string& error)
    {
        WebRtcSignalingMessage message;
        message.type = WebRtcSignalType::Error;
        message.error = error;
        sendMessage(hdl, message);
    }

    std::optional<websocketpp::connection_hdl> findPeerInRoom(const std::string& roomId,
                                                              const std::string& peerId) const
    {
        const auto roomIt = m_rooms.find(roomId);
        if(roomIt == m_rooms.end()) return std::nullopt;

        for(const auto& hdl : roomIt->second.members) {
            const auto clientIt = m_clients.find(hdl);
            if(clientIt != m_clients.end() && clientIt->second.peerId == peerId) {
                return hdl;
            }
        }

        return std::nullopt;
    }

    void broadcastToRoom(const std::string& roomId,
                         const WebRtcSignalingMessage& message,
                         const websocketpp::connection_hdl* except = nullptr)
    {
        std::vector<websocketpp::connection_hdl> recipients;
        {
            std::scoped_lock lock(m_mutex);
            const auto roomIt = m_rooms.find(roomId);
            if(roomIt == m_rooms.end()) return;

            for(const auto& hdl : roomIt->second.members) {
                if(except != nullptr && sameConnection(hdl, *except)) {
                    continue;
                }
                recipients.push_back(hdl);
            }
        }

        for(const auto& hdl : recipients) {
            sendMessage(hdl, message);
        }
    }

    void removeClientLocked(const websocketpp::connection_hdl& hdl, std::optional<WebRtcSignalingMessage>& peerLeft)
    {
        const auto clientIt = m_clients.find(hdl);
        if(clientIt == m_clients.end()) return;

        const std::string roomId = clientIt->second.roomId;
        const std::string peerId = clientIt->second.peerId;
        m_clients.erase(clientIt);

        if(!roomId.empty()) {
            auto roomIt = m_rooms.find(roomId);
            if(roomIt != m_rooms.end()) {
                roomIt->second.members.erase(hdl);
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

    void handleMessage(const websocketpp::connection_hdl& hdl, const std::string& payload)
    {
        const auto parsed = WebRtcSignalingMessage::fromText(payload);
        if(!parsed.has_value()) {
            sendError(hdl, "Invalid signaling payload");
            return;
        }

        std::optional<WebRtcSignalingMessage> directMessage;
        std::optional<WebRtcSignalingMessage> broadcastMessage;
        std::optional<websocketpp::connection_hdl> directTarget;
        std::string broadcastRoomId;

        {
            std::scoped_lock lock(m_mutex);
            ClientState& client = m_clients[hdl];
            const WebRtcSignalingMessage& message = *parsed;

            switch(message.type) {
                case WebRtcSignalType::Hello: {
                    if(message.peerId.empty()) {
                        sendError(hdl, "Missing peer id");
                        return;
                    }
                    client.peerId = message.peerId;

                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::Welcome;
                    directMessage->roomId = message.roomId;
                    directMessage->peerId = message.peerId;
                    directTarget = hdl;
                    break;
                }

                case WebRtcSignalType::RoomList: {
                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::RoomList;
                    directTarget = hdl;
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
                        sendError(hdl, "Missing room id or peer id");
                        return;
                    }

                    const auto existingRoom = m_rooms.find(message.roomId);
                    if(existingRoom != m_rooms.end() && !existingRoom->second.members.empty()) {
                        sendError(hdl, "Room already exists");
                        return;
                    }

                    client.peerId = message.peerId;
                    client.roomId = message.roomId;
                    RoomState& room = m_rooms[message.roomId];
                    room.password = message.password;
                    room.maxParticipants = message.maxParticipants > 0
                        ? static_cast<size_t>(message.maxParticipants)
                        : static_cast<size_t>(2);
                    room.members.insert(hdl);

                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::RoomJoined;
                    directMessage->roomId = message.roomId;
                    directMessage->peerId = message.peerId;
                    directTarget = hdl;
                    break;
                }

                case WebRtcSignalType::JoinRoom: {
                    if(message.roomId.empty() || message.peerId.empty()) {
                        sendError(hdl, "Missing room id or peer id");
                        return;
                    }

                    auto roomIt = m_rooms.find(message.roomId);
                    if(roomIt == m_rooms.end() || roomIt->second.members.empty()) {
                        sendError(hdl, "Room does not exist");
                        return;
                    }
                    if(roomIt->second.maxParticipants > 0 &&
                       roomIt->second.members.size() >= roomIt->second.maxParticipants) {
                        sendError(hdl, "Room is full");
                        return;
                    }
                    if(roomIt->second.password != message.password) {
                        sendError(hdl, "Invalid room password");
                        return;
                    }

                    client.peerId = message.peerId;
                    client.roomId = message.roomId;
                    roomIt->second.members.insert(hdl);

                    directMessage = WebRtcSignalingMessage{};
                    directMessage->type = WebRtcSignalType::RoomJoined;
                    directMessage->roomId = message.roomId;
                    directMessage->peerId = message.peerId;
                    directTarget = hdl;

                    broadcastMessage = WebRtcSignalingMessage{};
                    broadcastMessage->type = WebRtcSignalType::PeerJoined;
                    broadcastMessage->roomId = message.roomId;
                    broadcastMessage->peerId = message.peerId;
                    broadcastRoomId = message.roomId;
                    break;
                }

                case WebRtcSignalType::Offer:
                case WebRtcSignalType::Answer:
                case WebRtcSignalType::IceCandidate: {
                    if(client.roomId.empty()) {
                        sendError(hdl, "Join a room before sending signaling data");
                        return;
                    }
                    if(message.targetPeerId.empty()) {
                        sendError(hdl, "Missing target peer id");
                        return;
                    }

                    directTarget = findPeerInRoom(client.roomId, message.targetPeerId);
                    if(!directTarget.has_value()) {
                        sendError(hdl, "Target peer is not connected");
                        return;
                    }

                    directMessage = message;
                    directMessage->roomId = client.roomId;
                    directMessage->peerId = client.peerId;
                    break;
                }

                default:
                    sendError(hdl, "Unsupported signaling message");
                    return;
            }
        }

        if(directMessage.has_value() && directTarget.has_value()) {
            sendMessage(*directTarget, *directMessage);
        }

        if(broadcastMessage.has_value() && !broadcastRoomId.empty()) {
            broadcastToRoom(broadcastRoomId, *broadcastMessage, &hdl);
        }
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
            m_server = std::make_unique<Server>();
            m_server->clear_access_channels(websocketpp::log::alevel::all);
            m_server->clear_error_channels(websocketpp::log::elevel::all);
            m_server->init_asio();
            m_server->set_tcp_pre_bind_handler([](Server::transport_type::acceptor_ptr acceptor) {
                websocketpp::lib::error_code ec;
                acceptor->set_option(websocketpp::lib::asio::ip::v6_only(false), ec);
                return ec;
            });

            {
                std::scoped_lock lock(m_mutex);
                m_startResolved = false;
                m_startSucceeded = false;
                m_running = false;
                m_lastError.clear();
                m_port = port;
                m_clients.clear();
                m_rooms.clear();
            }

            m_server->set_open_handler([this](websocketpp::connection_hdl hdl) {
                std::scoped_lock lock(m_mutex);
                m_clients.try_emplace(hdl);
            });

            m_server->set_close_handler([this](websocketpp::connection_hdl hdl) {
                std::optional<WebRtcSignalingMessage> peerLeft;
                {
                    std::scoped_lock lock(m_mutex);
                    removeClientLocked(hdl, peerLeft);
                }

                if(peerLeft.has_value()) {
                    broadcastToRoom(peerLeft->roomId, *peerLeft, &hdl);
                }
            });

            m_server->set_fail_handler([this](websocketpp::connection_hdl hdl) {
                std::optional<WebRtcSignalingMessage> peerLeft;
                {
                    std::scoped_lock lock(m_mutex);
                    removeClientLocked(hdl, peerLeft);
                }

                if(peerLeft.has_value()) {
                    broadcastToRoom(peerLeft->roomId, *peerLeft, &hdl);
                }
            });

            m_server->set_message_handler([this](websocketpp::connection_hdl hdl, Server::message_ptr message) {
                handleMessage(hdl, message->get_payload());
            });

            m_server->listen(websocketpp::lib::asio::ip::tcp::endpoint(
                websocketpp::lib::asio::ip::tcp::v6(),
                port));
            m_server->start_accept();

            m_thread = std::thread([this]() {
                resolveStart(true);
                try {
                    if(m_server) {
                        m_server->run();
                    }
                } catch(const std::exception& ex) {
                    std::scoped_lock lock(m_mutex);
                    m_lastError = std::string("Embedded WebRTC signaling server failed: ") + ex.what();
                } catch(...) {
                    std::scoped_lock lock(m_mutex);
                    m_lastError = "Embedded WebRTC signaling server failed";
                }

                std::scoped_lock lock(m_mutex);
                m_running = false;
            });

            std::unique_lock lock(m_mutex);
            const bool resolved = m_startCondition.wait_for(
                lock,
                std::chrono::milliseconds(1500),
                [this]() { return m_startResolved; });
            if(!resolved || !m_startSucceeded) {
                lock.unlock();
                stop();
                if(m_lastError.empty()) {
                    m_lastError = "Embedded WebRTC signaling server failed to start";
                }
                return false;
            }

            m_running = true;
            return true;
        } catch(const websocketpp::lib::system_error& ex) {
            m_lastError = describeWebRtcStartError(ex.code());
        } catch(const std::exception&) {
            m_lastError = "Embedded WebRTC signaling server failed to start";
        } catch(...) {
            m_lastError = "Embedded WebRTC signaling server failed to start";
        }

        stop();
        return false;
    }

    void stop() override
    {
        if(m_server) {
            try {
                m_server->stop_listening();
            } catch(...) {
            }

            try {
                std::vector<websocketpp::connection_hdl> clients;
                {
                    std::scoped_lock lock(m_mutex);
                    for(const auto& [hdl, state] : m_clients) {
                        (void)state;
                        clients.push_back(hdl);
                    }
                }

                for(const auto& hdl : clients) {
                    websocketpp::lib::error_code ec;
                    m_server->close(hdl, websocketpp::close::status::going_away, "", ec);
                }
            } catch(...) {
            }

            try {
                m_server->stop();
            } catch(...) {
            }
        }

        if(m_thread.joinable()) {
            m_thread.join();
        }

        {
            std::scoped_lock lock(m_mutex);
            m_clients.clear();
            m_rooms.clear();
            m_running = false;
            m_port = 0;
            m_startResolved = false;
            m_startSucceeded = false;
        }

        m_server.reset();
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

    const std::string& lastError() const override
    {
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

    const std::string& lastError() const override
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
