#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/PPU.h"
#include "GeraNESNetplay/NetplayRuntime.h"

class EmulationHost : public SigSlot::SigSlotBase
{
public:
    struct InputState
    {
        bool p1A = false;
        bool p1B = false;
        bool p1Select = false;
        bool p1Start = false;
        bool p1Up = false;
        bool p1Down = false;
        bool p1Left = false;
        bool p1Right = false;
        bool p1X = false;
        bool p1Y = false;
        bool p1L = false;
        bool p1R = false;
        bool p1Up2 = false;
        bool p1Down2 = false;
        bool p1Left2 = false;
        bool p1Right2 = false;

        bool p2A = false;
        bool p2B = false;
        bool p2Select = false;
        bool p2Start = false;
        bool p2Up = false;
        bool p2Down = false;
        bool p2Left = false;
        bool p2Right = false;
        bool p2X = false;
        bool p2Y = false;
        bool p2L = false;
        bool p2R = false;
        bool p2Up2 = false;
        bool p2Down2 = false;
        bool p2Left2 = false;
        bool p2Right2 = false;

        bool p3A = false;
        bool p3B = false;
        bool p3Select = false;
        bool p3Start = false;
        bool p3Up = false;
        bool p3Down = false;
        bool p3Left = false;
        bool p3Right = false;

        bool p4A = false;
        bool p4B = false;
        bool p4Select = false;
        bool p4Start = false;
        bool p4Up = false;
        bool p4Down = false;
        bool p4Left = false;
        bool p4Right = false;
        std::array<bool, 12> p1PowerPadButtons = {};
        std::array<bool, 12> p2PowerPadButtons = {};
        IExpansionDevice::SuborKeyboardKeys suborKeyboardKeys = {};
        IExpansionDevice::FamilyBasicKeyboardKeys familyBasicKeyboardKeys = {};

        int zapperX = -1;
        int zapperY = -1;
        int mouseDeltaX = 0;
        int mouseDeltaY = 0;
        float arkanoidNesPosition = 0.5f;
        float arkanoidFamicomPosition = 0.5f;
        bool zapperP1Trigger = false;
        bool zapperP2Trigger = false;
        bool bandaiTrigger = false;
        bool konamiP1Run = false;
        bool konamiP1Jump = false;
        bool konamiP2Run = false;
        bool konamiP2Jump = false;
        bool mousePrimaryButton = false;
        bool mouseSecondaryButton = false;

        bool rewind = false;
        bool speedBoost = false;
    };

    using FrameInputProvider = std::function<std::optional<InputState>(uint32_t)>;

