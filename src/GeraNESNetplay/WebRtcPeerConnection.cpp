#include "GeraNESNetplay/WebRtcPeerConnection.h"

#include <deque>
#include <memory>
#include <mutex>

#if !defined(__EMSCRIPTEN__)
#include <rtc/rtc.h>
#endif

namespace Netplay {

namespace {

#if !defined(__EMSCRIPTEN__)
constexpr const char* kNetplayDataChannelLabel = "geranes";

class DesktopWebRtcPeerConnection final : public IWebRtcPeerConnection
{
private:
    int m_peerConnection = 0;
    int m_dataChannel = 0;
    bool m_open = false;
    bool m_dataChannelOpen = false;
    std::string m_lastError;
    std::deque<Event> m_events;
    mutable std::mutex m_mutex;

    void pushEvent(Event&& event)
    {
        std::scoped_lock lock(m_mutex);
        m_events.push_back(std::move(event));
    }

    void setLastError(const std::string& error)
    {
        std::scoped_lock lock(m_mutex);
        m_lastError = error;
    }

    static void onLocalDescriptionCallback(int pc, const char* sdp, const char* type, void* userPtr)
    {
        (void)pc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;

        Event event;
        event.type = Event::Type::LocalDescriptionReady;
        event.sdp = sdp != nullptr ? sdp : "";
        event.descriptionIsOffer = type != nullptr && std::string(type) == "offer";
        self->pushEvent(std::move(event));
    }

    static void onLocalCandidateCallback(int pc, const char* candidate, const char* mid, void* userPtr)
    {
        (void)pc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;

        Event event;
        event.type = Event::Type::IceCandidateReady;
        event.candidate = candidate != nullptr ? candidate : "";
        event.mid = mid != nullptr ? mid : "";
        self->pushEvent(std::move(event));
    }

    static void onStateChangeCallback(int pc, rtcState state, void* userPtr)
    {
        (void)pc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;

        if(state == RTC_FAILED) {
            self->setLastError("WebRTC peer connection failed");
            self->pushEvent(Event{Event::Type::Error, {}, false, {}, {}, -1, {}, "WebRTC peer connection failed"});
        } else if(state == RTC_DISCONNECTED || state == RTC_CLOSED) {
            self->pushEvent(Event{Event::Type::DataChannelClosed});
        }
    }

    static void onDataChannelCallback(int pc, int dc, void* userPtr)
    {
        (void)pc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;
        self->attachDataChannel(dc);
    }

    static void onChannelOpenCallback(int dc, void* userPtr)
    {
        (void)dc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;
        self->m_dataChannelOpen = true;
        self->pushEvent(Event{Event::Type::DataChannelOpen});
    }

    static void onChannelClosedCallback(int dc, void* userPtr)
    {
        (void)dc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;
        self->m_dataChannelOpen = false;
        self->pushEvent(Event{Event::Type::DataChannelClosed});
    }

    static void onChannelErrorCallback(int dc, const char* error, void* userPtr)
    {
        (void)dc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;
        const std::string text = error != nullptr ? error : "WebRTC data channel error";
        self->setLastError(text);
        self->pushEvent(Event{Event::Type::Error, {}, false, {}, {}, -1, {}, text});
    }

    static void onChannelMessageCallback(int dc, const char* message, int size, void* userPtr)
    {
        (void)dc;
        auto* self = static_cast<DesktopWebRtcPeerConnection*>(userPtr);
        if(self == nullptr) return;

        Event event;
        event.type = Event::Type::DataMessage;
        if(message != nullptr && size > 0) {
            event.payload.assign(reinterpret_cast<const uint8_t*>(message),
                                 reinterpret_cast<const uint8_t*>(message) + size);
        }
        self->pushEvent(std::move(event));
    }

    void attachDataChannel(int dataChannel)
    {
        if(dataChannel <= 0) return;

        if(m_dataChannel > 0 && m_dataChannel != dataChannel) {
            rtcDeleteDataChannel(m_dataChannel);
        }

        m_dataChannel = dataChannel;
        rtcSetUserPointer(m_dataChannel, this);
        rtcSetOpenCallback(m_dataChannel, &DesktopWebRtcPeerConnection::onChannelOpenCallback);
        rtcSetClosedCallback(m_dataChannel, &DesktopWebRtcPeerConnection::onChannelClosedCallback);
        rtcSetErrorCallback(m_dataChannel, &DesktopWebRtcPeerConnection::onChannelErrorCallback);
        rtcSetMessageCallback(m_dataChannel, &DesktopWebRtcPeerConnection::onChannelMessageCallback);
    }

public:
    ~DesktopWebRtcPeerConnection() override
    {
        close();
    }

    bool open(const WebRtcPeerConnectionOptions& options) override
    {
        close();

        rtcConfiguration config{};
        config.disableAutoNegotiation = true;
        m_peerConnection = rtcCreatePeerConnection(&config);
        if(m_peerConnection <= 0) {
            m_lastError = "Failed to create WebRTC peer connection";
            return false;
        }

        rtcSetUserPointer(m_peerConnection, this);
        rtcSetLocalDescriptionCallback(m_peerConnection, &DesktopWebRtcPeerConnection::onLocalDescriptionCallback);
        rtcSetLocalCandidateCallback(m_peerConnection, &DesktopWebRtcPeerConnection::onLocalCandidateCallback);
        rtcSetStateChangeCallback(m_peerConnection, &DesktopWebRtcPeerConnection::onStateChangeCallback);
        rtcSetDataChannelCallback(m_peerConnection, &DesktopWebRtcPeerConnection::onDataChannelCallback);

        if(options.host) {
            rtcDataChannelInit channelInit{};
            channelInit.reliability.unordered = false;
            channelInit.reliability.unreliable = false;
            const int dataChannel = rtcCreateDataChannelEx(m_peerConnection, kNetplayDataChannelLabel, &channelInit);
            if(dataChannel <= 0) {
                m_lastError = "Failed to create WebRTC data channel";
                close();
                return false;
            }
            attachDataChannel(dataChannel);
        }

        m_open = true;
        m_lastError.clear();
        return true;
    }

