#include "ConsoleNetplay/WebRtcSignalingClient.h"

#include "ConsoleNetplay/NetplayLog.h"

#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#if !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#else
#if defined(__ANDROID__)
#include <SDL_system.h>
#include <jni.h>
#endif
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#endif
#else
#include <emscripten.h>
#endif

namespace ConsoleNetplay {

namespace {

#if !defined(__EMSCRIPTEN__)
constexpr auto kDesktopSignalingConnectTimeout = std::chrono::milliseconds(10000);
constexpr auto kDesktopSignalingReceiveBufferSize = 16 * 1024u;

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

#if defined(__ANDROID__)
std::string fromJString(JNIEnv* env, jstring value)
{
    if(env == nullptr || value == nullptr) {
        return {};
    }

    const char* utfChars = env->GetStringUTFChars(value, nullptr);
    if(utfChars == nullptr) {
        return {};
    }

    std::string result(utfChars);
    env->ReleaseStringUTFChars(value, utfChars);
    return result;
}

std::string buildAndroidSystemCaBundlePath()
{
    JNIEnv* env = reinterpret_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
    if(env == nullptr || activity == nullptr) {
        return {};
    }

    jclass activityClass = env->GetObjectClass(activity);
    if(activityClass == nullptr) {
        return {};
    }

    const jmethodID method = env->GetMethodID(activityClass, "geranesPrepareSystemCaBundle", "()Ljava/lang/String;");
    if(method == nullptr) {
        env->DeleteLocalRef(activityClass);
        return {};
    }

    jstring pathValue = static_cast<jstring>(env->CallObjectMethod(activity, method));
    std::string path;
    if(!env->ExceptionCheck()) {
        path = fromJString(env, pathValue);
    } else {
        env->ExceptionClear();
    }

    if(pathValue != nullptr) {
        env->DeleteLocalRef(pathValue);
    }
    env->DeleteLocalRef(activityClass);
    return path;
}
#endif

#if defined(_WIN32)
struct ParsedWebSocketUrl
{
    bool secure = false;
    INTERNET_PORT port = 0;
    std::string host;
    std::string path;
};

std::wstring utf8ToWide(const std::string& text)
{
    if(text.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if(length <= 0) {
        return {};
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), length);
    if(!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

std::string wideToUtf8(const std::wstring& text)
{
    if(text.empty()) {
        return {};
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if(length <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), length, nullptr, nullptr);
    if(!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

std::string formatWindowsErrorMessage(DWORD error)
{
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags,
                                        nullptr,
                                        error,
                                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                        reinterpret_cast<LPWSTR>(&buffer),
                                        0,
                                        nullptr);
    std::string message;
    if(length > 0 && buffer != nullptr) {
        std::wstring wideMessage(buffer, static_cast<size_t>(length));
        while(!wideMessage.empty() && (wideMessage.back() == L'\r' || wideMessage.back() == L'\n')) {
            wideMessage.pop_back();
        }
        message = wideToUtf8(wideMessage);
    }
    if(buffer != nullptr) {
        LocalFree(buffer);
    }
    if(message.empty()) {
        message = "Windows error " + std::to_string(error);
    }
    return message;
}

std::string formatWinHttpError(const std::string& prefix, DWORD error)
{
    return prefix + ": " + formatWindowsErrorMessage(error);
}

bool parseWebSocketUrl(const std::string& url, ParsedWebSocketUrl& parsed, std::string& error)
{
    const bool secure = url.rfind("wss://", 0) == 0;
    const bool plain = url.rfind("ws://", 0) == 0;
    if(!secure && !plain) {
        error = "WebRTC signaling desktop client requires ws:// or wss:// URLs";
        return false;
    }

    const size_t schemeLength = secure ? 6 : 5;
    const size_t authorityEnd = url.find('/', schemeLength);
    const std::string authority = authorityEnd == std::string::npos
        ? url.substr(schemeLength)
        : url.substr(schemeLength, authorityEnd - schemeLength);
    if(authority.empty()) {
        error = "WebRTC signaling URL is missing a host";
        return false;
    }

    std::string host = authority;
    INTERNET_PORT port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

    if(host.front() == '[') {
        const size_t bracketEnd = host.find(']');
        if(bracketEnd == std::string::npos) {
            error = "WebRTC signaling URL contains an invalid IPv6 host";
            return false;
        }
        if(bracketEnd + 1 < host.size() && host[bracketEnd + 1] == ':') {
            const std::string portText = host.substr(bracketEnd + 2);
            if(portText.empty()) {
                error = "WebRTC signaling URL contains an empty port";
                return false;
            }
            try {
                port = static_cast<INTERNET_PORT>(std::stoi(portText));
            } catch(...) {
                error = "WebRTC signaling URL contains an invalid port";
                return false;
            }
        }
        host = host.substr(0, bracketEnd + 1);
    } else {
        const size_t colonPos = host.rfind(':');
        if(colonPos != std::string::npos) {
            const std::string portText = host.substr(colonPos + 1);
            bool numeric = !portText.empty();
            for(const char ch : portText) {
                if(ch < '0' || ch > '9') {
                    numeric = false;
                    break;
                }
            }
            if(numeric) {
                try {
                    port = static_cast<INTERNET_PORT>(std::stoi(portText));
                } catch(...) {
                    error = "WebRTC signaling URL contains an invalid port";
                    return false;
                }
                host = host.substr(0, colonPos);
            }
        }
    }

    if(host.empty()) {
        error = "WebRTC signaling URL is missing a host";
        return false;
    }

    parsed.secure = secure;
    parsed.port = port;
    parsed.host = host;
    parsed.path = authorityEnd == std::string::npos ? "/" : url.substr(authorityEnd);
    return true;
}

void closeInternetHandle(HINTERNET& handle)
{
    if(handle != nullptr) {
        WinHttpCloseHandle(handle);
        handle = nullptr;
    }
}

class DesktopWebRtcSignalingClient final : public IWebRtcSignalingClient
{
private:
    struct CallbackContext
    {
        std::atomic<bool> alive{true};
        DesktopWebRtcSignalingClient* owner = nullptr;
    };

    struct ConnectionHandles
    {
        HINTERNET session = nullptr;
        HINTERNET connection = nullptr;
        HINTERNET request = nullptr;
        HINTERNET websocket = nullptr;

        ~ConnectionHandles()
        {
            closeInternetHandle(websocket);
            closeInternetHandle(request);
            closeInternetHandle(connection);
            closeInternetHandle(session);
        }
    };

    std::shared_ptr<ConnectionHandles> m_handles;
    std::shared_ptr<std::thread> m_receiveThread;
    bool m_connected = false;
    std::string m_lastError;
    std::deque<Event> m_events;
    std::shared_ptr<CallbackContext> m_callbackContext;
    mutable std::mutex m_mutex;

    static DesktopWebRtcSignalingClient* ownerFromContext(const std::shared_ptr<CallbackContext>& context)
    {
        if(!context || !context->alive.load(std::memory_order_acquire)) {
            return nullptr;
        }
        return context->owner;
    }

    void logTrace(const std::string& message) const
    {
        logNetplayMessage("[WebRTC signaling] " + message, NetplayLogLevel::Info);
    }

    void pushEvent(Event&& event)
    {
        std::scoped_lock lock(m_mutex);
        m_events.push_back(std::move(event));
    }

    void handleTextMessage(const std::string& text)
    {
        Event event;
        event.type = Event::Type::Message;
        event.text = text;
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
            logTrace(messageLog);
        } else {
            logTrace("socket message received with unparsed payload");
        }
        pushEvent(std::move(event));
    }

    void handleRemoteClose()
    {
        {
            std::scoped_lock lock(m_mutex);
            if(!m_connected) {
                return;
            }
            m_connected = false;
        }
        logTrace("socket closed");
        pushEvent(Event{Event::Type::Disconnected, {}, {}});
    }

    void handleSocketError(const std::string& error)
    {
        {
            std::scoped_lock lock(m_mutex);
            if(!m_connected && m_lastError == error) {
                return;
            }
            m_connected = false;
            m_lastError = error;
        }
        logTrace("socket error: " + error);
        pushEvent(Event{Event::Type::Error, {}, error});
    }

    static void receiveLoop(const std::shared_ptr<ConnectionHandles>& handles,
                            const std::shared_ptr<CallbackContext>& callbackContext)
    {
        std::vector<uint8_t> buffer(kDesktopSignalingReceiveBufferSize);
        std::string pendingMessage;

        for(;;) {
            DWORD bytesRead = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
            const DWORD result = WinHttpWebSocketReceive(handles->websocket,
                                                         buffer.data(),
                                                         static_cast<DWORD>(buffer.size()),
                                                         &bytesRead,
                                                         &bufferType);

            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }

            if(result != ERROR_SUCCESS) {
                if(result == ERROR_WINHTTP_OPERATION_CANCELLED || result == ERROR_INVALID_OPERATION) {
                    return;
                }
                self->handleSocketError(formatWinHttpError("WebRTC signaling socket receive failed", result));
                return;
            }

            switch(bufferType) {
                case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
                case WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE: {
                    pendingMessage.append(reinterpret_cast<const char*>(buffer.data()), bytesRead);
                    if(bufferType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                        self->handleTextMessage(pendingMessage);
                        pendingMessage.clear();
                    }
                    break;
                }

                case WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE:
                    self->handleRemoteClose();
                    return;

                default:
                    self->handleSocketError("WebRTC signaling received unsupported websocket frame type");
                    return;
            }
        }
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

        ParsedWebSocketUrl parsedUrl;
        if(!parseWebSocketUrl(options.config.url, parsedUrl, m_lastError)) {
            return false;
        }

        auto handles = std::make_shared<ConnectionHandles>();
        std::string error;
        const std::wstring host = utf8ToWide(parsedUrl.host);
        const std::wstring path = utf8ToWide(parsedUrl.path);
        if(host.empty() || path.empty()) {
            m_lastError = "WebRTC signaling URL contains invalid UTF-8";
            return false;
        }

        handles->session = WinHttpOpen(L"Netplay/1.0",
                                       WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS,
                                       0);
        if(handles->session == nullptr) {
            m_lastError = formatWinHttpError("Unable to initialize WinHTTP session", GetLastError());
            return false;
        }

        WinHttpSetTimeouts(handles->session,
                           static_cast<int>(kDesktopSignalingConnectTimeout.count()),
                           static_cast<int>(kDesktopSignalingConnectTimeout.count()),
                           static_cast<int>(kDesktopSignalingConnectTimeout.count()),
                           static_cast<int>(kDesktopSignalingConnectTimeout.count()));

        handles->connection = WinHttpConnect(handles->session, host.c_str(), parsedUrl.port, 0);
        if(handles->connection == nullptr) {
            m_lastError = formatWinHttpError("Unable to connect to signaling host", GetLastError());
            return false;
        }

        const DWORD requestFlags = parsedUrl.secure ? WINHTTP_FLAG_SECURE : 0;
        handles->request = WinHttpOpenRequest(handles->connection,
                                              L"GET",
                                              path.c_str(),
                                              nullptr,
                                              WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              requestFlags);
        if(handles->request == nullptr) {
            m_lastError = formatWinHttpError("Unable to open signaling websocket request", GetLastError());
            return false;
        }

        if(!WinHttpSetOption(handles->request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
            m_lastError = formatWinHttpError("Unable to enable websocket upgrade", GetLastError());
            return false;
        }

        logTrace("connecting to " + options.config.url);

        if(!WinHttpSendRequest(handles->request,
                               WINHTTP_NO_ADDITIONAL_HEADERS,
                               0,
                               WINHTTP_NO_REQUEST_DATA,
                               0,
                               0,
                               0)) {
            m_lastError = formatWinHttpError("Unable to send signaling websocket request", GetLastError());
            return false;
        }

        if(!WinHttpReceiveResponse(handles->request, nullptr)) {
            m_lastError = formatWinHttpError("Unable to receive signaling websocket response", GetLastError());
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if(!WinHttpQueryHeaders(handles->request,
                                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &statusCode,
                                &statusCodeSize,
                                WINHTTP_NO_HEADER_INDEX)) {
            m_lastError = formatWinHttpError("Unable to query signaling websocket response status", GetLastError());
            return false;
        }

        if(statusCode != HTTP_STATUS_SWITCH_PROTOCOLS) {
            m_lastError = "Signaling websocket upgrade failed with HTTP status " + std::to_string(statusCode);
            return false;
        }

        handles->websocket = WinHttpWebSocketCompleteUpgrade(handles->request, 0);
        if(handles->websocket == nullptr) {
            m_lastError = formatWinHttpError("Unable to complete websocket upgrade", GetLastError());
            return false;
        }
        closeInternetHandle(handles->request);

        auto callbackContext = std::make_shared<CallbackContext>();
        callbackContext->owner = this;
        auto receiveThread = std::make_shared<std::thread>(
            [handles, callbackContext]() {
                receiveLoop(handles, callbackContext);
            });

        {
            std::scoped_lock lock(m_mutex);
            m_handles = handles;
            m_receiveThread = receiveThread;
            m_callbackContext = callbackContext;
            m_connected = true;
            m_lastError.clear();
            m_events.clear();
        }

        logTrace("socket opened");
        pushEvent(Event{Event::Type::Connected, {}, {}});
        return true;
    }

    void disconnect() override
    {
        std::shared_ptr<ConnectionHandles> handles;
        std::shared_ptr<std::thread> receiveThread;
        std::shared_ptr<CallbackContext> callbackContext;
        {
            std::scoped_lock lock(m_mutex);
            handles = std::move(m_handles);
            receiveThread = std::move(m_receiveThread);
            callbackContext = std::move(m_callbackContext);
            m_connected = false;
            m_events.clear();
        }

        if(callbackContext) {
            callbackContext->alive.store(false, std::memory_order_release);
            callbackContext->owner = nullptr;
        }

        if(handles || receiveThread) {
            logTrace("disconnecting socket");
            enqueueSignalingCleanup([handles = std::move(handles), receiveThread = std::move(receiveThread)]() mutable {
                if(handles && handles->websocket != nullptr) {
                    WinHttpWebSocketClose(handles->websocket,
                                          WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                                          nullptr,
                                          0);
                }
                handles.reset();
                if(receiveThread && receiveThread->joinable()) {
                    receiveThread->join();
                }
                receiveThread.reset();
            });
        }
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        std::shared_ptr<ConnectionHandles> handles;
        bool connected = false;
        {
            std::scoped_lock lock(m_mutex);
            handles = m_handles;
            connected = m_connected;
        }

        if(!handles || handles->websocket == nullptr || !connected) {
            m_lastError = "WebRTC signaling socket is not connected";
            return false;
        }

        const std::string payload = message.toText();
        const DWORD result = WinHttpWebSocketSend(handles->websocket,
                                                  WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                                  reinterpret_cast<void*>(const_cast<char*>(payload.data())),
                                                  static_cast<DWORD>(payload.size()));
        if(result != ERROR_SUCCESS) {
            m_lastError = formatWinHttpError("WebRTC signaling send failed", result);
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

#else

void ensureIxNetSystemInitialized()
{
    static std::once_flag once;
    static bool initialized = false;
    std::call_once(once, []() {
        initialized = ix::initNetSystem();
    });
    if(!initialized) {
        throw std::runtime_error("IXWebSocket network initialization failed");
    }
}

class DesktopWebRtcSignalingClient final : public IWebRtcSignalingClient
{
private:
    struct CallbackContext
    {
        std::atomic<bool> alive{true};
        DesktopWebRtcSignalingClient* owner = nullptr;
    };

    std::shared_ptr<ix::WebSocket> m_socket;
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
        logNetplayMessage("[WebRTC signaling] " + message, NetplayLogLevel::Info);
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
        ensureIxNetSystemInitialized();

        if(!options.config.valid()) {
            m_lastError = "Configure signaling URL and room id for WebRTC";
            return false;
        }

        if(!startsWith(options.config.url, "ws://") &&
           !startsWith(options.config.url, "wss://")) {
            m_lastError = "WebRTC signaling desktop client requires ws:// or wss:// URLs";
            return false;
        }

        auto socket = std::make_shared<ix::WebSocket>();
        socket->setUrl(options.config.url);
        socket->setHandshakeTimeout(
            static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(kDesktopSignalingConnectTimeout).count())
        );
        socket->disableAutomaticReconnection();
        socket->disablePerMessageDeflate();
        if(startsWith(options.config.url, "wss://")) {
            ix::SocketTLSOptions tlsOptions;
#if defined(_WIN32)
            const std::string windowsRootBundle = buildWindowsRootCaBundle();
            if(!windowsRootBundle.empty()) {
                tlsOptions.caFile = windowsRootBundle;
                logTrace("using Windows root CA bundle for TLS verification");
            } else {
                logTrace("Windows root CA bundle unavailable; falling back to IXWebSocket system TLS defaults");
            }
#elif defined(__ANDROID__)
            const std::string androidSystemCaBundle = buildAndroidSystemCaBundlePath();
            if(!androidSystemCaBundle.empty()) {
                tlsOptions.caFile = androidSystemCaBundle;
                logTrace("using Android exported system CA bundle for TLS verification");
            } else {
                logTrace("Android system CA bundle unavailable; IXWebSocket TLS verification may fail");
            }
#endif
            socket->setTLSOptions(tlsOptions);
        }
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

        socket->setOnMessageCallback([callbackContext](const ix::WebSocketMessagePtr& msg) {
            auto* self = ownerFromContext(callbackContext);
            if(self == nullptr) {
                return;
            }

            switch(msg->type) {
                case ix::WebSocketMessageType::Open: {
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
                    return;
                }

                case ix::WebSocketMessageType::Close: {
                    {
                        std::scoped_lock lock(self->m_mutex);
                        self->m_connected = false;
                    }
                    self->logTrace("socket closed");
                    self->pushEvent(Event{Event::Type::Disconnected, {}, {}});
                    self->markConnectResolved(false, "WebRTC signaling socket closed");
                    return;
                }

                case ix::WebSocketMessageType::Error: {
                    const std::string copiedError =
                        !msg->errorInfo.reason.empty() ? msg->errorInfo.reason : "WebRTC signaling socket error";
                    {
                        std::scoped_lock lock(self->m_mutex);
                        self->m_connected = false;
                        self->m_lastError = copiedError;
                    }
                    self->logTrace("socket error: " + copiedError);
                    self->pushEvent(Event{Event::Type::Error, {}, copiedError});
                    self->markConnectResolved(false, copiedError);
                    return;
                }

                case ix::WebSocketMessageType::Message: {
                    Event event;
                    event.type = Event::Type::Message;
                    event.text = msg->str;
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
                    return;
                }

                default:
                    return;
            }
        });

        try {
            socket->start();
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
        std::shared_ptr<ix::WebSocket> socket;
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
                    // ixwebsocket still emits a close event while stop() runs.
                    // Clearing the callback first can make its internal close
                    // callback call an empty std::function and abort the process.
                    socket->stop();
                    socket->setOnMessageCallback(nullptr);
                } catch(...) {
                }
                socket.reset();
            });
        }
    }

    bool send(const WebRtcSignalingMessage& message) override
    {
        std::shared_ptr<ix::WebSocket> socket;
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

        const ix::WebSocketSendInfo sendInfo = socket->sendText(message.toText());
        if(!sendInfo.success) {
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
#endif

#if defined(__EMSCRIPTEN__)
class WebEmscriptenWebRtcSignalingClient;

extern "C" {
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_open(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_close(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_error(intptr_t selfPtr, const char* text);
EMSCRIPTEN_KEEPALIVE void geranes_ws_on_message(intptr_t selfPtr, const char* text);
}

void geranes_ws_connect_bridge(int handle, const char* urlPtr, intptr_t self)
{
    MAIN_THREAD_EM_ASM((function() {
        const scope = Module.__geranes_ws_bridge || (Module.__geranes_ws_bridge = {
            sockets: {}
        });

        scope.callExport = scope.callExport || function(name, args) {
            const fn = Module['_' + name];
            if(typeof fn !== 'function') {
                console.error('[Netplay][WS] missing export', name);
                return;
            }
            fn.apply(null, args || []);
        };

        scope.callExportString = scope.callExportString || function(name, selfPtr, text) {
            const value = text || "";
            const len = lengthBytesUTF8(value) + 1;
            const ptr = _malloc(len);
            stringToUTF8(value, ptr, len);
            try {
                scope.callExport(name, [selfPtr, ptr]);
            } finally {
                _free(ptr);
            }
        };

        if(typeof WebSocket === 'undefined') {
            scope.callExportString('geranes_ws_on_error', $2, 'Browser WebSocket API is not available');
            return;
        }

        try {
            const handle = $0;
            const url = UTF8ToString($1);
            const selfPtr = $2;
            console.log('[Netplay][WS] connecting on main thread', { handle: handle, url: url });
            const ws = new WebSocket(url);
            ws.binaryType = 'arraybuffer';
            scope.sockets[handle] = ws;

            ws.onopen = function() {
                console.log('[Netplay][WS] open', { handle: handle, url: url });
                scope.callExport('geranes_ws_on_open', [selfPtr]);
            };
            ws.onclose = function(event) {
                console.log('[Netplay][WS] close', {
                    handle: handle,
                    code: event ? event.code : 0,
                    reason: event ? event.reason : "",
                    wasClean: event ? event.wasClean : false
                });
                scope.callExport('geranes_ws_on_close', [selfPtr]);
            };
            ws.onerror = function(event) {
                console.error('[Netplay][WS] error', {
                    handle: handle,
                    url: url,
                    readyState: ws.readyState,
                    event: event
                });
                scope.callExportString('geranes_ws_on_error', selfPtr, 'WebRTC signaling WebSocket error');
            };
            ws.onmessage = function(event) {
                const text = (typeof event.data === 'string')
                    ? event.data
                    : "";
                scope.callExportString('geranes_ws_on_message', selfPtr, text);
            };
        } catch(err) {
            console.error('[Netplay][WS] constructor/setup failed', err);
            try {
                scope.callExportString('geranes_ws_on_error', $2, err && err.message ? err.message : 'WebRTC signaling WebSocket setup failed');
            } catch(_) {
            }
        }
    })(), handle, urlPtr, self);
}

void geranes_ws_close_bridge(int handle)
{
    MAIN_THREAD_EM_ASM((function() {
        const scope = Module.__geranes_ws_bridge;
        const handle = $0;
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
    })(), handle);
}

int geranes_ws_send_bridge(int handle, const char* textPtr)
{
    return MAIN_THREAD_EM_ASM_INT(return (function() {
        const scope = Module.__geranes_ws_bridge;
        const handle = $0;
        if(!scope || !scope.sockets[handle]) {
            return 0;
        }
        const ws = scope.sockets[handle];
        if(ws.readyState !== WebSocket.OPEN) {
            return 0;
        }
        try {
            const text = UTF8ToString($1);
            ws.send(text);
            return 1;
        } catch(_) {
            return 0;
        }
    })();, handle, textPtr);
}

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
        logNetplayMessage("WebRTC signaling WebSocket closed", NetplayLogLevel::Warning);
        setConnected(false);
        pushEvent(Event{Event::Type::Disconnected, {}, {}});
    }

    void onError(const char* text)
    {
        const std::string error = text != nullptr ? text : "WebRTC signaling WebSocket error";
        logNetplayMessage(error, NetplayLogLevel::Error);
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

        static std::atomic<int> nextSocketHandle{1};
        m_socketHandle = nextSocketHandle.fetch_add(1, std::memory_order_relaxed);
        if(m_socketHandle == 0) {
            m_socketHandle = nextSocketHandle.fetch_add(1, std::memory_order_relaxed);
        }

        geranes_ws_connect_bridge(
            m_socketHandle,
            options.config.url.c_str(),
            reinterpret_cast<intptr_t>(this));

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

} // namespace ConsoleNetplay
