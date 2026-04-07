#include "GeraNESNetplay/WebRtcPeerConnection.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

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

#if defined(__EMSCRIPTEN__)
class WebEmscriptenWebRtcPeerConnection;

extern "C" {
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_local_description(intptr_t selfPtr, const char* sdp, int offer);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_ice_candidate(intptr_t selfPtr, const char* candidate, const char* mid, int mlineIndex);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_channel_open(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_channel_close(intptr_t selfPtr);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_data_message(intptr_t selfPtr, const uint8_t* data, int size);
EMSCRIPTEN_KEEPALIVE void geranes_rtc_on_error(intptr_t selfPtr, const char* text);
}

EM_JS(int, geranes_rtc_open_bridge, (const char* iceServersJsonPtr,
                                     int host,
                                     intptr_t self), {
    if(typeof RTCPeerConnection === 'undefined') {
        return 0;
    }

    const scope = Module.__geranes_rtc_bridge || (Module.__geranes_rtc_bridge = {
        nextHandle: 1,
        peers: {}
    });

    function callBytes(selfPtr, data) {
        const view = data instanceof Uint8Array ? data : new Uint8Array(data);
        const ptr = _malloc(view.length);
        HEAPU8.set(view, ptr);
        Module.ccall('geranes_rtc_on_data_message', null, ['number', 'number', 'number'], [selfPtr, ptr, view.length]);
        _free(ptr);
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
        Module.ccall(
            'geranes_rtc_on_error',
            null,
            ['number', 'string'],
            [selfPtr, message]
        );
    }

    try {
        const iceServersRaw = UTF8ToString(iceServersJsonPtr || 0);
        let iceServers = [];
        if(iceServersRaw) {
            try {
                const parsed = JSON.parse(iceServersRaw);
                if(Array.isArray(parsed)) {
                    iceServers = parsed
                        .filter(v => typeof v === 'string' && v.length > 0)
                        .map(v => ({ urls: v }));
                }
            } catch(_) {
            }
        }

        const pc = new RTCPeerConnection({ iceServers: iceServers });
        const handle = scope.nextHandle++;
        const state = {
            pc: pc,
            dc: null
        };
        scope.peers[handle] = state;

        function attachChannel(dc) {
            state.dc = dc;
            dc.binaryType = 'arraybuffer';
            dc.onopen = function() {
                Module.ccall('geranes_rtc_on_channel_open', null, ['number'], [self]);
            };
            dc.onclose = function() {
                Module.ccall('geranes_rtc_on_channel_close', null, ['number'], [self]);
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
            Module.ccall(
                'geranes_rtc_on_ice_candidate',
                null,
                ['number', 'string', 'string', 'number'],
                [
                    self,
                    event.candidate.candidate || "",
                    event.candidate.sdpMid || "",
                    typeof event.candidate.sdpMLineIndex === 'number' ? event.candidate.sdpMLineIndex : -1
                ]
            );
        };
        pc.onconnectionstatechange = function() {
            if(pc.connectionState === 'failed') {
                reportError(self, scope.describeRtcError('WebRTC peer connection failed', pc, state.dc));
            } else if(pc.connectionState === 'disconnected' || pc.connectionState === 'closed') {
                Module.ccall('geranes_rtc_on_channel_close', null, ['number'], [self]);
            }
        };
        pc.oniceconnectionstatechange = function() {};
        pc.onsignalingstatechange = function() {};
        pc.ondatachannel = function(event) {
            if(event && event.channel) {
                attachChannel(event.channel);
            }
        };

        if(host) {
            attachChannel(pc.createDataChannel('geranes', { ordered: true }));
        }

        return handle;
    } catch(err) {
        try {
            reportError(self, err);
        } catch(_) {
        }
        return 0;
    }
});

EM_JS(void, geranes_rtc_close_bridge, (int handle), {
    const scope = Module.__geranes_rtc_bridge;
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
});

EM_JS(int, geranes_rtc_create_offer_bridge, (int handle, intptr_t self, intptr_t onLocalDescription, intptr_t onError), {
    const scope = Module.__geranes_rtc_bridge;
    if(!scope || !scope.peers[handle]) {
        return 0;
    }

    function reportError(selfPtr, err) {
        const message = scope.describeRtcError
            ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
            : String(err);
        console.error('[GeraNES][RTC] createOffer failed', { self: selfPtr, error: err, message: message });
        Module.ccall(
            'geranes_rtc_on_error',
            null,
            ['number', 'string'],
            [selfPtr, message]
        );
    }

    (async function() {
        try {
            const pc = scope.peers[handle].pc;
            const offer = await pc.createOffer();
            await pc.setLocalDescription(offer);
            Module.ccall(
                'geranes_rtc_on_local_description',
                null,
                ['number', 'string', 'number'],
                [self, pc.localDescription ? pc.localDescription.sdp : offer.sdp, 1]
            );
        } catch(err) {
            reportError(self, err);
        }
    })();

    return 1;
});

EM_JS(int, geranes_rtc_create_answer_bridge, (int handle, intptr_t self), {
    const scope = Module.__geranes_rtc_bridge;
    if(!scope || !scope.peers[handle]) {
        return 0;
    }

    function reportError(selfPtr, err) {
        const message = scope.describeRtcError
            ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
            : String(err);
        console.error('[GeraNES][RTC] createAnswer failed', { self: selfPtr, error: err, message: message });
        Module.ccall(
            'geranes_rtc_on_error',
            null,
            ['number', 'string'],
            [selfPtr, message]
        );
    }

    (async function() {
        try {
            const pc = scope.peers[handle].pc;
            const answer = await pc.createAnswer();
            await pc.setLocalDescription(answer);
            Module.ccall(
                'geranes_rtc_on_local_description',
                null,
                ['number', 'string', 'number'],
                [self, pc.localDescription ? pc.localDescription.sdp : answer.sdp, 0]
            );
        } catch(err) {
            reportError(self, err);
        }
    })();

    return 1;
});

EM_JS(int, geranes_rtc_set_remote_description_bridge, (int handle, const char* sdpPtr, int offer, intptr_t self), {
    const scope = Module.__geranes_rtc_bridge;
    if(!scope || !scope.peers[handle]) {
        return 0;
    }

    const sdp = UTF8ToString(sdpPtr);

    function reportError(selfPtr, err) {
        const message = scope.describeRtcError
            ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
            : String(err);
        console.error('[GeraNES][RTC] setRemoteDescription failed', { self: selfPtr, error: err, message: message });
        Module.ccall(
            'geranes_rtc_on_error',
            null,
            ['number', 'string'],
            [selfPtr, message]
        );
    }

    (async function() {
        try {
            const pc = scope.peers[handle].pc;
            await pc.setRemoteDescription({
                type: offer ? 'offer' : 'answer',
                sdp: sdp
            });
        } catch(err) {
            reportError(self, err);
        }
    })();

    return 1;
});

EM_JS(int, geranes_rtc_add_ice_candidate_bridge, (int handle,
                                                  const char* candidatePtr,
                                                  const char* midPtr,
                                                  int mlineIndex,
                                                  intptr_t self), {
    const scope = Module.__geranes_rtc_bridge;
    if(!scope || !scope.peers[handle]) {
        return 0;
    }

    const candidate = UTF8ToString(candidatePtr);
    const mid = UTF8ToString(midPtr);

    function reportError(selfPtr, err) {
        const message = scope.describeRtcError
            ? scope.describeRtcError(err, scope.peers[handle] ? scope.peers[handle].pc : null, scope.peers[handle] ? scope.peers[handle].dc : null)
            : String(err);
        console.error('[GeraNES][RTC] addIceCandidate failed', { self: selfPtr, error: err, message: message });
        Module.ccall(
            'geranes_rtc_on_error',
            null,
            ['number', 'string'],
            [selfPtr, message]
        );
    }

    (async function() {
        try {
            const pc = scope.peers[handle].pc;
            await pc.addIceCandidate({
                candidate: candidate,
                sdpMid: mid || null,
                sdpMLineIndex: mlineIndex >= 0 ? mlineIndex : null
            });
        } catch(err) {
            reportError(self, err);
        }
    })();

    return 1;
});

EM_JS(int, geranes_rtc_send_bridge, (int handle, const uint8_t* payloadPtr, int payloadSize), {
    const scope = Module.__geranes_rtc_bridge;
    if(!scope || !scope.peers[handle]) {
        return 0;
    }
    const state = scope.peers[handle];
    if(!state.dc || state.dc.readyState !== 'open') {
        return 0;
    }
    try {
        const view = HEAPU8.slice(payloadPtr, payloadPtr + payloadSize);
        state.dc.send(view);
        return 1;
    } catch(_) {
        return 0;
    }
});

class WebEmscriptenWebRtcPeerConnection final : public IWebRtcPeerConnection
{
private:
    int m_handle = 0;
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

        m_handle = geranes_rtc_open_bridge(
            iceServers.dump().c_str(),
            options.host ? 1 : 0,
            reinterpret_cast<intptr_t>(this));

        if(m_handle == 0) {
            m_lastError = "Failed to create browser WebRTC peer connection";
            return false;
        }

        m_open = true;
        m_dataChannelOpen = false;
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
    return std::make_unique<WebEmscriptenWebRtcPeerConnection>();
#endif
}

} // namespace Netplay
