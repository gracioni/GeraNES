#pragma once

#include <cstdint>
#include <cstdlib>
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

    bool valid() const
    {
        return !url.empty() && !roomId.empty();
    }
};

inline std::string buildWebRtcSignalingUrl(const std::string& hostName, uint16_t port)
{
    return "ws://" + hostName + ":" + std::to_string(static_cast<unsigned>(port));
}

inline std::optional<uint16_t> parseWebRtcSignalingUrlPort(const std::string& url)
{
    if(url.empty()) return std::nullopt;

    const size_t schemeEnd = url.find("://");
    const size_t hostStart = schemeEnd == std::string::npos ? 0 : schemeEnd + 3;
    if(hostStart >= url.size()) return std::nullopt;

    const size_t pathStart = url.find('/', hostStart);
    const size_t hostEnd = pathStart == std::string::npos ? url.size() : pathStart;
    if(hostEnd <= hostStart) return std::nullopt;

    const std::string authority = url.substr(hostStart, hostEnd - hostStart);
    if(authority.empty()) return std::nullopt;

    const size_t atPos = authority.rfind('@');
    const std::string hostPort = atPos == std::string::npos ? authority : authority.substr(atPos + 1);
    if(hostPort.empty()) return std::nullopt;

    const bool bracketedIpv6 = !hostPort.empty() && hostPort.front() == '[';
    size_t colonPos = std::string::npos;
    if(bracketedIpv6) {
        const size_t closingBracket = hostPort.find(']');
        if(closingBracket == std::string::npos || closingBracket + 1 >= hostPort.size() || hostPort[closingBracket + 1] != ':') {
            return std::nullopt;
        }
        colonPos = closingBracket + 1;
    } else {
        colonPos = hostPort.rfind(':');
        if(colonPos == std::string::npos) {
            return std::nullopt;
        }
    }

    const std::string portText = hostPort.substr(colonPos + 1);
    if(portText.empty()) return std::nullopt;

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(portText.c_str(), &end, 10);
    if(end == nullptr || *end != '\0' || parsed == 0 || parsed > 65535ul) {
        return std::nullopt;
    }

    return static_cast<uint16_t>(parsed);
}

enum class WebRtcSignalType
{
    Hello,
    Welcome,
    RoomList,
    CreateRoom,
    JoinRoom,
    RoomJoined,
    PeerJoined,
    PeerLeft,
    Offer,
    Answer,
    IceCandidate,
    Error
};

inline const char* webRtcSignalTypeLabel(WebRtcSignalType type)
{
    switch(type) {
        case WebRtcSignalType::Hello: return "hello";
        case WebRtcSignalType::Welcome: return "welcome";
        case WebRtcSignalType::RoomList: return "room_list";
        case WebRtcSignalType::CreateRoom: return "create_room";
        case WebRtcSignalType::JoinRoom: return "join_room";
        case WebRtcSignalType::RoomJoined: return "room_joined";
        case WebRtcSignalType::PeerJoined: return "peer_joined";
        case WebRtcSignalType::PeerLeft: return "peer_left";
        case WebRtcSignalType::Offer: return "offer";
        case WebRtcSignalType::Answer: return "answer";
        case WebRtcSignalType::IceCandidate: return "ice_candidate";
        case WebRtcSignalType::Error: return "error";
        default: return "unknown";
    }
}

inline std::optional<WebRtcSignalType> parseWebRtcSignalType(const std::string& label)
{
    if(label == "hello") return WebRtcSignalType::Hello;
    if(label == "welcome") return WebRtcSignalType::Welcome;
    if(label == "room_list") return WebRtcSignalType::RoomList;
    if(label == "create_room") return WebRtcSignalType::CreateRoom;
    if(label == "join_room") return WebRtcSignalType::JoinRoom;
    if(label == "room_joined") return WebRtcSignalType::RoomJoined;
    if(label == "peer_joined") return WebRtcSignalType::PeerJoined;
    if(label == "peer_left") return WebRtcSignalType::PeerLeft;
    if(label == "offer") return WebRtcSignalType::Offer;
    if(label == "answer") return WebRtcSignalType::Answer;
    if(label == "ice_candidate") return WebRtcSignalType::IceCandidate;
    if(label == "error") return WebRtcSignalType::Error;
    return std::nullopt;
}

