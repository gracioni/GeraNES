#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace Netplay {

struct WebRtcSignalingConfig
{
    std::string url;
    std::string roomId;

    bool valid() const
    {
        return !url.empty() && !roomId.empty();
    }
};

enum class WebRtcSignalType
{
    Hello,
    Welcome,
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
    std::string error;

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
        if(!error.empty()) json["error"] = error;
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
        message.error = json.value("error", std::string{});
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
