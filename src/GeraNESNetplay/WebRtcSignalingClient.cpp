#include "GeraNESNetplay/WebRtcSignalingClient.h"

#include "logger/logger.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

#if !defined(__EMSCRIPTEN__)
#include <exception>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#else
#include <emscripten.h>
#endif

namespace Netplay {

namespace {

#if !defined(__EMSCRIPTEN__)
constexpr auto kDesktopSignalingConnectTimeout = std::chrono::milliseconds(1500);
#endif

#if !defined(__EMSCRIPTEN__)
class DesktopWebRtcSignalingClient final : public IWebRtcSignalingClient
{
private:
    using Client = websocketpp::client<websocketpp::config::asio_client>;

    std::unique_ptr<Client> m_client;
    websocketpp::connection_hdl m_connection;
    bool m_hasConnection = false;
    bool m_connected = false;
    bool m_connectResolved = false;
    bool m_connectSucceeded = false;
    std::string m_connectError;
    std::string m_lastError;
    std::deque<Event> m_events;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_connectCondition;

    static bool startsWith(const std::string& value, const char* prefix)
    {
        return value.rfind(prefix, 0) == 0;
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

    void reportRuntimeError(const std::string& error)
    {
        {
            std::scoped_lock lock(m_mutex);
            m_hasConnection = false;
            m_connected = false;
        }
        pushEvent(Event{Event::Type::Error, {}, error});
        markConnectResolved(false, error);
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

        if(!startsWith(options.config.url, "ws://")) {
            m_lastError = "WebRTC signaling currently supports only ws:// URLs on desktop";
            return false;
        }

        m_client = std::make_unique<Client>();
        m_client->clear_access_channels(websocketpp::log::alevel::all);
        m_client->clear_error_channels(websocketpp::log::elevel::all);
        m_client->init_asio();
        m_client->start_perpetual();

        {
            std::scoped_lock lock(m_mutex);
            m_connectResolved = false;
            m_connectSucceeded = false;
            m_connectError.clear();
            m_hasConnection = false;
            m_connected = false;
            m_lastError.clear();
            m_events.clear();
        }

        m_client->set_open_handler([this](websocketpp::connection_hdl hdl) {
            {
                std::scoped_lock lock(m_mutex);
                m_connection = hdl;
                m_hasConnection = true;
                m_connected = true;
            }
            pushEvent(Event{Event::Type::Connected, {}, {}});
            markConnectResolved(true);
        });

        m_client->set_close_handler([this](websocketpp::connection_hdl) {
            {
                std::scoped_lock lock(m_mutex);
                m_hasConnection = false;
                m_connected = false;
            }
            pushEvent(Event{Event::Type::Disconnected, {}, {}});
        });

        m_client->set_fail_handler([this](websocketpp::connection_hdl hdl) {
            std::string error = "WebRTC signaling connection failed";
            if(m_client) {
                Client::connection_ptr connection = m_client->get_con_from_hdl(hdl);
                if(connection) {
                    const auto ec = connection->get_ec();
                    if(ec) {
                        error = "WebRTC signaling connection failed: " + ec.message();
                    }
                }
            }

            {
                std::scoped_lock lock(m_mutex);
                m_hasConnection = false;
                m_connected = false;
            }
            pushEvent(Event{Event::Type::Error, {}, error});
            markConnectResolved(false, error);
        });

        m_client->set_message_handler([this](websocketpp::connection_hdl, Client::message_ptr message) {
            Event event;
            event.type = Event::Type::Message;
            event.text = message->get_payload();
            const auto parsed = WebRtcSignalingMessage::fromText(event.text);
            if(parsed.has_value()) {
                event.message = *parsed;
            }
            pushEvent(std::move(event));
        });

        websocketpp::lib::error_code ec;
        Client::connection_ptr connection;
        try {
            connection = m_client->get_connection(options.config.url, ec);
        } catch(const std::exception& ex) {
            m_client->stop_perpetual();
            m_client.reset();
            m_lastError = std::string("WebRTC signaling setup failed: ") + ex.what();
            return false;
        } catch(...) {
            m_client->stop_perpetual();
            m_client.reset();
            m_lastError = "WebRTC signaling setup failed";
            return false;
        }
        if(ec) {
            m_client->stop_perpetual();
            m_client.reset();
            m_lastError = "WebRTC signaling URL is invalid: " + ec.message();
            return false;
        }

        try {
            m_client->connect(connection);
        } catch(const std::exception& ex) {
            m_lastError = std::string("WebRTC signaling connect failed: ") + ex.what();
            disconnect();
            return false;
        } catch(...) {
            m_lastError = "WebRTC signaling connect failed";
            disconnect();
            return false;
        }
        connection.reset();

        m_thread = std::thread([this]() {
            try {
                if(m_client) {
                    m_client->run();
                }
            } catch(const std::exception& ex) {
                reportRuntimeError(std::string("WebRTC signaling runtime failed: ") + ex.what());
            } catch(...) {
                reportRuntimeError("WebRTC signaling runtime failed");
            }
        });

        std::unique_lock lock(m_mutex);
        const bool resolved = m_connectCondition.wait_for(
            lock,
            kDesktopSignalingConnectTimeout,
            [this]() { return m_connectResolved; });

        if(!resolved) {
            lock.unlock();
            m_lastError = "WebRTC signaling connection timed out";
            disconnect();
            return false;
        }

        const bool connectSucceeded = m_connectSucceeded;
        const std::string connectError = m_connectError;
        lock.unlock();

        if(!connectSucceeded) {
            m_lastError = connectError.empty() ? "WebRTC signaling connection failed" : connectError;
            disconnect();
            return false;
        }

        return true;
    }

    void disconnect() override
    {
        bool hasConnection = false;
        websocketpp::connection_hdl connection;
        {
            std::scoped_lock lock(m_mutex);
            hasConnection = m_hasConnection;
            connection = m_connection;
        }

        if(m_client && hasConnection) {
            websocketpp::lib::error_code ec;
            try {
                m_client->close(connection, websocketpp::close::status::going_away, "", ec);
            } catch(...) {
            }
        }

        if(m_client) {
            try {
                m_client->stop_perpetual();
                m_client->stop();
            } catch(...) {
            }
        }

        if(m_thread.joinable()) {
            m_thread.join();
        }

        {
            std::scoped_lock lock(m_mutex);
            m_hasConnection = false;
            m_connected = false;
            m_connectResolved = false;
            m_connectSucceeded = false;
            m_connectError.clear();
            m_events.clear();
        }

        m_client.reset();
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        websocketpp::connection_hdl connection;
        bool connected = false;
        bool hasConnection = false;
        {
            std::scoped_lock lock(m_mutex);
            connection = m_connection;
            connected = m_connected;
            hasConnection = m_hasConnection;
        }

        if(!connected || !m_client || !hasConnection) {
            m_lastError = "WebRTC signaling socket is not connected";
            return false;
        }

        websocketpp::lib::error_code ec;
        try {
            m_client->send(connection, message.toText(), websocketpp::frame::opcode::text, ec);
        } catch(const std::exception& ex) {
            m_lastError = std::string("WebRTC signaling send failed: ") + ex.what();
            return false;
        } catch(...) {
            m_lastError = "WebRTC signaling send failed";
            return false;
        }
        if(ec) {
            m_lastError = "WebRTC signaling send failed: " + ec.message();
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

    const std::string& lastError() const override
    {
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
        setConnected(true);
        pushEvent(Event{Event::Type::Connected, {}, {}});
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
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        if(m_socketHandle == 0 || !isConnected()) {
            m_lastError = "WebRTC signaling socket is not connected";
            return false;
        }
        if(!geranes_ws_send_bridge(m_socketHandle, message.toText().c_str())) {
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

    const std::string& lastError() const override
    {
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

    const std::string& lastError() const override
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