    struct NetplayDiagnosticsSnapshot
    {
        bool enabled = false;
        uint32_t currentFrame = 0;
        size_t snapshotCapacity = 0;
        size_t storedSnapshots = 0;
        uint32_t latestSnapshotCrc32 = 0;
        Netplay::RollbackStats rollbackStats;
    };

private:
    static void applyInputStateToEmu(GeraNESEmu& emu, const InputState& input)
    {
        InputFrame frame = emu.createInputFrame(emu.frameCount());
        frame.p1A = input.p1A; frame.p1B = input.p1B; frame.p1Select = input.p1Select; frame.p1Start = input.p1Start;
        frame.p1Up = input.p1Up; frame.p1Down = input.p1Down; frame.p1Left = input.p1Left; frame.p1Right = input.p1Right;
        frame.p1X = input.p1X; frame.p1Y = input.p1Y; frame.p1L = input.p1L; frame.p1R = input.p1R;
        frame.p2A = input.p2A; frame.p2B = input.p2B; frame.p2Select = input.p2Select; frame.p2Start = input.p2Start;
        frame.p2Up = input.p2Up; frame.p2Down = input.p2Down; frame.p2Left = input.p2Left; frame.p2Right = input.p2Right;
        frame.p2X = input.p2X; frame.p2Y = input.p2Y; frame.p2L = input.p2L; frame.p2R = input.p2R;
        frame.p3A = input.p3A; frame.p3B = input.p3B; frame.p3Select = input.p3Select; frame.p3Start = input.p3Start;
        frame.p3Up = input.p3Up; frame.p3Down = input.p3Down; frame.p3Left = input.p3Left; frame.p3Right = input.p3Right;
        frame.p4A = input.p4A; frame.p4B = input.p4B; frame.p4Select = input.p4Select; frame.p4Start = input.p4Start;
        frame.p4Up = input.p4Up; frame.p4Down = input.p4Down; frame.p4Left = input.p4Left; frame.p4Right = input.p4Right;
        frame.vbP1A = input.p1A; frame.vbP1B = input.p1B; frame.vbP1Select = input.p1Select; frame.vbP1Start = input.p1Start;
        frame.vbP1Up0 = input.p1Up; frame.vbP1Down0 = input.p1Down; frame.vbP1Left0 = input.p1Left; frame.vbP1Right0 = input.p1Right;
        frame.vbP1Up1 = input.p1Up2; frame.vbP1Down1 = input.p1Down2; frame.vbP1Left1 = input.p1Left2; frame.vbP1Right1 = input.p1Right2;
        frame.vbP1L = input.p1L; frame.vbP1R = input.p1R;
        frame.vbP2A = input.p2A; frame.vbP2B = input.p2B; frame.vbP2Select = input.p2Select; frame.vbP2Start = input.p2Start;
        frame.vbP2Up0 = input.p2Up; frame.vbP2Down0 = input.p2Down; frame.vbP2Left0 = input.p2Left; frame.vbP2Right0 = input.p2Right;
        frame.vbP2Up1 = input.p2Up2; frame.vbP2Down1 = input.p2Down2; frame.vbP2Left1 = input.p2Left2; frame.vbP2Right1 = input.p2Right2;
        frame.vbP2L = input.p2L; frame.vbP2R = input.p2R;
        frame.powerPadP1Buttons = input.p1PowerPadButtons;
        frame.powerPadP2Buttons = input.p2PowerPadButtons;
        frame.suborKeyboardKeys = input.suborKeyboardKeys;
        frame.familyBasicKeyboardKeys = input.familyBasicKeyboardKeys;
        frame.bandaiA = input.p2A; frame.bandaiB = input.p2B; frame.bandaiSelect = input.p2Select; frame.bandaiStart = input.p2Start;
        frame.bandaiUp = input.p2Up; frame.bandaiDown = input.p2Down; frame.bandaiLeft = input.p2Left; frame.bandaiRight = input.p2Right;
        frame.zapperP1X = input.zapperX; frame.zapperP1Y = input.zapperY; frame.zapperP1Trigger = input.zapperP1Trigger;
        frame.zapperP2X = input.zapperX; frame.zapperP2Y = input.zapperY; frame.zapperP2Trigger = input.zapperP2Trigger;
        frame.bandaiX = input.zapperX; frame.bandaiY = input.zapperY; frame.bandaiTrigger = input.bandaiTrigger;
        frame.arkanoidP1Position = input.arkanoidNesPosition; frame.arkanoidP1Button = input.mousePrimaryButton;
        frame.arkanoidP2Position = input.arkanoidNesPosition; frame.arkanoidP2Button = input.mousePrimaryButton;
        frame.arkanoidFamicomPosition = input.arkanoidFamicomPosition; frame.arkanoidFamicomButton = input.mousePrimaryButton;
        frame.konamiP1Run = input.konamiP1Run; frame.konamiP1Jump = input.konamiP1Jump; frame.konamiP2Run = input.konamiP2Run; frame.konamiP2Jump = input.konamiP2Jump;
        frame.snesMouseP1DeltaX = input.mouseDeltaX; frame.snesMouseP1DeltaY = input.mouseDeltaY; frame.snesMouseP1Left = input.mousePrimaryButton; frame.snesMouseP1Right = input.mouseSecondaryButton;
        frame.snesMouseP2DeltaX = input.mouseDeltaX; frame.snesMouseP2DeltaY = input.mouseDeltaY; frame.snesMouseP2Left = input.mousePrimaryButton; frame.snesMouseP2Right = input.mouseSecondaryButton;
        frame.suborMouseP1DeltaX = input.mouseDeltaX; frame.suborMouseP1DeltaY = input.mouseDeltaY; frame.suborMouseP1Left = input.mousePrimaryButton; frame.suborMouseP1Right = input.mouseSecondaryButton;
        frame.suborMouseP2DeltaX = input.mouseDeltaX; frame.suborMouseP2DeltaY = input.mouseDeltaY; frame.suborMouseP2Left = input.mousePrimaryButton; frame.suborMouseP2Right = input.mouseSecondaryButton;
        emu.queueInputFrame(frame);
        emu.setRewind(input.rewind);
        emu.setSpeedBoost(input.speedBoost);
    }

    struct Snapshot
    {
        bool valid = false;
        bool paused = false;
        bool rewinding = false;
        bool spriteLimitDisabled = false;
        bool overclocked = false;
        bool nsfLoaded = false;
        bool nsfPlaying = false;
        bool nsfPaused = false;
        bool nsfEnded = false;
        uint32_t frameCount = 0;
        uint32_t regionFps = 60;
        Settings::Region region = Settings::Region::NTSC;
        std::optional<Settings::Device> port1Device = Settings::Device::CONTROLLER;
        std::optional<Settings::Device> port2Device = Settings::Device::CONTROLLER;
        Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
        Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
        Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
        int nsfTotalSongs = 0;
        int nsfCurrentSong = 0;
        std::string audioDeviceName;
        std::vector<std::string> audioDevices;
        float audioVolume = 1.0f;
        std::string audioChannelsJson = "{\"channels\":[]}";
    };

    mutable std::mutex m_netplayRuntimeMutex;
    Netplay::NetplayRuntime m_netplayRuntime;
    bool m_netplayRuntimeEnabled = false;

    void resetNetplayRuntimeLocked()
    {
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        m_netplayRuntime.reset();
    }

    void requestNetplaySnapshotOnFrameReady()
    {
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        if(!m_netplayRuntimeEnabled || !m_emu.valid()) return;
        m_emu.requestNetplaySnapshotAtSafePoint();
    }

    void flushPendingNetplaySnapshotLocked()
    {
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        if(!m_netplayRuntimeEnabled || !m_emu.valid()) return;

        std::optional<GeraNESEmu::PendingNetplaySnapshot> snapshot = m_emu.consumeNetplaySnapshotAtSafePoint();
        if(!snapshot.has_value()) return;

        m_netplayRuntime.setCurrentFrame(snapshot->frame);
        m_netplayRuntime.captureSnapshot(snapshot->frame, [&snapshot]() {
            return snapshot->data;
        });
    }

#ifdef __EMSCRIPTEN__
    GeraNESEmu m_emu;
    IAudioOutput& m_audioOutput;
    InputState m_pendingInput;

