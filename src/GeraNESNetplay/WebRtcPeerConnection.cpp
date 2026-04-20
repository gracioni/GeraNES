#include "GeraNESNetplay/WebRtcPeerConnection.h"
#include "logger/logger.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#if !defined(__EMSCRIPTEN__)
#include <rtc/rtc.h>
#else
#include <emscripten.h>
#include <nlohmann/json.hpp>
#endif

namespace Netplay {

namespace {

#if !defined(__EMSCRIPTEN__)
constexpr const char* kNetplayDataChannelLabel = "geranes";

class RtcCleanupQueue
{
public:
    static RtcCleanupQueue& instance()
    {
        static auto* queue = new RtcCleanupQueue();
        return *queue;
    }

    void enqueue(std::function<void()> task)
    {
        {
            std::scoped_lock lock(m_mutex);
            m_tasks.push_back(std::move(task));
        }
        m_condition.notify_one();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<std::function<void()>> m_tasks;
    std::thread m_worker;

    RtcCleanupQueue()
        : m_worker([this]() { run(); })
    {
    }

    void run()
    {
        for(;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(m_mutex);
                m_condition.wait(lock, [this]() { return !m_tasks.empty(); });
                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            task();
        }
    }
};

void retireRtcCallbackContext(void* context) noexcept
{
    // libdatachannel callbacks can still race briefly with teardown even after
    // callbacks/user pointers are cleared. Keep retired callback contexts alive
    // for process lifetime and rely on the alive flag to make them inert.
    static std::mutex retiredMutex;
    static std::vector<void*> retiredContexts;
    if(context == nullptr) {
        return;
    }
    std::scoped_lock lock(retiredMutex);
    retiredContexts.push_back(context);
}

void onRtcLog(rtcLogLevel level, const char* message)
{
    Logger::Type type = Logger::Type::INFO;
    switch(level) {
        case RTC_LOG_FATAL:
        case RTC_LOG_ERROR:
            type = Logger::Type::ERROR;
            break;
        case RTC_LOG_WARNING:
            type = Logger::Type::WARNING;
            break;
        case RTC_LOG_DEBUG:
        case RTC_LOG_VERBOSE:
            type = Logger::Type::DEBUG;
            break;
        case RTC_LOG_NONE:
        case RTC_LOG_INFO:
        default:
            type = Logger::Type::INFO;
            break;
    }

    Logger::instance().log(std::string("[libdatachannel] ") + (message != nullptr ? message : ""), type);
}

void ensureRtcLoggerInitialized()
{
    static std::once_flag once;
    std::call_once(once, []() {
        rtcInitLogger(RTC_LOG_INFO, &onRtcLog);
    });
}

std::string trimAsciiWhitespace(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    });
    if(first == value.end()) {
        return {};
    }

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }).base();
    return std::string(first, last);
}

int createPeerConnectionWithIceServers(const std::vector<std::string>& iceServers)
{
    rtcConfiguration config{};
    config.disableAutoNegotiation = true;

    std::vector<const char*> iceServerViews;
    if(!iceServers.empty()) {
        iceServerViews.reserve(iceServers.size());
        for(const auto& iceServer : iceServers) {
            iceServerViews.push_back(iceServer.c_str());
        }
        config.iceServers = iceServerViews.data();
        config.iceServersCount = static_cast<int>(iceServerViews.size());
    }

    return rtcCreatePeerConnection(&config);
}

template<typename Fn>
void enqueueRtcCleanup(Fn&& task)
{
    RtcCleanupQueue::instance().enqueue(std::forward<Fn>(task));
}

class DesktopWebRtcPeerConnection final : public IWebRtcPeerConnection
{
private:
    struct CallbackContext
    {
        std::atomic<bool> alive{true};
        DesktopWebRtcPeerConnection* owner = nullptr;
    };

    int m_peerConnection = 0;
    int m_dataChannel = 0;
    bool m_open = false;
    bool m_dataChannelOpen = false;
    std::string m_lastError;
    std::deque<Event> m_events;
    mutable std::mutex m_stateMutex;
    mutable std::mutex m_eventMutex;
    CallbackContext* m_peerCallbackContext = nullptr;
    CallbackContext* m_dataChannelCallbackContext = nullptr;

    static DesktopWebRtcPeerConnection* ownerFromContext(void* userPtr)
    {
        auto* context = static_cast<CallbackContext*>(userPtr);
        if(context == nullptr) {
            return nullptr;
        }
        if(!context->alive.load(std::memory_order_acquire)) {
            return nullptr;
        }
        return context->owner;
    }

    void pushEvent(Event&& event)
    {
        std::scoped_lock lock(m_eventMutex);
        m_events.push_back(std::move(event));
    }

