#include "GeraNESNetplay/WebRtcSignalingClient.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#if !defined(__EMSCRIPTEN__)
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#endif

namespace Netplay {

namespace {

constexpr auto kDesktopSignalingConnectTimeout = std::chrono::milliseconds(1500);

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
            if(!error.empty()) {
                m_lastError = error;
            }
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
            m_connected = false;
            m_lastError.clear();
            m_events.clear();
        }

        m_client->set_open_handler([this](websocketpp::connection_hdl hdl) {
            m_connection = hdl;
            m_hasConnection = true;
            {
                std::scoped_lock lock(m_mutex);
                m_connected = true;
            }
            pushEvent(Event{Event::Type::Connected, {}, {}});
            markConnectResolved(true);
        });

        m_client->set_close_handler([this](websocketpp::connection_hdl) {
            {
                std::scoped_lock lock(m_mutex);
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
                m_connected = false;
                m_lastError = error;
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
        Client::connection_ptr connection = m_client->get_connection(options.config.url, ec);
        if(ec) {
            m_client->stop_perpetual();
            m_client.reset();
            m_lastError = "WebRTC signaling URL is invalid: " + ec.message();
            return false;
        }

        m_thread = std::thread([this]() {
            if(m_client) {
                m_client->run();
            }
        });

        m_client->connect(connection);

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

        return m_connectSucceeded;
    }

    void disconnect() override
    {
        if(m_client && m_hasConnection) {
            websocketpp::lib::error_code ec;
            m_client->close(m_connection, websocketpp::close::status::going_away, "", ec);
        }

        if(m_client) {
            m_client->stop_perpetual();
            m_client->stop();
        }

        if(m_thread.joinable()) {
            m_thread.join();
        }

        {
            std::scoped_lock lock(m_mutex);
            m_connected = false;
            m_connectResolved = false;
            m_connectSucceeded = false;
            m_events.clear();
        }

        m_hasConnection = false;
        m_client.reset();
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        if(!isConnected() || !m_client || !m_hasConnection) {
            m_lastError = "WebRTC signaling socket is not connected";
            return false;
        }

        websocketpp::lib::error_code ec;
        m_client->send(m_connection, message.toText(), websocketpp::frame::opcode::text, ec);
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
    return std::make_unique<StubWebRtcSignalingClient>();
#endif
}

} // namespace Netplay