    void applyPendingInput()
    {
        applyInputStateToEmu(m_emu, m_pendingInput);
    }
#else
    enum class FramePacingMode : uint8_t
    {
        Suspended,
        FreeRunning,
        PresenterLocked
    };

    GeraNESEmu m_emu;
    IAudioOutput& m_audioOutput;
    mutable std::mutex m_emuMutex;
    mutable std::mutex m_snapshotMutex;
    mutable std::mutex m_workerReadyMutex;
    std::condition_variable m_workerReadyCv;
    bool m_workerReady = false;
    mutable std::mutex m_workerStepMutex;
    std::condition_variable m_workerStepCv;
    uint64_t m_workerStepSerial = 0;
    mutable std::mutex m_presenterMutex;
    std::condition_variable m_presenterCv;
    std::atomic<bool> m_shutdownStarted{false};
    std::jthread m_workerThread;
    SigSlot::Signal<std::function<void(GeraNESEmu&)>> m_signalCommand;
    std::atomic<FramePacingMode> m_framePacingMode{FramePacingMode::FreeRunning};
    std::atomic<uint32_t> m_presenterTickDtMs{16};
    std::atomic<uint32_t> m_pendingPresenterTicks{0};
    std::atomic<bool> m_workerWakeRequested{false};
    std::atomic<int> m_frontFramebufferIndex{0};
    std::array<std::vector<uint32_t>, 2> m_framebuffers{
        std::vector<uint32_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0),
        std::vector<uint32_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0)
    };
    Snapshot m_snapshot;
    mutable std::mutex m_pendingInputMutex;
    InputState m_pendingInput;
    mutable std::mutex m_frameInputProviderMutex;
    FrameInputProvider m_frameInputProvider;
    std::optional<InputState> m_lastFrameProviderInput;
    bool m_repeatLastFrameProviderInput = true;

    void applyPendingInputLocked()
    {
        InputState input;
        FrameInputProvider frameInputProvider;
        {
            std::scoped_lock pendingInputLock(m_pendingInputMutex);
            input = m_pendingInput;
        }
        {
            std::scoped_lock providerLock(m_frameInputProviderMutex);
            frameInputProvider = m_frameInputProvider;
        }
        if(frameInputProvider) {
            const uint32_t playbackFrame = m_emu.frameCount() + 1u;
            if(std::optional<InputState> provided = frameInputProvider(playbackFrame); provided.has_value()) {
                input = *provided;
                m_lastFrameProviderInput = input;
            } else if(m_repeatLastFrameProviderInput && m_lastFrameProviderInput.has_value()) {
                input = *m_lastFrameProviderInput;
            } else {
                m_lastFrameProviderInput.reset();
            }
        }
        applyInputStateToEmu(m_emu, input);
    }

    void onCommand(std::function<void(GeraNESEmu&)> command)
    {
        if(command) {
            command(m_emu);
            refreshSnapshotLocked();
        }

        m_workerWakeRequested.store(true, std::memory_order_release);
        m_presenterCv.notify_one();
    }

    void refreshSnapshotLocked()
    {
        Snapshot snapshot;
        snapshot.valid = m_emu.valid();
        snapshot.paused = m_emu.paused();
        snapshot.rewinding = m_emu.isRewinding();
        snapshot.spriteLimitDisabled = m_emu.spriteLimitDisabled();
        snapshot.overclocked = m_emu.overclocked();
        snapshot.nsfLoaded = m_emu.isNsfLoaded();
        snapshot.nsfPlaying = m_emu.nsfIsPlaying();
        snapshot.nsfPaused = m_emu.nsfIsPaused();
        snapshot.nsfEnded = m_emu.nsfHasEnded();
        snapshot.frameCount = m_emu.frameCount();
        snapshot.regionFps = m_emu.getRegionFPS();
        snapshot.region = m_emu.region();
        snapshot.port1Device = m_emu.getPortDevice(Settings::Port::P_1);
        snapshot.port2Device = m_emu.getPortDevice(Settings::Port::P_2);
        snapshot.expansionDevice = m_emu.getExpansionDevice();
        snapshot.nesMultitapDevice = m_emu.getNesMultitapDevice();
        snapshot.famicomMultitapDevice = m_emu.getFamicomMultitapDevice();
        snapshot.nsfTotalSongs = m_emu.nsfTotalSongs();
        snapshot.nsfCurrentSong = m_emu.nsfCurrentSong();
        snapshot.audioDeviceName = m_audioOutput.currentDeviceName();
        snapshot.audioDevices = m_audioOutput.getAudioList();
        snapshot.audioVolume = m_audioOutput.getVolume();
        snapshot.audioChannelsJson = m_audioOutput.getAudioChannelsJson();

        const int backIndex = 1 - m_frontFramebufferIndex.load(std::memory_order_relaxed);
        std::memcpy(
            m_framebuffers[backIndex].data(),
            m_emu.getFramebuffer(),
            m_framebuffers[backIndex].size() * sizeof(uint32_t)
        );

        {
            std::scoped_lock snapshotLock(m_snapshotMutex);
            m_snapshot = snapshot;
        }
        m_frontFramebufferIndex.store(backIndex, std::memory_order_release);
    }

    void notifyWorkerStepComplete()
    {
        {
            std::scoped_lock stepLock(m_workerStepMutex);
            ++m_workerStepSerial;
        }
        m_workerStepCv.notify_all();
    }

    void workerLoop(std::stop_token stopToken)
    {
        using clock = std::chrono::steady_clock;
        constexpr uint32_t STEP_MS = 1;
        constexpr uint32_t MAX_CATCHUP_STEPS = 16;

        move_to_current_thread();
        {
            std::scoped_lock readyLock(m_workerReadyMutex);
            m_workerReady = true;
        }
        m_workerReadyCv.notify_all();

        auto nextTick = clock::now();

        while(!stopToken.stop_requested()) {
            if(m_framePacingMode.load(std::memory_order_acquire) == FramePacingMode::Suspended) {
                {
                    std::unique_lock presenterLock(m_presenterMutex);
                    m_presenterCv.wait_for(presenterLock, std::chrono::milliseconds(20), [&]() {
                        return stopToken.stop_requested() ||
                            m_framePacingMode.load(std::memory_order_acquire) != FramePacingMode::Suspended ||
                            m_workerWakeRequested.load(std::memory_order_acquire);
                    });
                }

                {
                    std::scoped_lock emuLock(m_emuMutex);
                    dispatch_queued_calls();
                    m_workerWakeRequested.store(false, std::memory_order_release);
                    refreshSnapshotLocked();
                    notifyWorkerStepComplete();
                }

                nextTick = clock::now();
                continue;
            }

            if(m_framePacingMode.load(std::memory_order_acquire) == FramePacingMode::PresenterLocked) {
                const uint32_t dtMs = std::max<uint32_t>(1, m_presenterTickDtMs.load(std::memory_order_acquire));
                const auto waitTimeout = std::chrono::milliseconds(std::clamp<uint32_t>(dtMs, 1, 20));
                bool timedOutWaitingPresenter = false;
                {
                    std::unique_lock presenterLock(m_presenterMutex);
                    if(!m_presenterCv.wait_for(presenterLock, waitTimeout, [&]() {
                            return stopToken.stop_requested() ||
                                m_framePacingMode.load(std::memory_order_acquire) != FramePacingMode::PresenterLocked ||
                                m_pendingPresenterTicks.load(std::memory_order_acquire) > 0 ||
                                m_workerWakeRequested.load(std::memory_order_acquire);
                        })) {
                        timedOutWaitingPresenter = true;
                    }
                }

                {
                    std::scoped_lock emuLock(m_emuMutex);
                    dispatch_queued_calls();

                    const bool wakeOnly = m_workerWakeRequested.exchange(false, std::memory_order_acq_rel);
                    const uint32_t ticksToConsume = m_pendingPresenterTicks.exchange(0, std::memory_order_acq_rel);

                    if(m_emu.valid() && ticksToConsume > 0) {
                        m_emu.updateUntilFrame(dtMs);
                        flushPendingNetplaySnapshotLocked();
                    }
                    else if(m_emu.valid() && timedOutWaitingPresenter && !wakeOnly) {
                        // If the presenter/UI stalls (e.g. native window move/resize modal loop),
                        // temporarily keep emulation/audio flowing with internal pacing until
                        // presentation ticks resume.
                        m_emu.update(dtMs);
                        flushPendingNetplaySnapshotLocked();
                    }

                    (void)wakeOnly;
                    refreshSnapshotLocked();
                    notifyWorkerStepComplete();
                }

                nextTick = clock::now();
                continue;
            }

            auto now = clock::now();
            if(now < nextTick) {
                std::this_thread::sleep_until(nextTick);
                continue;
            }

            uint32_t catchupSteps = 0;
            while(now >= nextTick && catchupSteps < MAX_CATCHUP_STEPS && !stopToken.stop_requested()) {
                ++catchupSteps;
                nextTick += std::chrono::milliseconds(STEP_MS);

                {
                    std::scoped_lock emuLock(m_emuMutex);
                    dispatch_queued_calls();
                    if(m_emu.valid()) {
                        m_emu.update(STEP_MS);
                        flushPendingNetplaySnapshotLocked();
                        refreshSnapshotLocked();
                        notifyWorkerStepComplete();
                    } else {
                        refreshSnapshotLocked();
                        notifyWorkerStepComplete();
                    }
                }
            }

            if(catchupSteps == MAX_CATCHUP_STEPS) {
                nextTick = clock::now();
            }
        }
    }