    void queueDataChannelClosedOnce()
    {
        bool shouldEmit = false;
        {
            std::scoped_lock lock(m_stateMutex);
            if(m_dataChannelOpen) {
                m_dataChannelOpen = false;
                shouldEmit = true;
            }
        }
        if(shouldEmit) {
            pushEvent(Event{Event::Type::DataChannelClosed});
        }
    }

    void setLastError(const std::string& error)
    {
        std::scoped_lock lock(m_stateMutex);
        m_lastError = error;
    }

    static void onLocalDescriptionCallback(int pc, const char* sdp, const char* type, void* userPtr)
    {
        (void)pc;
        auto* self = ownerFromContext(userPtr);
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
        auto* self = ownerFromContext(userPtr);
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
        auto* self = ownerFromContext(userPtr);
        if(self == nullptr) return;

        if(state == RTC_FAILED) {
            self->setLastError("WebRTC peer connection failed");
            self->pushEvent(Event{Event::Type::Error, {}, false, {}, {}, -1, {}, "WebRTC peer connection failed"});
        } else if(state == RTC_DISCONNECTED || state == RTC_CLOSED) {
            self->queueDataChannelClosedOnce();
        }
    }

    static void onDataChannelCallback(int pc, int dc, void* userPtr)
    {
        (void)pc;
        auto* self = ownerFromContext(userPtr);
        if(self == nullptr) return;
        self->attachDataChannel(dc);
    }

    static void onChannelOpenCallback(int dc, void* userPtr)
    {
        (void)dc;
        auto* self = ownerFromContext(userPtr);
        if(self == nullptr) return;
        {
            std::scoped_lock lock(self->m_stateMutex);
            self->m_dataChannelOpen = true;
        }
        self->pushEvent(Event{Event::Type::DataChannelOpen});
    }

    static void onChannelClosedCallback(int dc, void* userPtr)
    {
        (void)dc;
        auto* self = ownerFromContext(userPtr);
        if(self == nullptr) return;
        self->queueDataChannelClosedOnce();
    }

    static void onChannelErrorCallback(int dc, const char* error, void* userPtr)
    {
        (void)dc;
        auto* self = ownerFromContext(userPtr);
        if(self == nullptr) return;
        const std::string text = error != nullptr ? error : "WebRTC data channel error";
        self->setLastError(text);
        self->pushEvent(Event{Event::Type::Error, {}, false, {}, {}, -1, {}, text});
    }

    static void onChannelMessageCallback(int dc, const char* message, int size, void* userPtr)
    {
        (void)dc;
        auto* self = ownerFromContext(userPtr);
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

        int previousDataChannel = 0;
        CallbackContext* previousDataChannelContext = nullptr;
        {
            std::scoped_lock lock(m_stateMutex);
            if(m_dataChannel > 0 && m_dataChannel != dataChannel) {
                previousDataChannel = m_dataChannel;
                previousDataChannelContext = m_dataChannelCallbackContext;
            }

            m_dataChannel = dataChannel;
            m_dataChannelCallbackContext = new CallbackContext();
            m_dataChannelCallbackContext->owner = this;
        }

        if(previousDataChannelContext != nullptr) {
            previousDataChannelContext->alive.store(false, std::memory_order_release);
            previousDataChannelContext->owner = nullptr;
        }
        if(previousDataChannel > 0 || previousDataChannelContext != nullptr) {
            enqueueRtcCleanup([previousDataChannel, previousDataChannelContext]() {
                try {
                    if(previousDataChannel > 0) {
                        rtcSetUserPointer(previousDataChannel, nullptr);
                        rtcSetOpenCallback(previousDataChannel, nullptr);
                        rtcSetClosedCallback(previousDataChannel, nullptr);
                        rtcSetErrorCallback(previousDataChannel, nullptr);
                        rtcSetMessageCallback(previousDataChannel, nullptr);
                        rtcDeleteDataChannel(previousDataChannel);
                    }
                } catch(...) {
                }
                retireRtcCallbackContext(previousDataChannelContext);
            });
        }

        rtcSetUserPointer(m_dataChannel, m_dataChannelCallbackContext);
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
        ensureRtcLoggerInitialized();

        std::vector<std::string> sanitizedIceServers;
        if(!options.iceServers.empty()) {
            sanitizedIceServers.reserve(options.iceServers.size());
            for(const auto& iceServer : options.iceServers) {
                std::string trimmedIceServer = trimAsciiWhitespace(iceServer);
                if(trimmedIceServer.empty()) {
                    continue;
                }
                sanitizedIceServers.push_back(std::move(trimmedIceServer));
            }
        }

        m_peerConnection = createPeerConnectionWithIceServers(sanitizedIceServers);
        if(m_peerConnection <= 0 && !sanitizedIceServers.empty()) {
            m_peerConnection = createPeerConnectionWithIceServers({});
        }
        if(m_peerConnection <= 0) {
            m_lastError = "Failed to create WebRTC peer connection";
            return false;
        }

        m_peerCallbackContext = new CallbackContext();
        m_peerCallbackContext->owner = this;
        rtcSetUserPointer(m_peerConnection, m_peerCallbackContext);
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
        int dataChannel = 0;
        int peerConnection = 0;
        CallbackContext* peerContext = nullptr;
        CallbackContext* dataChannelContext = nullptr;
        {
            std::scoped_lock lock(m_stateMutex);
            dataChannel = m_dataChannel;
            peerConnection = m_peerConnection;
            peerContext = m_peerCallbackContext;
            dataChannelContext = m_dataChannelCallbackContext;
            m_peerCallbackContext = nullptr;
            m_dataChannelCallbackContext = nullptr;
            m_dataChannel = 0;
            m_peerConnection = 0;
            m_open = false;
            m_dataChannelOpen = false;
            m_lastError.clear();
        }

        if(dataChannelContext != nullptr) {
            dataChannelContext->alive.store(false, std::memory_order_release);
            dataChannelContext->owner = nullptr;
        }
        if(peerContext != nullptr) {
            peerContext->alive.store(false, std::memory_order_release);
            peerContext->owner = nullptr;
        }

        if(dataChannel > 0 || dataChannelContext != nullptr) {
            enqueueRtcCleanup([dataChannel, dataChannelContext]() {
                try {
                    if(dataChannel > 0) {
                        rtcSetUserPointer(dataChannel, nullptr);
                        rtcSetOpenCallback(dataChannel, nullptr);
                        rtcSetClosedCallback(dataChannel, nullptr);
                        rtcSetErrorCallback(dataChannel, nullptr);
                        rtcSetMessageCallback(dataChannel, nullptr);
                        rtcDeleteDataChannel(dataChannel);
                    }
                } catch(...) {
                }
                retireRtcCallbackContext(dataChannelContext);
            });
        }

        if(peerConnection > 0 || peerContext != nullptr) {
            enqueueRtcCleanup([peerConnection, peerContext]() {
                try {
                    if(peerConnection > 0) {
                        rtcSetUserPointer(peerConnection, nullptr);
                        rtcSetLocalDescriptionCallback(peerConnection, nullptr);
                        rtcSetLocalCandidateCallback(peerConnection, nullptr);
                        rtcSetStateChangeCallback(peerConnection, nullptr);
                        rtcSetDataChannelCallback(peerConnection, nullptr);
                        rtcDeletePeerConnection(peerConnection);
                    }
                } catch(...) {
                }
                retireRtcCallbackContext(peerContext);
            });
        }

        std::scoped_lock lock(m_eventMutex);
        m_events.clear();
    }