    void close() override
    {
        if(m_dataChannel > 0) {
            rtcDeleteDataChannel(m_dataChannel);
            m_dataChannel = 0;
        }

        if(m_peerConnection > 0) {
            rtcDeletePeerConnection(m_peerConnection);
            m_peerConnection = 0;
        }

        m_open = false;
        m_dataChannelOpen = false;

        std::scoped_lock lock(m_mutex);
        m_events.clear();
    }

    bool createOffer() override
    {
        if(!m_open || m_peerConnection <= 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(rtcSetLocalDescription(m_peerConnection, "offer") != RTC_ERR_SUCCESS) {
            m_lastError = "Failed to create WebRTC offer";
            return false;
        }
        return true;
    }

    bool createAnswer() override
    {
        if(!m_open || m_peerConnection <= 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(rtcSetLocalDescription(m_peerConnection, "answer") != RTC_ERR_SUCCESS) {
            m_lastError = "Failed to create WebRTC answer";
            return false;
        }
        return true;
    }

    bool setRemoteDescription(const std::string& sdp, bool offer) override
    {
        if(!m_open || m_peerConnection <= 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(rtcSetRemoteDescription(m_peerConnection, sdp.c_str(), offer ? "offer" : "answer") != RTC_ERR_SUCCESS) {
            m_lastError = "Failed to set remote WebRTC description";
            return false;
        }
        return true;
    }

    bool addRemoteIceCandidate(const std::string& candidate,
                               const std::string& mid,
                               int mlineIndex) override
    {
        (void)mlineIndex;
        if(!m_open || m_peerConnection <= 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(rtcAddRemoteCandidate(m_peerConnection, candidate.c_str(), mid.empty() ? nullptr : mid.c_str()) != RTC_ERR_SUCCESS) {
            m_lastError = "Failed to add remote WebRTC ICE candidate";
            return false;
        }
        return true;
    }

    bool sendDataReliable(const std::vector<uint8_t>& payload) override
    {
        if(!m_dataChannelOpen || m_dataChannel <= 0) {
            m_lastError = "WebRTC data channel is not open";
            return false;
        }
        if(rtcSendMessage(m_dataChannel,
                          reinterpret_cast<const char*>(payload.data()),
                          static_cast<int>(payload.size())) != RTC_ERR_SUCCESS) {
            m_lastError = "Failed to send WebRTC data channel payload";
            return false;
        }
        return true;
    }

    bool sendDataUnreliable(const std::vector<uint8_t>& payload) override
    {
        return sendDataReliable(payload);
    }

    std::vector<Event> poll() override
    {
        std::vector<Event> events;
        std::scoped_lock lock(m_mutex);
        while(!m_events.empty()) {
            events.push_back(std::move(m_events.front()));
            m_events.pop_front();
        }
        return events;
    }

    bool isOpen() const override
    {
        return m_open;
    }

    bool isDataChannelOpen() const override
    {
        return m_dataChannelOpen;
    }

    const std::string& lastError() const override
    {
        return m_lastError;
    }
};
#endif

class StubWebRtcPeerConnection final : public IWebRtcPeerConnection
{
private:
    bool m_open = false;
    std::string m_lastError;

public:
    bool open(const WebRtcPeerConnectionOptions& options) override
    {
        (void)options;
#if defined(__EMSCRIPTEN__)
        m_lastError = "WebRTC peer connection bridge is not implemented yet in the web build";
#else
        m_lastError = "WebRTC peer connection is not implemented yet on desktop";
#endif
        m_open = false;
        return false;
    }

    void close() override
    {
        m_open = false;
    }

    bool createOffer() override
    {
        m_lastError = "WebRTC peer connection is not open";
        return false;
    }

    bool createAnswer() override
    {
        m_lastError = "WebRTC peer connection is not open";
        return false;
    }

    bool setRemoteDescription(const std::string& sdp, bool offer) override
    {
        (void)sdp;
        (void)offer;
        m_lastError = "WebRTC peer connection is not open";
        return false;
    }

    bool addRemoteIceCandidate(const std::string& candidate,
                               const std::string& mid,
                               int mlineIndex) override
    {
        (void)candidate;
        (void)mid;
        (void)mlineIndex;
        m_lastError = "WebRTC peer connection is not open";
        return false;
    }

    bool sendDataReliable(const std::vector<uint8_t>& payload) override
    {
        (void)payload;
        m_lastError = "WebRTC data channel is not open";
        return false;
    }

    bool sendDataUnreliable(const std::vector<uint8_t>& payload) override
    {
        (void)payload;
        m_lastError = "WebRTC data channel is not open";
        return false;
    }

    std::vector<Event> poll() override
    {
        return {};
    }

    bool isOpen() const override
    {
        return m_open;
    }

    bool isDataChannelOpen() const override
    {
        return false;
    }

    const std::string& lastError() const override
    {
        return m_lastError;
    }
};

} // namespace

std::unique_ptr<IWebRtcPeerConnection> createWebRtcPeerConnection()
{
#if !defined(__EMSCRIPTEN__)
    return std::make_unique<DesktopWebRtcPeerConnection>();
#else
    return std::make_unique<StubWebRtcPeerConnection>();
#endif
}

} // namespace Netplay