#endif

public:
    explicit EmulationHost(IAudioOutput& audioOutput)
        : m_emu(audioOutput)
        , m_audioOutput(audioOutput)
    {
#ifdef __EMSCRIPTEN__
        m_emu.signalFrameStart.bind(&EmulationHost::applyPendingInput, this);
        m_emu.signalFrameReady.bind(&EmulationHost::requestNetplaySnapshotOnFrameReady, this);
#else
        m_emu.signalFrameStart.bind(&EmulationHost::applyPendingInputLocked, this);
        m_emu.signalFrameReady.bind(&EmulationHost::requestNetplaySnapshotOnFrameReady, this);
        m_signalCommand.bind_auto(&EmulationHost::onCommand, this);
        {
            std::scoped_lock emuLock(m_emuMutex);
            refreshSnapshotLocked();
        }
        m_workerThread = std::jthread([this](std::stop_token stopToken) {
            workerLoop(stopToken);
        });
        {
            std::unique_lock readyLock(m_workerReadyMutex);
            m_workerReadyCv.wait(readyLock, [this]() { return m_workerReady; });
        }
#endif
    }

    ~EmulationHost()
    {
        shutdown();
    }

    void shutdown()
    {
#ifdef __EMSCRIPTEN__
        return;
#else
        if(m_shutdownStarted.exchange(true)) {
            return;
        }

        if(m_workerThread.joinable()) {
            m_workerThread.request_stop();
            m_presenterCv.notify_all();
            m_workerThread.join();
        }
#endif
    }

    void setPendingInput(const InputState& input)
    {
#ifdef __EMSCRIPTEN__
        m_pendingInput = input;
#else
        {
            std::scoped_lock pendingInputLock(m_pendingInputMutex);
            m_pendingInput = input;
        }
        m_workerWakeRequested.store(true, std::memory_order_release);
        m_presenterCv.notify_one();
#endif
    }

    void setFrameInputProvider(FrameInputProvider provider)
    {
#ifdef __EMSCRIPTEN__
        (void)provider;
#else
        {
            std::scoped_lock providerLock(m_frameInputProviderMutex);
            m_frameInputProvider = std::move(provider);
        }
        m_lastFrameProviderInput.reset();
        m_workerWakeRequested.store(true, std::memory_order_release);
        m_presenterCv.notify_one();
#endif
    }

    void setRepeatLastFrameProviderInput(bool enabled)
    {
#ifdef __EMSCRIPTEN__
        (void)enabled;
#else
        {
            std::scoped_lock providerLock(m_frameInputProviderMutex);
            m_repeatLastFrameProviderInput = enabled;
        }
        if(!enabled) {
            m_lastFrameProviderInput.reset();
        }
        m_workerWakeRequested.store(true, std::memory_order_release);
        m_presenterCv.notify_one();
#endif
    }

    void resetFrameInputProviderState()
    {
#ifdef __EMSCRIPTEN__
        return;
#else
        m_lastFrameProviderInput.reset();
#endif
    }

    void postCommand(std::function<void(GeraNESEmu&)> command)
    {
#ifdef __EMSCRIPTEN__
        if(command) command(m_emu);
#else
        m_workerWakeRequested.store(true, std::memory_order_release);
        m_signalCommand(std::move(command));
        m_presenterCv.notify_one();
#endif
    }

    template<typename Fn>
    decltype(auto) withExclusiveAccess(Fn&& fn)
    {
#ifdef __EMSCRIPTEN__
        return std::forward<Fn>(fn)(m_emu);
#else
        std::scoped_lock emuLock(m_emuMutex);
        if constexpr(std::is_void_v<std::invoke_result_t<Fn, GeraNESEmu&>>) {
            std::forward<Fn>(fn)(m_emu);
            refreshSnapshotLocked();
        } else {
            return std::forward<Fn>(fn)(m_emu);
        }
#endif
    }

    template<typename Fn>
    decltype(auto) withExclusiveAccess(Fn&& fn) const
    {
#ifdef __EMSCRIPTEN__
        return std::forward<Fn>(fn)(m_emu);
#else
        std::scoped_lock emuLock(m_emuMutex);
        return std::forward<Fn>(fn)(m_emu);
#endif
    }

    bool open(const std::string& path)
    {
#ifdef __EMSCRIPTEN__
        resetNetplayRuntimeLocked();
        return m_emu.open(path);
#else
        std::scoped_lock emuLock(m_emuMutex);
        resetNetplayRuntimeLocked();
        const bool opened = m_emu.open(path);
        refreshSnapshotLocked();
        return opened;
#endif
    }

    std::vector<std::string> getAudioList() const
    {
#ifdef __EMSCRIPTEN__
        return m_audioOutput.getAudioList();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioDevices;
#endif
    }

    std::string currentAudioDeviceName() const
    {
#ifdef __EMSCRIPTEN__
        return m_audioOutput.currentDeviceName();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioDeviceName;
#endif
    }

    float getAudioVolume() const
    {
#ifdef __EMSCRIPTEN__
        return m_audioOutput.getVolume();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioVolume;
#endif
    }

    std::string getAudioChannelsJson() const
    {
#ifdef __EMSCRIPTEN__
        return m_audioOutput.getAudioChannelsJson();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioChannelsJson;
#endif
    }

    void configAudioDevice(const std::string& deviceName)
    {
#ifdef __EMSCRIPTEN__
        m_audioOutput.config(deviceName);
#else
        postCommand([deviceName, this](GeraNESEmu&) {
            m_audioOutput.config(deviceName);
        });
#endif
    }

    void restartAudio()
    {
#ifdef __EMSCRIPTEN__
        m_audioOutput.restart();
#else
        postCommand([this](GeraNESEmu&) {
            m_audioOutput.restart();
        });
#endif
    }

    void setAudioVolume(float volume)
    {
#ifdef __EMSCRIPTEN__
        m_audioOutput.setVolume(volume);
#else
        postCommand([volume, this](GeraNESEmu&) {
            m_audioOutput.setVolume(volume);
        });
#endif
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume)
    {
#ifdef __EMSCRIPTEN__
        return m_audioOutput.setAudioChannelVolumeById(id, volume);
#else
        postCommand([id, volume, this](GeraNESEmu&) {
            m_audioOutput.setAudioChannelVolumeById(id, volume);
        });
        return true;
#endif
    }

    bool valid() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.valid();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.valid;
