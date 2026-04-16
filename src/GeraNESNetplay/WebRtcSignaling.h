#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace Netplay {

struct WebRtcSignalingConfig
{
    std::string url;
    std::string roomId;
    std::string password;

    bool valid() const;
};

std::string buildWebRtcSignalingUrl(const std::string& hostName, uint16_t port);
std::optional<uint16_t> parseWebRtcSignalingUrlPort(const std::string& url);

enum class WebRtcSignalType
{
    Hello,
    Welcome,
    RoomList,
    CreateRoom,
    JoinRoom,
    LeaveRoom,
    RoomJoined,
    PeerJoined,
    PeerLeft,
    Offer,
    Answer,
    IceCandidate,
    Error
};

const char* webRtcSignalTypeLabel(WebRtcSignalType type);
std::optional<WebRtcSignalType> parseWebRtcSignalType(const std::string& label);

struct WebRtcSignalingRoomInfo
{
    std::string roomId;
    bool passwordProtected = false;

    nlohmann::json toJson() const;
    static std::optional<WebRtcSignalingRoomInfo> fromJson(const nlohmann::json& json);
};

struct WebRtcSignalingMessage
{
    static constexpr int kProtocolVersion = 1;

    WebRtcSignalType type = WebRtcSignalType::Hello;
    int version = kProtocolVersion;
    std::string roomId;
    std::string peerId;
    std::string targetPeerId;
    std::string sdp;
    std::string candidate;
    std::string mid;
    int mlineIndex = -1;
    std::string password;
    std::string error;
    std::vector<std::string> iceServers;
    std::vector<WebRtcSignalingRoomInfo> rooms;
    int maxParticipants = 0;

    nlohmann::json toJson() const;
    std::string toText() const;

    static std::optional<WebRtcSignalingMessage> fromJson(const nlohmann::json& json);
    static std::optional<WebRtcSignalingMessage> fromText(const std::string& text);
};

} // namespace Netplay
