#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Netplay {

class IWebRtcSignalingServer
{
public:
    virtual ~IWebRtcSignalingServer() = default;

    virtual bool start(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual uint16_t port() const = 0;
    virtual std::string lastError() const = 0;
};

std::unique_ptr<IWebRtcSignalingServer> createWebRtcSignalingServer();

} // namespace Netplay