#endif
    }

    void setupRewindSystem(bool enabled, int maxSeconds)
    {
#ifdef __EMSCRIPTEN__
        m_emu.setupRewindSystem(enabled, maxSeconds);
#else
        postCommand([=](GeraNESEmu& emu) {
            emu.setupRewindSystem(enabled, maxSeconds);
        });
#endif
    }

    void disableSpriteLimit(bool disabled)
    {
#ifdef __EMSCRIPTEN__
        m_emu.disableSpriteLimit(disabled);
#else
        postCommand([=](GeraNESEmu& emu) {
            emu.disableSpriteLimit(disabled);
        });
#endif
    }

    bool spriteLimitDisabled() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.spriteLimitDisabled();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.spriteLimitDisabled;
#endif
    }

    void enableOverclock(bool enabled)
    {
#ifdef __EMSCRIPTEN__
        m_emu.enableOverclock(enabled);
#else
        postCommand([=](GeraNESEmu& emu) {
            emu.enableOverclock(enabled);
        });
#endif
    }

    bool overclocked() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.overclocked();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.overclocked;
#endif
    }

    Settings::Region region() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.region();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.region;
#endif
    }

    void setRegion(Settings::Region region)
    {
#ifdef __EMSCRIPTEN__
        if(!m_emu.valid()) return;
        m_emu.setRegion(region);
#else
        postCommand([=](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.setRegion(region);
        });
#endif
    }

    bool paused() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.paused();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.paused;
