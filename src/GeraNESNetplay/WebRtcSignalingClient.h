#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "WebRtcSignaling.h"

namespace Netplay {

struct WebRtcSignalingClientOptions
{
    WebRtcSignalingConfig config;
    std::string localPeerId;
    bool host = false;
};

class IWebRtcSignalingClient
{
public:
    struct Event
    {
        enum class Type : uint8_t
        {
            None,
            Connected,
            Disconnected,
            Message,
            Error
        };

        Type type = Type::None;
        WebRtcSignalingMessage message = {};
        std::string text;
    };

    virtual ~IWebRtcSignalingClient() = default;

    virtual bool connect(const WebRtcSignalingClientOptions& options) = 0;
    virtual void disconnect() = 0;
    virtual bool send(const WebRtcSignalingMessage& message) = 0;
    virtual std::vector<Event> poll() = 0;
    virtual bool isConnected() const = 0;
    virtual const std::string& lastError() const = 0;
};

std::unique_ptr<IWebRtcSignalingClient> createWebRtcSignalingClient();

} // namespace Netplay