    bool createOffer() override
    {
        if(!m_open || m_peerConnection <= 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        std::scoped_lock lock(m_stateMutex);
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
        std::scoped_lock lock(m_stateMutex);
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
        std::scoped_lock lock(m_stateMutex);
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
        std::scoped_lock lock(m_stateMutex);
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
        std::scoped_lock lock(m_stateMutex);
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

    size_t bufferedAmount() const override
    {
        std::scoped_lock lock(m_stateMutex);
        if(m_dataChannel <= 0) {
            return 0;
        }
        const int amount = rtcGetBufferedAmount(m_dataChannel);
        return amount > 0 ? static_cast<size_t>(amount) : 0u;
    }

    ConnectionPath connectionPath() const override
    {
        return ConnectionPath::Unknown;
    }

    std::vector<Event> poll() override
    {
        std::vector<Event> events;
        std::scoped_lock lock(m_eventMutex);
        while(!m_events.empty()) {
            events.push_back(std::move(m_events.front()));
            m_events.pop_front();
        }
        return events;
    }

    bool isOpen() const override
    {
        std::scoped_lock lock(m_stateMutex);
        return m_open;
    }

    bool isDataChannelOpen() const override
    {
        std::scoped_lock lock(m_stateMutex);
        return m_dataChannelOpen;
    }

    std::string lastError() const override
    {
        std::scoped_lock lock(m_stateMutex);
        return m_lastError;
    }
};
#endif

#if defined(__EMSCRIPTEN__)
class WebEmscriptenWebRtcPeerConnection;

extern "C" {
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_local_description(intptr_t selfPtr, const char* sdp, int offer);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_ice_candidate(intptr_t selfPtr, const char* candidate, const char* mid, int mlineIndex);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_channel_open(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_channel_close(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_data_message(intptr_t selfPtr, const uint8_t* data, int size);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_error(intptr_t selfPtr, const char* text);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_connection_path(intptr_t selfPtr, int pathCode);
}

void geranes_rtc_open_bridge(int handle, const char* iceServersJsonPtr, int host, intptr_t self)
{
    MAIN_THREAD_EM_ASM((function() {
        const scope = Module.__geranes_rtc_bridge || (Module.__geranes_rtc_bridge = {
            peers: {}
        });

        function callExport(name, args) {
            const fn = Module['_' + name];
            if(typeof fn !== 'function') {
                console.error('[GeraNES][RTC] missing export', name);
                return;
            }
            fn.apply(null, args || []);
        }

        function withUtf8(value, fn) {
            const text = value || "";
            const len = lengthBytesUTF8(text) + 1;
            const ptr = _malloc(len);
            stringToUTF8(text, ptr, len);
            try {
                fn(ptr);
            } finally {
                _free(ptr);
            }
        }

        function callExportString(name, selfPtr, text) {
            withUtf8(text, function(ptr) {
                callExport(name, [selfPtr, ptr]);
            });
        }

        function callExportLocalDescription(selfPtr, sdp, offer) {
            withUtf8(sdp, function(sdpPtr) {
                callExport('geranes_rtc_on_local_description', [selfPtr, sdpPtr, offer ? 1 : 0]);
            });
        }

        function callExportIceCandidate(selfPtr, candidate, mid, mlineIndex) {
            withUtf8(candidate, function(candidatePtr) {
                withUtf8(mid, function(midPtr) {
                    callExport('geranes_rtc_on_ice_candidate', [selfPtr, candidatePtr, midPtr, mlineIndex]);
                });
            });
        }
        function callExportConnectionPath(selfPtr, pathCode) {
            callExport('geranes_rtc_on_connection_path', [selfPtr, pathCode | 0]);
        }

        scope.callExport = scope.callExport || callExport;
        scope.callExportString = scope.callExportString || callExportString;
        scope.callExportLocalDescription = scope.callExportLocalDescription || callExportLocalDescription;
        scope.callExportIceCandidate = scope.callExportIceCandidate || callExportIceCandidate;
        scope.callExportConnectionPath = scope.callExportConnectionPath || callExportConnectionPath;

        function callBytes(selfPtr, data) {
            const view = data instanceof Uint8Array ? data : new Uint8Array(data);
            const ptr = _malloc(view.length);
            HEAPU8.set(view, ptr);
            try {
                callExport('geranes_rtc_on_data_message', [selfPtr, ptr, view.length]);
            } finally {
                _free(ptr);
            }
        }

        scope.describeRtcError = scope.describeRtcError || function(err, pc, dc) {
            try {
                if(!err) {
                    return 'Unknown WebRTC browser error';
                }

                if(typeof err === 'string') {
                    return err;
                }

            const parts = [];
            const baseType = err.type || err.name || typeof err;
            if(baseType) {
                parts.push(String(baseType));
            }

            const detail = err.errorDetail || (err.error && err.error.errorDetail);
            if(detail) {
                parts.push('detail=' + detail);
            }

            const message = err.message || (err.error && err.error.message);
            if(message) {
                parts.push('message=' + message);
            }

            const sctpCauseCode = err.sctpCauseCode || (err.error && err.error.sctpCauseCode);
            if(typeof sctpCauseCode === 'number') {
                parts.push('sctpCauseCode=' + sctpCauseCode);
            }

            const receivedAlert = err.receivedAlert || (err.error && err.error.receivedAlert);
            if(typeof receivedAlert === 'number') {
                parts.push('receivedAlert=' + receivedAlert);
            }

            const sentAlert = err.sentAlert || (err.error && err.error.sentAlert);
            if(typeof sentAlert === 'number') {
                parts.push('sentAlert=' + sentAlert);
            }

            if(pc) {
                parts.push('pc.connectionState=' + pc.connectionState);
                parts.push('pc.iceConnectionState=' + pc.iceConnectionState);
                parts.push('pc.iceGatheringState=' + pc.iceGatheringState);
                parts.push('pc.signalingState=' + pc.signalingState);
            }

            if(dc) {
                parts.push('dc.readyState=' + dc.readyState);
                if(dc.label) {
                    parts.push('dc.label=' + dc.label);
                }
            }

                return parts.join(', ');
            } catch(_) {
                return String(err);
            }
        };

        function reportError(selfPtr, err) {
            const message = scope.describeRtcError(err, null, null);
            console.error('[GeraNES][RTC] error', { self: selfPtr, error: err, message: message });
            callExportString('geranes_rtc_on_error', selfPtr, message);
        }

        if(typeof RTCPeerConnection === 'undefined') {
            reportError($3, 'Browser RTCPeerConnection API is not available');
            return;
        }

        try {
        const handle = $0;
        const iceServersRaw = UTF8ToString($1 || 0);
        const self = $3;
        let iceServers = [];
        function parseIceServer(value) {
            if(typeof value !== 'string' || value.length === 0) {
                return null;
            }

            const trimmed = value.trim();
            if(trimmed.length === 0) {
                return null;
            }

            if(!trimmed.startsWith('turn:') && !trimmed.startsWith('turns:')) {
                return { urls: trimmed };
            }

            const schemeEnd = trimmed.indexOf(':');
            if(schemeEnd <= 0) {
                return { urls: trimmed };
            }

            const scheme = trimmed.substring(0, schemeEnd);
            const remainder = trimmed.substring(schemeEnd + 1);
            const atIndex = remainder.indexOf('@');
            if(atIndex <= 0) {
                return { urls: trimmed };
            }

            const credentials = remainder.substring(0, atIndex);
            const hostPart = remainder.substring(atIndex + 1);
            const credentialSep = credentials.indexOf(':');
            if(credentialSep <= 0) {
                return { urls: trimmed };
            }

            const username = credentials.substring(0, credentialSep);
            const credential = credentials.substring(credentialSep + 1);
            if(username.length === 0 || credential.length === 0 || hostPart.length === 0) {
                return { urls: trimmed };
            }

            return {
                urls: scheme + ':' + hostPart,
                username: username,
                credential: credential
            };
        }
        if(iceServersRaw) {
            try {
                const parsed = JSON.parse(iceServersRaw);
                if(Array.isArray(parsed)) {
                    iceServers = parsed
                        .map(parseIceServer)
                        .filter(function(v) { return !!v; });
                }
            } catch(_) {
            }
        }

        const pc = new RTCPeerConnection({ iceServers: iceServers });
        const state = {
            pc: pc,
            dc: null,
            connectionPathCode: 0
        };
        scope.peers[handle] = state;

        function classifyConnectionPath(stats, pair) {
            if(!pair) return 0;
            const local = pair.localCandidateId ? stats.get(pair.localCandidateId) : null;
            const remote = pair.remoteCandidateId ? stats.get(pair.remoteCandidateId) : null;
            const localType = local && typeof local.candidateType === 'string' ? local.candidateType : '';
            const remoteType = remote && typeof remote.candidateType === 'string' ? remote.candidateType : '';
            if(localType === 'relay' || remoteType === 'relay') {
                return 2; // TurnRelay
            }
            if(localType || remoteType) {
                return 1; // Direct
            }
            return 0; // Unknown
        }

        async function refreshConnectionPath() {
            try {
                const stats = await pc.getStats();
                let selectedPair = null;

                stats.forEach(function(report) {
                    if(selectedPair) return;
                    if(report.type === 'transport' && report.selectedCandidatePairId) {
                        const pair = stats.get(report.selectedCandidatePairId);
                        if(pair) {
                            selectedPair = pair;
                        }
                    }
                });

                if(!selectedPair) {
                    stats.forEach(function(report) {
                        if(selectedPair) return;
                        if(report.type !== 'candidate-pair') return;
                        const selected = report.selected === true || report.nominated === true;
                        if(selected && report.state === 'succeeded') {
                            selectedPair = report;
                        }
                    });
                }

                const pathCode = classifyConnectionPath(stats, selectedPair);
                if(pathCode !== state.connectionPathCode) {
                    state.connectionPathCode = pathCode;
                    scope.callExportConnectionPath(self, pathCode);
                }
            } catch(_) {
            }
        }

        function attachChannel(dc) {
            state.dc = dc;
            dc.binaryType = 'arraybuffer';
            dc.onopen = function() {
                callExport('geranes_rtc_on_channel_open', [self]);
                refreshConnectionPath();
            };
            dc.onclose = function() {
                callExport('geranes_rtc_on_channel_close', [self]);
            };
            dc.onerror = function(err) {
                reportError(self, scope.describeRtcError(err || 'WebRTC data channel error', state.pc, dc));
            };
            dc.onmessage = function(event) {
                if(event.data instanceof ArrayBuffer) {
                    callBytes(self, new Uint8Array(event.data));
                    return;
                }
                if(ArrayBuffer.isView(event.data)) {
                    callBytes(self, new Uint8Array(event.data.buffer, event.data.byteOffset, event.data.byteLength));
                    return;
                }
                if(typeof event.data === 'string') {
                    callBytes(self, new TextEncoder().encode(event.data));
                    return;
                }
                reportError(self, 'Unsupported WebRTC data message type');
            };
        }

        pc.onicecandidate = function(event) {
            if(!event.candidate) {
                return;
            }
            callExportIceCandidate(
                self,
                event.candidate.candidate || "",
                event.candidate.sdpMid || "",
                typeof event.candidate.sdpMLineIndex === 'number' ? event.candidate.sdpMLineIndex : -1
            );
        };
        pc.onconnectionstatechange = function() {
            if(pc.connectionState === 'failed') {
                reportError(self, scope.describeRtcError('WebRTC peer connection failed', pc, state.dc));
            } else if(pc.connectionState === 'disconnected' || pc.connectionState === 'closed') {
                callExport('geranes_rtc_on_channel_close', [self]);
            }
            refreshConnectionPath();
        };
        pc.oniceconnectionstatechange = function() { refreshConnectionPath(); };
        pc.onsignalingstatechange = function() {};
        pc.ondatachannel = function(event) {
            if(event && event.channel) {
                attachChannel(event.channel);
            }
        };

        if($2) {
            attachChannel(pc.createDataChannel('geranes', { ordered: true }));
        }
        refreshConnectionPath();
        } catch(err) {
            try {
                reportError($3, err);
            } catch(_) {
            }
        }
    })(), handle, iceServersJsonPtr, host, self);
}

void geranes_rtc_close_bridge(int handle)
{
    MAIN_THREAD_EM_ASM((function() {
        const scope = Module.__geranes_rtc_bridge;
        const handle = $0;
        if(!scope || !scope.peers[handle]) {
            return;
        }

        const state = scope.peers[handle];
        delete scope.peers[handle];

        try {
            if(state.dc) {
                state.dc.onopen = null;
                state.dc.onclose = null;
                state.dc.onerror = null;
                state.dc.onmessage = null;
                state.dc.close();
            }
        } catch(_) {
        }

        try {
            if(state.pc) {
                state.pc.onicecandidate = null;
                state.pc.onconnectionstatechange = null;
                state.pc.ondatachannel = null;
                state.pc.close();
            }
        } catch(_) {
        }
    })(), handle);
}

int geranes_rtc_create_offer_bridge(int handle, intptr_t self, intptr_t onLocalDescription, intptr_t onError)
{
    (void)onLocalDescription;
    (void)onError;
    return MAIN_THREAD_EM_ASM_INT(return (function() {
        const scope = Module.__geranes_rtc_bridge;
        const handle = $0;
        const selfPtr = $1;
        if(!scope || !scope.peers[handle]) {
            return 0;
        }

        function reportError(err) {
            const message = scope.describeRtcError
                ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
                : String(err);
            console.error('[GeraNES][RTC] createOffer failed', { self: selfPtr, error: err, message: message });
            scope.callExportString('geranes_rtc_on_error', selfPtr, message);
        }

        (async function() {
            try {
                const pc = scope.peers[handle].pc;
                const offer = await pc.createOffer();
                await pc.setLocalDescription(offer);
                scope.callExportLocalDescription(selfPtr, pc.localDescription ? pc.localDescription.sdp : offer.sdp, 1);
            } catch(err) {
                reportError(err);
            }
        })();

        return 1;
    })();, handle, self);
}

int geranes_rtc_create_answer_bridge(int handle, intptr_t self)
{
    return MAIN_THREAD_EM_ASM_INT(return (function() {
        const scope = Module.__geranes_rtc_bridge;
        const handle = $0;
        const selfPtr = $1;
        if(!scope || !scope.peers[handle]) {
            return 0;
        }

        function reportError(err) {
            const message = scope.describeRtcError
                ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
                : String(err);
            console.error('[GeraNES][RTC] createAnswer failed', { self: selfPtr, error: err, message: message });
            scope.callExportString('geranes_rtc_on_error', selfPtr, message);
        }

        (async function() {
            try {
                const pc = scope.peers[handle].pc;
                const answer = await pc.createAnswer();
                await pc.setLocalDescription(answer);
                scope.callExportLocalDescription(selfPtr, pc.localDescription ? pc.localDescription.sdp : answer.sdp, 0);
            } catch(err) {
                reportError(err);
            }
        })();

        return 1;
    })();, handle, self);
}

int geranes_rtc_set_remote_description_bridge(int handle, const char* sdpPtr, int offer, intptr_t self)
{
    return MAIN_THREAD_EM_ASM_INT(return (function() {
        const scope = Module.__geranes_rtc_bridge;
        const handle = $0;
        const selfPtr = $3;
        if(!scope || !scope.peers[handle]) {
            return 0;
        }

        const sdp = UTF8ToString($1);

        function reportError(err) {
            const message = scope.describeRtcError
                ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
                : String(err);
            console.error('[GeraNES][RTC] setRemoteDescription failed', { self: selfPtr, error: err, message: message });
            scope.callExportString('geranes_rtc_on_error', selfPtr, message);
        }

        (async function() {
            try {
                const pc = scope.peers[handle].pc;
                await pc.setRemoteDescription({
                    type: $2 ? 'offer' : 'answer',
                    sdp: sdp
                });
            } catch(err) {
                reportError(err);
            }
        })();

        return 1;
    })();, handle, sdpPtr, offer, self);
}

int geranes_rtc_add_ice_candidate_bridge(int handle,
                                         const char* candidatePtr,
                                         const char* midPtr,
                                         int mlineIndex,
                                         intptr_t self)
{
    return MAIN_THREAD_EM_ASM_INT(return (function() {
        const scope = Module.__geranes_rtc_bridge;
        const handle = $0;
        const selfPtr = $4;
        if(!scope || !scope.peers[handle]) {
            return 0;
        }

        const candidate = UTF8ToString($1);
        const mid = UTF8ToString($2);

        function reportError(err) {
            const message = scope.describeRtcError
                ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
                : String(err);
            console.error('[GeraNES][RTC] addIceCandidate failed', { self: selfPtr, error: err, message: message });
            scope.callExportString('geranes_rtc_on_error', selfPtr, message);
        }

        (async function() {
            try {
                const pc = scope.peers[handle].pc;
                await pc.addIceCandidate({
                    candidate: candidate,
                    sdpMid: mid || null,
                    sdpMLineIndex: $3 >= 0 ? $3 : null
                });
            } catch(err) {
                reportError(err);
            }
        })();

        return 1;
    })();, handle, candidatePtr, midPtr, mlineIndex, self);
}

int geranes_rtc_send_bridge(int handle, const uint8_t* payloadPtr, int payloadSize)
{
    return MAIN_THREAD_EM_ASM_INT(return (function() {
        const scope = Module.__geranes_rtc_bridge;
        const handle = $0;
        if(!scope || !scope.peers[handle]) {
            return 0;
        }
        const state = scope.peers[handle];
        if(!state.dc || state.dc.readyState !== 'open') {
            return 0;
        }
        try {
            const view = HEAPU8.slice($1, $1 + $2);
            state.dc.send(view);
            return 1;
        } catch(_) {
            return 0;
        }
    })();, handle, payloadPtr, payloadSize);
}

int geranes_rtc_buffered_amount_bridge(int handle)
{
    return MAIN_THREAD_EM_ASM_INT(return (function() {
        const scope = Module.__geranes_rtc_bridge;
        const handle = $0;
        if(!scope || !scope.peers[handle]) {
            return 0;
        }
        const state = scope.peers[handle];
        if(!state.dc || state.dc.readyState !== 'open') {
            return 0;
        }
        return state.dc.bufferedAmount >>> 0;
    })();, handle);
}

class WebEmscriptenWebRtcPeerConnection final : public IWebRtcPeerConnection
{
private:
    int m_handle = 0;
    bool m_open = false;
    bool m_dataChannelOpen = false;
    ConnectionPath m_connectionPath = ConnectionPath::Unknown;
    std::string m_lastError;
    std::deque<Event> m_events;
    mutable std::mutex m_mutex;

    void pushEvent(Event&& event)
    {
        std::scoped_lock lock(m_mutex);
        m_events.push_back(std::move(event));
    }

    void setLastError(std::string error)
    {
        std::scoped_lock lock(m_mutex);
        m_lastError = std::move(error);
    }

public:
    void onLocalDescription(const char* sdp, int offer)
    {
        Event event;
        event.type = Event::Type::LocalDescriptionReady;
        event.sdp = sdp != nullptr ? sdp : "";
        event.descriptionIsOffer = offer != 0;
        pushEvent(std::move(event));
    }

    void onIceCandidate(const char* candidate, const char* mid, int mlineIndex)
    {
        Event event;
        event.type = Event::Type::IceCandidateReady;
        event.candidate = candidate != nullptr ? candidate : "";
        event.mid = mid != nullptr ? mid : "";
        event.mlineIndex = mlineIndex;
        pushEvent(std::move(event));
    }

    void onChannelOpen()
    {
        m_dataChannelOpen = true;
        Event event;
        event.type = Event::Type::DataChannelOpen;
        pushEvent(std::move(event));
    }

    void onChannelClose()
    {
        m_dataChannelOpen = false;
        Event event;
        event.type = Event::Type::DataChannelClosed;
        pushEvent(std::move(event));
    }

    void onDataMessage(const uint8_t* data, int size)
    {
        Event event;
        event.type = Event::Type::DataMessage;
        if(data != nullptr && size > 0) {
            event.payload.assign(data, data + size);
        }
        pushEvent(std::move(event));
    }

    void onError(const char* text)
    {
        const std::string error = text != nullptr ? text : "WebRTC browser bridge error";
        setLastError(error);
        pushEvent(Event{Event::Type::Error, {}, false, {}, {}, -1, {}, error});
    }

    void onConnectionPath(int pathCode)
    {
        ConnectionPath path = ConnectionPath::Unknown;
        if(pathCode == 1) {
            path = ConnectionPath::Direct;
        } else if(pathCode == 2) {
            path = ConnectionPath::TurnRelay;
        }
        std::scoped_lock lock(m_mutex);
        m_connectionPath = path;
    }

    ~WebEmscriptenWebRtcPeerConnection() override
    {
        close();
    }

    bool open(const WebRtcPeerConnectionOptions& options) override
    {
        close();

        nlohmann::json iceServers = nlohmann::json::array();
        for(const auto& iceServer : options.iceServers) {
            if(!iceServer.empty()) {
                iceServers.push_back(iceServer);
            }
        }

        static std::atomic<int> nextPeerHandle{1};
        m_handle = nextPeerHandle.fetch_add(1, std::memory_order_relaxed);
        if(m_handle == 0) {
            m_handle = nextPeerHandle.fetch_add(1, std::memory_order_relaxed);
        }

        geranes_rtc_open_bridge(
            m_handle,
            iceServers.dump().c_str(),
            options.host ? 1 : 0,
            reinterpret_cast<intptr_t>(this));

        m_open = true;
        m_dataChannelOpen = false;
        m_connectionPath = ConnectionPath::Unknown;
        m_lastError.clear();
        return true;
    }

    void close() override
    {
        if(m_handle != 0) {
            geranes_rtc_close_bridge(m_handle);
            m_handle = 0;
        }
        m_open = false;
        m_dataChannelOpen = false;
        m_connectionPath = ConnectionPath::Unknown;

        std::scoped_lock lock(m_mutex);
        m_events.clear();
    }

    bool createOffer() override
    {
        if(!m_open || m_handle == 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(!geranes_rtc_create_offer_bridge(
                m_handle,
                reinterpret_cast<intptr_t>(this),
                0,
                0)) {
            m_lastError = "Failed to create WebRTC offer";
            return false;
        }
        return true;
    }

    bool createAnswer() override
    {
        if(!m_open || m_handle == 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(!geranes_rtc_create_answer_bridge(m_handle, reinterpret_cast<intptr_t>(this))) {
            m_lastError = "Failed to create WebRTC answer";
            return false;
        }
        return true;
    }

    bool setRemoteDescription(const std::string& sdp, bool offer) override
    {
        if(!m_open || m_handle == 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(!geranes_rtc_set_remote_description_bridge(
                m_handle,
                sdp.c_str(),
                offer ? 1 : 0,
                reinterpret_cast<intptr_t>(this))) {
            m_lastError = "Failed to set remote WebRTC description";
            return false;
        }
        return true;
    }

    bool addRemoteIceCandidate(const std::string& candidate,
                               const std::string& mid,
                               int mlineIndex) override
    {
        if(!m_open || m_handle == 0) {
            m_lastError = "WebRTC peer connection is not open";
            return false;
        }
        if(!geranes_rtc_add_ice_candidate_bridge(
                m_handle,
                candidate.c_str(),
                mid.c_str(),
                mlineIndex,
                reinterpret_cast<intptr_t>(this))) {
            m_lastError = "Failed to add remote WebRTC ICE candidate";
            return false;
        }
        return true;
    }

    bool sendDataReliable(const std::vector<uint8_t>& payload) override
    {
        if(!m_dataChannelOpen || m_handle == 0) {
            m_lastError = "WebRTC data channel is not open";
            return false;
        }
        if(!geranes_rtc_send_bridge(m_handle, payload.data(), static_cast<int>(payload.size()))) {
            m_lastError = "Failed to send WebRTC data channel payload";
            return false;
        }
        return true;
    }

    bool sendDataUnreliable(const std::vector<uint8_t>& payload) override
    {
        return sendDataReliable(payload);
    }

    size_t bufferedAmount() const override
    {
        if(m_handle == 0 || !m_dataChannelOpen) {
            return 0u;
        }
        return static_cast<size_t>(geranes_rtc_buffered_amount_bridge(m_handle));
    }

    ConnectionPath connectionPath() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_connectionPath;
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

    std::string lastError() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_lastError;
    }
};

extern "C" {
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_local_description(intptr_t selfPtr, const char* sdp, int offer)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcPeerConnection*>(selfPtr);
    if(self != nullptr) self->onLocalDescription(sdp, offer);
}

EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_ice_candidate(intptr_t selfPtr, const char* candidate, const char* mid, int mlineIndex)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcPeerConnection*>(selfPtr);
    if(self != nullptr) self->onIceCandidate(candidate, mid, mlineIndex);
}

EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_channel_open(intptr_t selfPtr)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcPeerConnection*>(selfPtr);
    if(self != nullptr) self->onChannelOpen();
}

EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_channel_close(intptr_t selfPtr)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcPeerConnection*>(selfPtr);
    if(self != nullptr) self->onChannelClose();
}

EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_data_message(intptr_t selfPtr, const uint8_t* data, int size)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcPeerConnection*>(selfPtr);
    if(self != nullptr) self->onDataMessage(data, size);
}

EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_error(intptr_t selfPtr, const char* text)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcPeerConnection*>(selfPtr);
    if(self != nullptr) self->onError(text);
}

EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_connection_path(intptr_t selfPtr, int pathCode)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcPeerConnection*>(selfPtr);
    if(self != nullptr) self->onConnectionPath(pathCode);
}
}
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

    size_t bufferedAmount() const override
    {
        return 0u;
    }

    ConnectionPath connectionPath() const override
    {
        return ConnectionPath::Unknown;
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

    std::string lastError() const override
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
    return std::make_unique<WebEmscriptenWebRtcPeerConnection>();
#endif
}

} // namespace Netplay