struct WebRtcSignalingRoomInfo
{
    std::string roomId;
    bool passwordProtected = false;

    nlohmann::json toJson() const
    {
        return {
            {"roomId", roomId},
            {"passwordProtected", passwordProtected}
        };
    }

    static std::optional<WebRtcSignalingRoomInfo> fromJson(const nlohmann::json& json)
    {
        if(!json.is_object()) return std::nullopt;
        if(!json.contains("roomId") || !json["roomId"].is_string()) return std::nullopt;

        WebRtcSignalingRoomInfo info;
        info.roomId = json["roomId"].get<std::string>();
        info.passwordProtected = json.value("passwordProtected", false);
        return info;
    }
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

    nlohmann::json toJson() const
    {
        nlohmann::json json = {
            {"version", version},
            {"type", webRtcSignalTypeLabel(type)}
        };

        if(!roomId.empty()) json["roomId"] = roomId;
        if(!peerId.empty()) json["peerId"] = peerId;
        if(!targetPeerId.empty()) json["targetPeerId"] = targetPeerId;
        if(!sdp.empty()) json["sdp"] = sdp;
        if(!candidate.empty()) json["candidate"] = candidate;
        if(!mid.empty()) json["mid"] = mid;
        if(mlineIndex >= 0) json["mlineIndex"] = mlineIndex;
        if(!password.empty()) json["password"] = password;
        if(!error.empty()) json["error"] = error;
        if(!iceServers.empty()) json["iceServers"] = iceServers;
        if(!rooms.empty()) {
            json["rooms"] = nlohmann::json::array();
            for(const auto& room : rooms) {
                json["rooms"].push_back(room.toJson());
            }
        }
        return json;
    }

    std::string toText() const
    {
        return toJson().dump();
    }

    static std::optional<WebRtcSignalingMessage> fromJson(const nlohmann::json& json)
    {
        if(!json.is_object()) return std::nullopt;
        if(!json.contains("type") || !json["type"].is_string()) return std::nullopt;

        const std::optional<WebRtcSignalType> parsedType = parseWebRtcSignalType(json["type"].get<std::string>());
        if(!parsedType.has_value()) return std::nullopt;

        WebRtcSignalingMessage message;
        message.type = *parsedType;
        message.version = json.value("version", kProtocolVersion);
        message.roomId = json.value("roomId", std::string{});
        message.peerId = json.value("peerId", std::string{});
        message.targetPeerId = json.value("targetPeerId", std::string{});
        message.sdp = json.value("sdp", std::string{});
        message.candidate = json.value("candidate", std::string{});
        message.mid = json.value("mid", std::string{});
        message.mlineIndex = json.value("mlineIndex", -1);
        message.password = json.value("password", std::string{});
        message.error = json.value("error", std::string{});
        if(json.contains("iceServers") && json["iceServers"].is_array()) {
            for(const auto& entry : json["iceServers"]) {
                if(entry.is_string()) {
                    message.iceServers.push_back(entry.get<std::string>());
                }
            }
        }
        if(json.contains("rooms") && json["rooms"].is_array()) {
            for(const auto& entry : json["rooms"]) {
                const auto parsedRoom = WebRtcSignalingRoomInfo::fromJson(entry);
                if(parsedRoom.has_value()) {
                    message.rooms.push_back(*parsedRoom);
                }
            }
        }
        return message;
    }

    static std::optional<WebRtcSignalingMessage> fromText(const std::string& text)
    {
        try {
            return fromJson(nlohmann::json::parse(text));
        } catch(...) {
            return std::nullopt;
        }
    }
};

} // namespace Netplay
