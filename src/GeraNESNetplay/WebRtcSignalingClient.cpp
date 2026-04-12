#include "GeraNESNetplay/WebRtcSignalingClient.h"

#include "logger/logger.h"

#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

#if !defined(__EMSCRIPTEN__)
#include <rtc/websocket.hpp>
#else
#include <emscripten.h>
#endif

namespace Netplay {

namespace {

#if !defined(__EMSCRIPTEN__)
constexpr auto kDesktopSignalingConnectTimeout = std::chrono::milliseconds(10000);
constexpr auto kDesktopSignalingGracefulCloseTimeout = std::chrono::milliseconds(500);

class SignalingCleanupQueue
{
public:
    static SignalingCleanupQueue& instance()
    {
        static auto* queue = new SignalingCleanupQueue();
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

    SignalingCleanupQueue()
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

template<typename Fn>
void enqueueSignalingCleanup(Fn&& task)
{
    SignalingCleanupQueue::instance().enqueue(std::forward<Fn>(task));
}

class DesktopWebRtcSignalingClient final : public IWebRtcSignalingClient
{
private:
    struct CallbackContext
    {
        std::atomic<bool> alive{true};
        DesktopWebRtcSignalingClient* owner = nullptr;
    };

    std::shared_ptr<rtc::WebSocket> m_socket;
    bool m_connected = false;
    bool m_connectResolved = false;
    bool m_connectSucceeded = false;
    std::string m_connectError;
    std::string m_lastError;
    std::deque<Event> m_events;
    std::shared_ptr<CallbackContext> m_callbackContext;
    mutable std::mutex m_mutex;
    std::condition_variable m_connectCondition;

    static DesktopWebRtcSignalingClient* ownerFromContext(const std::shared_ptr<CallbackContext>& context)
    {
        if(!context || !context->alive.load(std::memory_order_acquire)) {
            return nullptr;
        }
        return context->owner;
    }

    static bool startsWith(const std::string& value, const char* prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    void logTrace(const std::string& message) const
    {
        Logger::instance().log("[WebRTC signaling] " + message, Logger::Type::INFO);
    }

    void pushEvent(Event&& event)
    {
        std::scoped_lock lock(m_mutex);
        m_events.push_back(std::move(event));
    }

    void markConnectResolved(bool success, const std::string& error = {})
    {
        {
            std::scoped_lock lock(m_mutex);
            m_connectResolved = true;
            m_connectSucceeded = success;
            m_connectError = error;
        }
        m_connectCondition.notify_all();
    }

public:
    ~DesktopWebRtcSignalingClient() override
    {
        disconnect();
    }

    bool connect(const WebRtcSignalingClientOptions& options) override
    {
        disconnect();

        if(!options.config.valid()) {
            m_lastError = "Configure signaling URL and room id for WebRTC";
            return false;
        }

        if(!startsWith(options.config.url, "ws://") &&
           !startsWith(options.config.url, "wss://")) {
            m_lastError = "WebRTC signaling desktop client requires ws:// or wss:// URLs";
            return false;
        }

        rtc::WebSocket::Configuration config;
        config.connectionTimeout = kDesktopSignalingConnectTimeout;
        auto socket = std::make_shared<rtc::WebSocket>(config);
        logTrace("connecting to " + options.config.url);

        {
            std::scoped_lock lock(m_mutex);
            m_socket = socket;
            m_connected = false;
            m_connectResolved = false;
            m_connectSucceeded = false;
            m_connectError.clear();
            m_lastError.clear();
            m_events.clear();
            m_callbackContext = std::make_shared<CallbackContext>();
            m_callbackContext->owner = this;
        }

        const std::shared_ptr<CallbackContext> callbackContext = m_callbackContext;

        socket->onOpen([callbackContext]() {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            bool firstOpen = false;
            {
                std::scoped_lock lock(self->m_mutex);
                firstOpen = !self->m_connected;
                self->m_connected = true;
            }
            if(firstOpen) {
                self->logTrace("socket opened");
                self->pushEvent(Event{Event::Type::Connected, {}, {}});
            }
            self->markConnectResolved(true);
        });

        socket->onClosed([callbackContext]() {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            {
                std::scoped_lock lock(self->m_mutex);
                self->m_connected = false;
            }
            self->logTrace("socket closed");
            self->pushEvent(Event{Event::Type::Disconnected, {}, {}});
        });

        socket->onError([callbackContext](std::string error) {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            std::string copiedError = std::move(error);
            {
                std::scoped_lock lock(self->m_mutex);
                self->m_connected = false;
                self->m_lastError = copiedError;
            }
            self->logTrace("socket error: " + copiedError);
            self->pushEvent(Event{Event::Type::Error, {}, copiedError});
            self->markConnectResolved(false, copiedError);
        });

        socket->onMessage([callbackContext](rtc::message_variant data) {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }
            const std::string* text = std::get_if<std::string>(&data);
            if(text == nullptr) {
                return;
            }

            Event event;
            event.type = Event::Type::Message;
            event.text = *text;
            if(const auto parsed = WebRtcSignalingMessage::fromText(event.text); parsed.has_value()) {
                event.message = *parsed;
                std::string messageLog =
                    std::string("socket message type=") +
                    webRtcSignalTypeLabel(event.message.type);
                if(!event.message.peerId.empty()) {
                    messageLog += " peerId=" + event.message.peerId;
                }
                if(!event.message.targetPeerId.empty()) {
                    messageLog += " targetPeerId=" + event.message.targetPeerId;
                }
                if(!event.message.roomId.empty()) {
                    messageLog += " roomId=" + event.message.roomId;
                }
                self->logTrace(messageLog);
            } else {
                self->logTrace("socket message received with unparsed payload");
            }
            self->pushEvent(std::move(event));
        });

        try {
            socket->open(options.config.url);
        } catch(const std::exception& ex) {
            m_lastError = std::string("WebRTC signaling connect failed: ") + ex.what();
            disconnect();
            return false;
        } catch(...) {
            m_lastError = "WebRTC signaling connect failed";
            disconnect();
            return false;
        }

        std::unique_lock lock(m_mutex);
        m_connectCondition.wait_for(
            lock,
            kDesktopSignalingConnectTimeout,
            [this]() { return m_connectResolved; }
        );
        if(!m_connectResolved) {
            m_lastError = "WebRTC signaling connect timed out";
            lock.unlock();
            disconnect();
            return false;
        }
        if(!m_connectSucceeded) {
            m_lastError = !m_connectError.empty() ? m_connectError : "WebRTC signaling connect failed";
            lock.unlock();
            disconnect();
            return false;
        }

        return true;
    }

    void disconnect() override
    {
        std::shared_ptr<rtc::WebSocket> socket;
        std::shared_ptr<CallbackContext> callbackContext;
        {
            std::scoped_lock lock(m_mutex);
            socket = std::move(m_socket);
            callbackContext = std::move(m_callbackContext);
            m_connected = false;
            m_connectResolved = false;
            m_connectSucceeded = false;
            m_connectError.clear();
            m_events.clear();
        }

        if(callbackContext) {
            callbackContext->alive.store(false, std::memory_order_release);
            callbackContext->owner = nullptr;
        }

        if(socket) {
            logTrace("disconnecting socket");
            enqueueSignalingCleanup([socket = std::move(socket)]() mutable {
                try {
                    socket->close();
                    std::this_thread::sleep_for(kDesktopSignalingGracefulCloseTimeout);
                } catch(...) {
                }
                try {
                    socket->resetCallbacks();
                    socket->forceClose();
                } catch(...) {
                }
                socket.reset();
            });
        }
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        std::shared_ptr<rtc::WebSocket> socket;
        bool connected = false;
        {
            std::scoped_lock lock(m_mutex);
            socket = m_socket;
            connected = m_connected;
        }

        if(!socket || !connected) {
            m_lastError = "WebRTC signaling socket is not connected";
            return false;
        }

        if(!socket->send(message.toText())) {
            m_lastError = "WebRTC signaling send failed";
            return false;
        }
        return true;
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

    bool isConnected() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_connected;
    }

    std::string lastError() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_lastError;
    }
};
#endif

#if defined(__EMSCRIPTEN__)
class WebEmscriptenWebRtcSignalingClient;

extern "C" {
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_open(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_close(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_error(intptr_t selfPtr, const char* text);
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_message(intptr_t selfPtr, const char* text);
}

EM_JS(int, geranes_ws_connect_bridge, (const char* urlPtr,
                                       intptr_t self), {
    if(typeof WebSocket === 'undefined') {
        return 0;
    }

    const scope = Module.__geranes_ws_bridge || (Module.__geranes_ws_bridge = {
        nextHandle: 1,
        sockets: {}
    });

    function callVoid(cb, selfPtr) {
        {{{ makeDynCall('vi') }}}(cb, selfPtr);
    }

    function callString(cb, selfPtr, text) {
        const value = text || "";
        const len = lengthBytesUTF8(value) + 1;
        const ptr = _malloc(len);
        stringToUTF8(value, ptr, len);
        {{{ makeDynCall('vii') }}}(cb, selfPtr, ptr);
        _free(ptr);
    }

    try {
        const url = UTF8ToString(urlPtr);
        const handle = scope.nextHandle++;
        const ws = new WebSocket(url);
        ws.binaryType = 'arraybuffer';
        scope.sockets[handle] = ws;

        ws.onopen = function() {
            Module.ccall('geranes_ws_on_open', null, ['number'], [self]);
        };
        ws.onclose = function(event) {
            Module.ccall('geranes_ws_on_close', null, ['number'], [self]);
        };
        ws.onerror = function(event) {
            console.error('[GeraNES][WS] error', {
                handle: handle,
                url: url,
                readyState: ws.readyState,
                event: event
            });
            Module.ccall('geranes_ws_on_error', null, ['number', 'string'], [self, 'WebRTC signaling WebSocket error']);
        };
        ws.onmessage = function(event) {
            const text = (typeof event.data === 'string')
                ? event.data
                : "";
            Module.ccall('geranes_ws_on_message', null, ['number', 'string'], [self, text]);
        };

        return handle;
    } catch(err) {
        console.error('[GeraNES][WS] constructor/setup failed', err);
        try {
            Module.ccall('geranes_ws_on_error', null, ['number', 'string'], [self, err && err.message ? err.message : 'WebRTC signaling WebSocket setup failed']);
        } catch(_) {
        }
        return 0;
    }
});

EM_JS(void, geranes_ws_close_bridge, (int handle), {
    const scope = Module.__geranes_ws_bridge;
    if(!scope || !scope.sockets[handle]) {
        return;
    }
    const ws = scope.sockets[handle];
    delete scope.sockets[handle];
    try {
        ws.onopen = null;
        ws.onclose = null;
        ws.onerror = null;
        ws.onmessage = null;
        ws.close();
    } catch(_) {
    }
});

EM_JS(int, geranes_ws_send_bridge, (int handle, const char* textPtr), {
    const scope = Module.__geranes_ws_bridge;
    if(!scope || !scope.sockets[handle]) {
        return 0;
    }
    const ws = scope.sockets[handle];
    if(ws.readyState !== WebSocket.OPEN) {
        return 0;
    }
    try {
        const text = UTF8ToString(textPtr);
        ws.send(text);
        return 1;
    } catch(_) {
        return 0;
    }
});

class WebEmscriptenWebRtcSignalingClient final : public IWebRtcSignalingClient
{
private:
    int m_socketHandle = 0;
    bool m_connected = false;
    std::string m_lastError;
    std::deque<Event> m_events;
    std::deque<std::string> m_pendingMessages;
    mutable std::mutex m_mutex;

    void pushEvent(Event&& event)
    {
        std::scoped_lock lock(m_mutex);
        m_events.push_back(std::move(event));
    }

    void setConnected(bool connected)
    {
        std::scoped_lock lock(m_mutex);
        m_connected = connected;
    }

    void setLastError(std::string error)
    {
        std::scoped_lock lock(m_mutex);
        m_lastError = std::move(error);
    }

public:
    void onOpen()
    {
        std::deque<std::string> pendingMessages;
        {
            std::scoped_lock lock(m_mutex);
            m_connected = true;
            pendingMessages = m_pendingMessages;
            m_pendingMessages.clear();
        }
        pushEvent(Event{Event::Type::Connected, {}, {}});

        for(const std::string& pending : pendingMessages) {
            if(!geranes_ws_send_bridge(m_socketHandle, pending.c_str())) {
                setLastError("WebRTC signaling send failed");
                pushEvent(Event{Event::Type::Error, {}, "WebRTC signaling send failed"});
                break;
            }
        }
    }

    void onClose()
    {
        Logger::instance().log("WebRTC signaling WebSocket closed", Logger::Type::WARNING);
        setConnected(false);
        pushEvent(Event{Event::Type::Disconnected, {}, {}});
    }

    void onError(const char* text)
    {
        const std::string error = text != nullptr ? text : "WebRTC signaling WebSocket error";
        Logger::instance().log(error, Logger::Type::ERROR);
        setConnected(false);
        setLastError(error);
        pushEvent(Event{Event::Type::Error, {}, error});
    }

    void onMessage(const char* text)
    {
        Event event;
        event.type = Event::Type::Message;
        event.text = text != nullptr ? text : "";
        if(const auto parsed = WebRtcSignalingMessage::fromText(event.text); parsed.has_value()) {
            event.message = *parsed;
        }
        pushEvent(std::move(event));
    }
    ~WebEmscriptenWebRtcSignalingClient() override
    {
        disconnect();
    }

    bool connect(const WebRtcSignalingClientOptions& options) override
    {
        disconnect();

        if(!options.config.valid()) {
            m_lastError = "Configure signaling URL and room id for WebRTC";
            return false;
        }

        {
            std::scoped_lock lock(m_mutex);
            m_events.clear();
            m_lastError.clear();
            m_pendingMessages.clear();
        }

        m_socketHandle = geranes_ws_connect_bridge(
            options.config.url.c_str(),
            reinterpret_cast<intptr_t>(this));

        if(m_socketHandle == 0) {
            m_lastError = "Failed to create browser WebSocket for WebRTC signaling";
            return false;
        }

        return true;
    }

    void disconnect() override
    {
        if(m_socketHandle != 0) {
            geranes_ws_close_bridge(m_socketHandle);
            m_socketHandle = 0;
        }

        std::scoped_lock lock(m_mutex);
        m_connected = false;
        m_events.clear();
        m_pendingMessages.clear();
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        if(m_socketHandle == 0) {
            m_lastError = "WebRTC signaling socket is not connected";
            return false;
        }

        const std::string payload = message.toText();

        {
            std::scoped_lock lock(m_mutex);
            if(!m_connected) {
                m_pendingMessages.push_back(payload);
                return true;
            }
        }

        if(!geranes_ws_send_bridge(m_socketHandle, payload.c_str())) {
            m_lastError = "WebRTC signaling send failed";
            return false;
        }
        return true;
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

    bool isConnected() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_connected;
    }

    std::string lastError() const override
    {
        std::scoped_lock lock(m_mutex);
        return m_lastError;
    }
};

extern "C" {
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_open(intptr_t selfPtr)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcSignalingClient*>(selfPtr);
    if(self != nullptr) self->onOpen();
}

EMSCRIPTEN_KEEPALIVE void geranes_ws_on_close(intptr_t selfPtr)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcSignalingClient*>(selfPtr);
    if(self != nullptr) self->onClose();
}

EMSCRIPTEN_KEEPALIVE void geranes_ws_on_error(intptr_t selfPtr, const char* text)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcSignalingClient*>(selfPtr);
    if(self != nullptr) self->onError(text);
}

EMSCRIPTEN_KEEPALIVE void geranes_ws_on_message(intptr_t selfPtr, const char* text)
{
    auto* self = reinterpret_cast<WebEmscriptenWebRtcSignalingClient*>(selfPtr);
    if(self != nullptr) self->onMessage(text);
}
}
#endif

class StubWebRtcSignalingClient final : public IWebRtcSignalingClient
{
private:
    bool m_connected = false;
    std::string m_lastError;

public:
    bool connect(const WebRtcSignalingClientOptions& options) override
    {
        (void)options;
#if defined(__EMSCRIPTEN__)
        m_lastError = "WebRTC signaling WebSocket bridge is not implemented yet in the web build";
#else
        m_lastError = "WebRTC signaling WebSocket client is not implemented yet on desktop";
#endif
        m_connected = false;
        return false;
    }

    void disconnect() override
    {
        m_connected = false;
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        (void)message;
        if(!m_connected) {
            m_lastError = "WebRTC signaling socket is not connected";
            return false;
        }
        return false;
    }

    std::vector<Event> poll() override
    {
        return {};
    }

    bool isConnected() const override
    {
        return m_connected;
    }

    std::string lastError() const override
    {
        return m_lastError;
    }
};

} // namespace

std::unique_ptr<IWebRtcSignalingClient> createWebRtcSignalingClient()
{
#if !defined(__EMSCRIPTEN__)
    return std::make_unique<DesktopWebRtcSignalingClient>();
#else
    return std::make_unique<WebEmscriptenWebRtcSignalingClient>();
#endif
}

} // namespace Netplay