#endif
    }

    void togglePaused()
    {
#ifdef __EMSCRIPTEN__
        if(!m_emu.valid()) return;
        m_emu.togglePaused();
#else
        postCommand([](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.togglePaused();
        });
#endif
    }

    void reset()
    {
#ifdef __EMSCRIPTEN__
        if(!m_emu.valid()) return;
        resetNetplayRuntimeLocked();
        m_emu.reset();
#else
        postCommand([](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.reset();
        });
        resetNetplayRuntimeLocked();
#endif
    }

    void saveState()
    {
        postCommand([](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.saveState();
        });
    }

    void loadState()
    {
        postCommand([](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.loadState();
        });
        resetNetplayRuntimeLocked();
    }

    std::optional<Settings::Device> getPortDevice(Settings::Port port) const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.getPortDevice(port);
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return port == Settings::Port::P_1 ? m_snapshot.port1Device : m_snapshot.port2Device;
#endif
    }

    void setPortDevice(Settings::Port port, Settings::Device device)
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setPortDevice(port, device);
        });
    }

    Settings::ExpansionDevice getExpansionDevice() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.getExpansionDevice();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.expansionDevice;
#endif
    }

    Settings::NesMultitapDevice getNesMultitapDevice() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.getNesMultitapDevice();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nesMultitapDevice;
#endif
    }

    Settings::FamicomMultitapDevice getFamicomMultitapDevice() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.getFamicomMultitapDevice();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.famicomMultitapDevice;
#endif
    }

    void setExpansionDevice(Settings::ExpansionDevice device)
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setExpansionDevice(device);
        });
    }

    void setNesMultitapDevice(Settings::NesMultitapDevice device)
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setNesMultitapDevice(device);
        });
    }

    void setFamicomMultitapDevice(Settings::FamicomMultitapDevice device)
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setFamicomMultitapDevice(device);
        });
    }

    void fdsSwitchDiskSide() { postCommand([](GeraNESEmu& emu) { emu.fdsSwitchDiskSide(); }); }
    void fdsEjectDisk() { postCommand([](GeraNESEmu& emu) { emu.fdsEjectDisk(); }); }
    void fdsInsertNextDisk() { postCommand([](GeraNESEmu& emu) { emu.fdsInsertNextDisk(); }); }
    void vsInsertCoin(int slot) { postCommand([=](GeraNESEmu& emu) { emu.vsInsertCoin(slot); }); }
    void vsServiceButton(int button) { postCommand([=](GeraNESEmu& emu) { emu.vsServiceButton(button); }); }

    bool isNsfLoaded() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.isNsfLoaded();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfLoaded;
#endif
    }

    bool nsfIsPlaying() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.nsfIsPlaying();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfPlaying;
#endif
    }

    bool nsfIsPaused() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.nsfIsPaused();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfPaused;
#endif
    }

    bool nsfHasEnded() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.nsfHasEnded();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfEnded;
#endif
    }

    int nsfTotalSongs() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.nsfTotalSongs();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfTotalSongs;
#endif
    }

    int nsfCurrentSong() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.nsfCurrentSong();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfCurrentSong;
#endif
    }

    void nsfPlay() { postCommand([](GeraNESEmu& emu) { emu.nsfPlay(); }); }
    void nsfPause() { postCommand([](GeraNESEmu& emu) { emu.nsfPause(); }); }
    void nsfStop() { postCommand([](GeraNESEmu& emu) { emu.nsfStop(); }); }
    void nsfNextSong() { postCommand([](GeraNESEmu& emu) { emu.nsfNextSong(); }); }
    void nsfPrevSong() { postCommand([](GeraNESEmu& emu) { emu.nsfPrevSong(); }); }
    void nsfSetSong(int song1Based) { postCommand([=](GeraNESEmu& emu) { emu.nsfSetSong(song1Based); }); }

    bool isRewinding() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.isRewinding();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.rewinding;
#endif
    }

    uint32_t frameCount() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.frameCount();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.frameCount;
