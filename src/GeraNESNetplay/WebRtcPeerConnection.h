#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Netplay {

struct WebRtcPeerConnectionOptions
{
    std::string localPeerId;
    std::string remotePeerId;
    std::vector<std::string> iceServers;
    bool host = false;
};

class IWebRtcPeerConnection
{
public:
    struct Event
    {
        enum class Type : uint8_t
        {
            None,
            LocalDescriptionReady,
            IceCandidateReady,
            DataChannelOpen,
            DataChannelClosed,
            DataMessage,
            Error
        };

        Type type = Type::None;
        std::string sdp;
        bool descriptionIsOffer = false;
        std::string candidate;
        std::string mid;
        int mlineIndex = -1;
        std::vector<uint8_t> payload;
        std::string text;
    };

    virtual ~IWebRtcPeerConnection() = default;

    virtual bool open(const WebRtcPeerConnectionOptions& options) = 0;
    virtual void close() = 0;
    virtual bool createOffer() = 0;
    virtual bool createAnswer() = 0;
    virtual bool setRemoteDescription(const std::string& sdp, bool offer) = 0;
    virtual bool addRemoteIceCandidate(const std::string& candidate,
                                       const std::string& mid,
                                       int mlineIndex) = 0;
    virtual bool sendDataReliable(const std::vector<uint8_t>& payload) = 0;
    virtual bool sendDataUnreliable(const std::vector<uint8_t>& payload) = 0;
    virtual size_t bufferedAmount() const = 0;
    virtual std::vector<Event> poll() = 0;
    virtual bool isOpen() const = 0;
    virtual bool isDataChannelOpen() const = 0;
    virtual std::string lastError() const = 0;
};

std::unique_ptr<IWebRtcPeerConnection> createWebRtcPeerConnection();

} // namespace Netplay