#endif
    }

    uint32_t getRegionFPS() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.getRegionFPS();
#else
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.regionFps;
#endif
    }

    const uint32_t* getFramebuffer() const
    {
#ifdef __EMSCRIPTEN__
        return m_emu.getFramebuffer();
#else
        return m_framebuffers[m_frontFramebufferIndex.load(std::memory_order_acquire)].data();
#endif
    }

    void setPresenterLockActive(bool active)
    {
#ifdef __EMSCRIPTEN__
        (void)active;
#else
        if(active) {
            m_framePacingMode.store(FramePacingMode::PresenterLocked, std::memory_order_release);
        } else {
            m_framePacingMode.store(FramePacingMode::FreeRunning, std::memory_order_release);
            m_pendingPresenterTicks.store(0, std::memory_order_release);
        }
        m_presenterCv.notify_one();
#endif
    }

    void setSimulationSuspended(bool suspended)
    {
#ifdef __EMSCRIPTEN__
        (void)suspended;
#else
        if(suspended) {
            m_framePacingMode.store(FramePacingMode::Suspended, std::memory_order_release);
            m_pendingPresenterTicks.store(0, std::memory_order_release);
        } else if(m_framePacingMode.load(std::memory_order_acquire) == FramePacingMode::Suspended) {
            m_framePacingMode.store(FramePacingMode::FreeRunning, std::memory_order_release);
        }
        m_presenterCv.notify_one();
#endif
    }

    bool update(uint32_t dt)
    {
#ifdef __EMSCRIPTEN__
        const uint32_t previousFrame = m_emu.frameCount();
        const bool advanced = m_emu.update(dt);
        if(m_emu.frameCount() != previousFrame) {
            flushPendingNetplaySnapshotLocked();
        }
        return advanced;
#else
        (void)dt;
        if(m_framePacingMode.load(std::memory_order_acquire) != FramePacingMode::Suspended) {
            m_framePacingMode.store(FramePacingMode::FreeRunning, std::memory_order_release);
        }
        m_pendingPresenterTicks.store(0, std::memory_order_release);
        m_presenterCv.notify_one();
        return true;
#endif
    }

    void updateUntilFrame(uint32_t dt)
    {
#ifdef __EMSCRIPTEN__
        const uint32_t previousFrame = m_emu.frameCount();
        m_emu.updateUntilFrame(dt);
        if(m_emu.frameCount() != previousFrame) {
            flushPendingNetplaySnapshotLocked();
        }
#else
        const uint32_t initialFrame = [&]() {
            std::scoped_lock snapshotLock(m_snapshotMutex);
            return m_snapshot.frameCount;
        }();
        const uint64_t initialStepSerial = [&]() {
            std::scoped_lock stepLock(m_workerStepMutex);
            return m_workerStepSerial;
        }();
        m_presenterTickDtMs.store(std::max<uint32_t>(1, dt), std::memory_order_release);
        m_framePacingMode.store(FramePacingMode::PresenterLocked, std::memory_order_release);
        m_pendingPresenterTicks.store(1, std::memory_order_release);
        m_presenterCv.notify_one();
        std::unique_lock stepLock(m_workerStepMutex);
        m_workerStepCv.wait_for(stepLock, std::chrono::milliseconds(50), [&]() {
            if(m_workerStepSerial == initialStepSerial) {
                return false;
            }

            std::scoped_lock snapshotLock(m_snapshotMutex);
            return m_snapshot.frameCount != initialFrame;
        });
#endif
    }

    void configureNetplaySnapshots(size_t snapshotCapacity)
    {
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        m_netplayRuntimeEnabled = snapshotCapacity > 0;
        m_netplayRuntime.reset();
        m_netplayRuntime.configureRollbackWindow(snapshotCapacity);
    }

    bool rollbackToFrame(uint32_t frame)
    {
#ifdef __EMSCRIPTEN__
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        if(!m_netplayRuntimeEnabled) return false;
        return m_netplayRuntime.rollbackTo(frame, [this](const std::vector<uint8_t>& data) {
            m_emu.loadStateFromMemoryOnCleanBoot(data);
        });
#else
        std::scoped_lock emuLock(m_emuMutex);
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        if(!m_netplayRuntimeEnabled) return false;

        const bool rolledBack = m_netplayRuntime.rollbackTo(frame, [this](const std::vector<uint8_t>& data) {
            m_emu.loadStateFromMemoryOnCleanBoot(data);
        });

        if(rolledBack) {
            resetFrameInputProviderState();
            refreshSnapshotLocked();
        }

        return rolledBack;
#endif
    }

    std::vector<uint8_t> saveStateToMemory()
    {
#ifdef __EMSCRIPTEN__
        return m_emu.saveStateToMemory();
#else
        std::scoped_lock emuLock(m_emuMutex);
        return m_emu.saveStateToMemory();
#endif
    }

    std::vector<uint8_t> saveNetplayStateToMemory()
    {
#ifdef __EMSCRIPTEN__
        return m_emu.saveNetplayStateToMemory();
#else
        std::scoped_lock emuLock(m_emuMutex);
        return m_emu.saveNetplayStateToMemory();
#endif
    }

    bool loadStateFromMemory(const std::vector<uint8_t>& data)
    {
#ifdef __EMSCRIPTEN__
        if(data.empty()) return false;
        resetNetplayRuntimeLocked();
        return m_emu.loadStateFromMemoryOnCleanBoot(data);
#else
        if(data.empty()) return false;
        std::scoped_lock emuLock(m_emuMutex);
        resetNetplayRuntimeLocked();
        if(!m_emu.loadStateFromMemoryOnCleanBoot(data)) {
            return false;
        }
        resetFrameInputProviderState();
        refreshSnapshotLocked();
        return true;
#endif
    }

    template<typename InputProvider>
    bool resimulateToFrame(uint32_t targetFrame, InputProvider&& inputProvider)
    {
#ifdef __EMSCRIPTEN__
        if(!m_emu.valid()) return false;

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, m_emu.getRegionFPS()));
        InputState lastReplayInput{};
        bool hasLastReplayInput = false;
        while(m_emu.frameCount() < targetFrame) {
            const uint32_t nextFrame = m_emu.frameCount() + 1;
            m_pendingInput = std::forward<InputProvider>(inputProvider)(nextFrame);
            lastReplayInput = m_pendingInput;
            hasLastReplayInput = true;
            applyPendingInput();
            m_emu.updateUntilFrame(frameDt);
            flushPendingNetplaySnapshotLocked();
        }
        if(hasLastReplayInput) {
            m_pendingInput = lastReplayInput;
        }
        return true;
#else
        std::scoped_lock emuLock(m_emuMutex);
        if(!m_emu.valid()) return false;

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, m_emu.getRegionFPS()));
        FrameInputProvider savedFrameInputProvider;
        std::optional<InputState> savedLastFrameProviderInput;
        {
            std::scoped_lock providerLock(m_frameInputProviderMutex);
            savedFrameInputProvider = m_frameInputProvider;
            savedLastFrameProviderInput = m_lastFrameProviderInput;
            m_frameInputProvider = nullptr;
            m_lastFrameProviderInput.reset();
        }
        InputState lastReplayInput{};
        bool hasLastReplayInput = false;
        while(m_emu.frameCount() < targetFrame) {
            const uint32_t nextFrame = m_emu.frameCount() + 1;
            const InputState replayInput = std::forward<InputProvider>(inputProvider)(nextFrame);
            {
                std::scoped_lock pendingInputLock(m_pendingInputMutex);
                m_pendingInput = replayInput;
            }
            lastReplayInput = replayInput;
            hasLastReplayInput = true;
            m_emu.updateUntilFrame(frameDt);
            flushPendingNetplaySnapshotLocked();
        }

        {
            std::scoped_lock providerLock(m_frameInputProviderMutex);
            m_frameInputProvider = std::move(savedFrameInputProvider);
            m_lastFrameProviderInput = hasLastReplayInput ? std::optional<InputState>{lastReplayInput} : savedLastFrameProviderInput;
        }
        if(hasLastReplayInput) {
            std::scoped_lock pendingInputLock(m_pendingInputMutex);
            m_pendingInput = lastReplayInput;
        }

        refreshSnapshotLocked();
        return true;
#endif
    }

    uint32_t canonicalStateCrc32()
    {
#ifdef __EMSCRIPTEN__
        return m_emu.canonicalStateCrc32();
#else
        std::scoped_lock emuLock(m_emuMutex);
        return m_emu.canonicalStateCrc32();
#endif
    }

    uint32_t canonicalNetplayStateCrc32()
    {
#ifdef __EMSCRIPTEN__
        return m_emu.canonicalNetplayStateCrc32();
#else
        std::scoped_lock emuLock(m_emuMutex);
        return m_emu.canonicalNetplayStateCrc32();
#endif
    }

    std::optional<std::vector<uint8_t>> netplaySnapshotForFrame(uint32_t frame) const
    {
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        if(!m_netplayRuntimeEnabled) return std::nullopt;

        const Netplay::SnapshotRecord* record = m_netplayRuntime.snapshots().find(frame);
        if(record == nullptr) return std::nullopt;
        return record->data;
    }

    std::optional<uint32_t> netplaySnapshotCrc32ForFrame(uint32_t frame) const
    {
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        if(!m_netplayRuntimeEnabled) return std::nullopt;
        return m_netplayRuntime.snapshots().crc32ForFrame(frame);
    }

    void seedNetplaySnapshot(uint32_t frame, const std::vector<uint8_t>& data)
    {
        if(data.empty()) return;

        std::scoped_lock netplayLock(m_netplayRuntimeMutex);
        if(!m_netplayRuntimeEnabled) return;
        m_netplayRuntime.setCurrentFrame(frame);
        m_netplayRuntime.captureSnapshot(frame, [&data]() {
            return data;
        });
    }

    NetplayDiagnosticsSnapshot getNetplayDiagnostics() const
    {
        std::scoped_lock netplayLock(m_netplayRuntimeMutex);

        NetplayDiagnosticsSnapshot snapshot;
        snapshot.enabled = m_netplayRuntimeEnabled;
        snapshot.currentFrame = m_netplayRuntime.currentFrame();
        snapshot.snapshotCapacity = m_netplayRuntime.snapshots().capacity();
        snapshot.storedSnapshots = m_netplayRuntime.snapshots().size();
        snapshot.rollbackStats = m_netplayRuntime.stats();

        if(const Netplay::SnapshotRecord* latest = m_netplayRuntime.snapshots().latest()) {
            snapshot.latestSnapshotCrc32 = latest->crc32;
        }

        return snapshot;
    }
};
