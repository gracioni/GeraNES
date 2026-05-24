#pragma once

#include <array>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GeraNESApp/IEmulationHost.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/PPU.h"


class ThreadedEmulationHost : public IEmulationHost
{
public:
    using InputState = IEmulationHost::InputState;
    using ReplayFrameInput = IEmulationHost::ReplayFrameInput;
    using FrameInputResolver = IEmulationHost::FrameInputResolver;
    using NetplayDiagnosticsSnapshot = IEmulationHost::NetplayDiagnosticsSnapshot;
    using ManualStateChangeKind = IEmulationHost::ManualStateChangeKind;
    using ManualStateChangeRecord = IEmulationHost::ManualStateChangeRecord;
    using InputTopologySnapshot = IEmulationHost::InputTopologySnapshot;
    using ModRenderSnapshot = IEmulationHost::ModRenderSnapshot;
    using ModFrameCaptureHook = EmulationHostTypes::ModFrameCaptureHook;
    using PpuViewerSnapshot = EmulationHostTypes::PpuViewerSnapshot;
    using PpuEventViewerSnapshot = EmulationHostTypes::PpuEventViewerSnapshot;

private:
    static InputFrame buildInputFrameForEmu(GeraNESEmu& emu,
                                            uint32_t frameNumber,
                                            const InputState& input)
    {
        InputFrame frame = emu.createInputFrame(frameNumber);
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
        return frame;
    }

    static void applyInputStateToEmu(GeraNESEmu& emu, const InputState& input)
    {
        InputFrame frame = buildInputFrameForEmu(emu, emu.frameCount() + 1u, input);
        emu.queueInputFrame(frame);
        emu.setRewind(input.rewind);
        emu.setSpeedBoost(input.speedBoost);
    }

    static void queueReplayFrameInputToEmu(GeraNESEmu& emu,
                                           uint32_t targetFrame,
                                           const ReplayFrameInput& input)
    {
        InputFrame frame = input.hasFrameOverride
            ? input.frameOverride
            : buildInputFrameForEmu(emu, targetFrame, input.state);
        frame.frame = targetFrame;
        frame.timelineEpoch = emu.inputTimelineEpoch();
        emu.queueInputFrame(frame);
        emu.setRewind(input.state.rewind);
        emu.setSpeedBoost(input.state.speedBoost);
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
        uint32_t inputTimelineEpoch = 0;
        uint32_t lastFrameReadyFrame = 0;
        uint32_t lastFrameReadyNetplayCrc32 = 0;
        uint32_t manualResetGeneration = 0;
        uint32_t manualLoadStateGeneration = 0;
        uint32_t regionFps = 60;
        Settings::Region region = Settings::Region::NTSC;
        GameDatabase::System cartridgeSystem = GameDatabase::System::Unknown;
        std::optional<Settings::Device> port1Device = Settings::Device::CONTROLLER;
        std::optional<Settings::Device> port2Device = Settings::Device::CONTROLLER;
        Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
        Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
        Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
        int nsfTotalSongs = 0;
        int nsfCurrentSong = 0;
        std::string audioDeviceName;
        std::vector<std::string> audioDevices;
        IAudioOutput::AudioFormatOptions audioFormatOptions;
        int audioSampleRate = 44100;
        int audioSampleSize = 16;
        float audioVolume = 1.0f;
        std::string audioChannelsJson = "{\"channels\":[]}";
    };

    struct NetplayStoredSnapshot
    {
        uint32_t frame = 0;
        uint32_t crc32 = 0;
        std::shared_ptr<std::vector<uint8_t>> data;
    };

    mutable std::mutex m_netplaySnapshotMutex;
    std::deque<NetplayStoredSnapshot> m_netplaySnapshots;
    std::unordered_map<uint32_t, uint64_t> m_netplaySnapshotIndexByFrame;
    uint64_t m_netplaySnapshotHeadPosition = 0;
    uint64_t m_netplaySnapshotNextPosition = 0;
    size_t m_netplaySnapshotCapacity = 0;
    mutable NetplayDiagnosticsSnapshot m_netplayDiagnostics;
    bool m_hasCachedNetplayCrc = false;
    uint32_t m_cachedNetplayCrcFrame = 0;
    uint32_t m_cachedNetplayCrcValue = 0;
    uint32_t m_lastFrameReadyFrameValue = 0;
    uint32_t m_lastFrameReadyNetplayCrc32Value = 0;
    mutable std::mutex m_manualStateChangeMutex;
    std::deque<ManualStateChangeRecord> m_manualStateChanges;
    void onResetExecutedLocked(uint32_t frame);
    void onLoadExecutedLocked(uint32_t frame);
    void recordFrameReadyNetplayState(GeraNESEmu& emu);
    std::optional<size_t> snapshotIndexForFrameLocked(uint32_t frame) const;
    void discardNetplaySnapshotsAfter(uint32_t frame);

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
    std::thread::id m_workerThreadId;
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
    mutable std::mutex m_framebufferMutex;
    std::atomic<bool> m_holdPresentedFramebufferUntilFrameReady{false};
    std::array<std::vector<uint32_t>, 2> m_framebuffers{
        std::vector<uint32_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0),
        std::vector<uint32_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0)
    };
    Snapshot m_snapshot;
    mutable std::mutex m_ppuViewerSnapshotMutex;
    PpuViewerSnapshot m_ppuViewerSnapshot;
    mutable std::mutex m_ppuEventViewerSnapshotMutex;
    PpuEventViewerSnapshot m_ppuEventViewerSnapshot;
    mutable std::mutex m_modRenderSnapshotMutex;
    ModRenderSnapshot m_modRenderSnapshot;
    std::vector<uint32_t> m_presentedModFramebuffer;
    mutable std::mutex m_modFrameCaptureHookMutex;
    ModFrameCaptureHook m_modFrameCaptureHook;
    bool m_ppuViewerCaptureEnabled = false;
    bool m_ppuViewerScanlineCaptureEnabled = false;
    bool m_ppuEventViewerCaptureEnabled = false;
    mutable std::mutex m_pendingInputMutex;
    InputState m_pendingInput;
    mutable std::mutex m_frameInputResolverMutex;
    FrameInputResolver m_frameInputResolver;
    std::atomic<bool> m_autoQueuePendingInputOnFrameStart{true};
    std::atomic<bool> m_allowPresenterTimeoutAdvance{true};
    mutable std::mutex m_preAdvanceHookMutex;
    std::function<void(GeraNESEmu&)> m_preAdvanceHook;
    bool m_snapshotConfigDirty = true;
    std::chrono::steady_clock::time_point m_lastSlowSnapshotRefresh = {};

    void runPreAdvanceHookLocked();
    void applyPendingInputLocked();
    void onCommand(std::function<void(GeraNESEmu&)> command);
    void refreshSnapshotLocked();
    void refreshPpuViewerSnapshotLocked(uint32_t frameCount);
    void refreshPpuEventViewerSnapshotLocked(uint32_t frameCount);
    void refreshModRenderSnapshotLocked();
    void onFrameReadyLocked();
    void workerLoop(std::stop_token stopToken);
    static thread_local const ThreadedEmulationHost* t_directAccessHost;
    bool onWorkerThread() const
    {
        return std::this_thread::get_id() == m_workerThreadId;
    }
    bool hasDirectEmuAccess() const
    {
        return onWorkerThread() || t_directAccessHost == this;
    }

public:
    explicit ThreadedEmulationHost(IAudioOutput& audioOutput);
    ~ThreadedEmulationHost();

    void shutdown() override;
    void setPendingInput(const InputState& input) override;
    void setFrameInputResolver(FrameInputResolver resolver) override;

    void queueInputForFrame(uint32_t frameNumber, const InputState& input) override
    {
        postCommand([frameNumber, input](GeraNESEmu& emu) {
            emu.queueInputFrame(buildInputFrameForEmu(emu, frameNumber, input));
        });
    }

    void queueInputFrames(const std::vector<std::pair<uint32_t, InputState>>& inputs) override
    {
        if(inputs.empty()) return;
        postCommand([inputs](GeraNESEmu& emu) {
            for(const auto& [frameNumber, input] : inputs) {
                emu.queueInputFrame(buildInputFrameForEmu(emu, frameNumber, input));
            }
        });
    }

    void setAutoQueuePendingInputOnFrameStart(bool enabled) override;
    void setAllowPresenterTimeoutAdvance(bool enabled) override;
    void setPreAdvanceHook(std::function<void(GeraNESEmu&)> hook) override;
    void setModFrameCaptureHook(ModFrameCaptureHook hook) override;
    void setDebugTraceSink(std::function<void(const std::string&)> sink);
    void postCommand(std::function<void(GeraNESEmu&)> command) override;
    void setPpuViewerCaptureEnabled(bool enabled, bool scanlineTrace = false);
    bool getPpuViewerSnapshot(PpuViewerSnapshot& out) const;
    void setPpuEventViewerCaptureEnabled(bool enabled);
    bool getPpuEventViewerSnapshot(PpuEventViewerSnapshot& out) const;

    template<typename Fn>
    decltype(auto) withExclusiveAccess(Fn&& fn)
    {
        std::scoped_lock emuLock(m_emuMutex);
        struct DirectAccessScope
        {
            const ThreadedEmulationHost* previous = nullptr;
            explicit DirectAccessScope(const ThreadedEmulationHost* host)
                : previous(t_directAccessHost)
            {
                t_directAccessHost = host;
            }
            ~DirectAccessScope()
            {
                t_directAccessHost = previous;
            }
        } directAccessScope(this);

        if constexpr(std::is_void_v<std::invoke_result_t<Fn, GeraNESEmu&>>) {
            std::forward<Fn>(fn)(m_emu);
            refreshSnapshotLocked();
        } else {
            return std::forward<Fn>(fn)(m_emu);
        }
    }

    template<typename Fn>
    decltype(auto) withExclusiveAccess(Fn&& fn) const
    {
        std::scoped_lock emuLock(m_emuMutex);
        struct DirectAccessScope
        {
            const ThreadedEmulationHost* previous = nullptr;
            explicit DirectAccessScope(const ThreadedEmulationHost* host)
                : previous(t_directAccessHost)
            {
                t_directAccessHost = host;
            }
            ~DirectAccessScope()
            {
                t_directAccessHost = previous;
            }
        } directAccessScope(this);

        return std::forward<Fn>(fn)(m_emu);
    }

    bool open(const std::string& path) override
    {
        std::scoped_lock emuLock(m_emuMutex);
        m_hasCachedNetplayCrc = false;
        const bool opened = m_emu.open(path);
        if(opened) {
            const uint32_t bootstrapFrame = m_emu.frameCount();
            ReplayFrameInput bootstrapInput;
            bool queuedBootstrap = false;
            {
                std::scoped_lock resolverLock(m_frameInputResolverMutex);
                if(m_frameInputResolver && m_frameInputResolver(bootstrapFrame, bootstrapInput)) {
                    queueReplayFrameInputToEmu(m_emu, bootstrapFrame, bootstrapInput);
                    queuedBootstrap = true;
                }
            }
            if(!queuedBootstrap) {
                InputState pendingInput;
                {
                    std::scoped_lock pendingInputLock(m_pendingInputMutex);
                    pendingInput = m_pendingInput;
                }
                InputFrame frame = buildInputFrameForEmu(m_emu, bootstrapFrame, pendingInput);
                m_emu.queueInputFrame(frame);
            }
        }
        refreshSnapshotLocked();
        return opened;
    }

    std::vector<std::string> getAudioList() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioDevices;
    }

    IAudioOutput::AudioFormatOptions getAudioFormatOptions(const std::string& deviceName) const override
    {
        (void)deviceName;
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioFormatOptions;
    }

    std::string currentAudioDeviceName() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioDeviceName;
    }

    int currentAudioSampleRate() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioSampleRate;
    }

    int currentAudioSampleSize() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioSampleSize;
    }

    float getAudioVolume() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioVolume;
    }

    std::string getAudioChannelsJson() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.audioChannelsJson;
    }

    void configAudioDevice(const std::string& deviceName) override
    {
        postCommand([deviceName, this](GeraNESEmu&) {
            m_audioOutput.config(deviceName);
            m_snapshotConfigDirty = true;
        });
    }

    void configAudioDevice(const std::string& deviceName, int sampleRate, int sampleSize) override
    {
        postCommand([deviceName, sampleRate, sampleSize, this](GeraNESEmu&) {
            m_audioOutput.config(deviceName, sampleRate, sampleSize);
            m_snapshotConfigDirty = true;
        });
    }

    void restartAudio() override
    {
        postCommand([this](GeraNESEmu&) {
            m_audioOutput.restart();
            m_snapshotConfigDirty = true;
        });
    }

    void discardQueuedAudio() override
    {
        postCommand([this](GeraNESEmu&) {
            m_audioOutput.discardQueuedAudio();
            m_audioOutput.clearAudioBuffers();
        });
    }

    void discardQueuedNetplayInputsAfter(uint32_t frame) override
    {
        postCommand([this, frame](GeraNESEmu& emu) {
            emu.discardQueuedInputFramesAfter(frame);
            discardNetplaySnapshotsAfter(frame);
        });
    }

    std::vector<ManualStateChangeRecord> consumeManualStateChanges() override
    {
        std::vector<ManualStateChangeRecord> events;
        std::scoped_lock eventLock(m_manualStateChangeMutex);
        events.assign(m_manualStateChanges.begin(), m_manualStateChanges.end());
        m_manualStateChanges.clear();
        return events;
    }

    void setAudioVolume(float volume) override
    {
        postCommand([volume, this](GeraNESEmu&) {
            m_audioOutput.setVolume(volume);
            m_snapshotConfigDirty = true;
        });
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        postCommand([id, volume, this](GeraNESEmu&) {
            m_audioOutput.setAudioChannelVolumeById(id, volume);
            m_snapshotConfigDirty = true;
        });
        return true;
    }

    void setColorPalette(const std::array<uint32_t, 64>& palette) override
    {
        postCommand([palette](GeraNESEmu& emu) {
            emu.getConsole().ppu().setColorPalette(palette);
        });
    }

    bool valid() const override
    {
        if(hasDirectEmuAccess()) {
            return m_emu.valid();
        }
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.valid;
    }

    void setupRewindSystem(bool enabled, int maxSeconds) override
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setupRewindSystem(enabled, maxSeconds);
        });
    }

    void disableSpriteLimit(bool disabled) override
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.disableSpriteLimit(disabled);
        });
    }

    bool spriteLimitDisabled() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.spriteLimitDisabled;
    }

    void enableOverclock(bool enabled) override
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.enableOverclock(enabled);
        });
    }

    bool overclocked() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.overclocked;
    }

    Settings::Region region() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.region;
    }

    void setRegion(Settings::Region region) override
    {
        postCommand([=](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.setRegion(region);
        });
    }

    bool paused() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.paused;
    }

    void togglePaused() override
    {
        postCommand([](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.togglePaused();
        });
    }

    void reset() override
    {
        m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
        postCommand([](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.reset();
        });
    }

    void closeRom() override
    {
        m_holdPresentedFramebufferUntilFrameReady.store(false, std::memory_order_release);
        postCommand([this](GeraNESEmu& emu) {
            m_hasCachedNetplayCrc = false;
            emu.close();
            {
                std::scoped_lock framebufferLock(m_framebufferMutex);
                for(auto& framebuffer : m_framebuffers) {
                    std::fill(framebuffer.begin(), framebuffer.end(), 0u);
                }
            }
            m_frontFramebufferIndex.store(0, std::memory_order_release);
        });
    }

    void saveState(uint8_t slot = 0) override
    {
        postCommand([slot](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.saveState(slot);
        });
    }

    void loadState(uint8_t slot = 0) override
    {
        m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
        postCommand([slot](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.loadState(slot);
        });
    }

    std::optional<Settings::Device> getPortDevice(Settings::Port port) const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return port == Settings::Port::P_1 ? m_snapshot.port1Device : m_snapshot.port2Device;
    }

    void setPortDevice(Settings::Port port, Settings::Device device) override
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setPortDevice(port, device);
        });
    }

    Settings::ExpansionDevice getExpansionDevice() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.expansionDevice;
    }

    Settings::NesMultitapDevice getNesMultitapDevice() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nesMultitapDevice;
    }

    Settings::FamicomMultitapDevice getFamicomMultitapDevice() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.famicomMultitapDevice;
    }

    InputTopologySnapshot getInputTopologySnapshot() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        InputTopologySnapshot snapshot;
        snapshot.port1Device = m_snapshot.port1Device;
        snapshot.port2Device = m_snapshot.port2Device;
        snapshot.expansionDevice = m_snapshot.expansionDevice;
        snapshot.nesMultitapDevice = m_snapshot.nesMultitapDevice;
        snapshot.famicomMultitapDevice = m_snapshot.famicomMultitapDevice;
        return snapshot;
    }

    GameDatabase::System currentCartridgeSystem() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.cartridgeSystem;
    }

    void setExpansionDevice(Settings::ExpansionDevice device) override
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setExpansionDevice(device);
        });
    }

    void setNesMultitapDevice(Settings::NesMultitapDevice device) override
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setNesMultitapDevice(device);
        });
    }

    void setFamicomMultitapDevice(Settings::FamicomMultitapDevice device) override
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setFamicomMultitapDevice(device);
        });
    }

    void fdsSwitchDiskSide() override { postCommand([](GeraNESEmu& emu) { emu.fdsSwitchDiskSide(); }); }
    void fdsEjectDisk() override { postCommand([](GeraNESEmu& emu) { emu.fdsEjectDisk(); }); }
    void fdsInsertNextDisk() override { postCommand([](GeraNESEmu& emu) { emu.fdsInsertNextDisk(); }); }
    void vsInsertCoin(int slot) override { postCommand([=](GeraNESEmu& emu) { emu.vsInsertCoin(slot); }); }
    void vsServiceButton(int button) override { postCommand([=](GeraNESEmu& emu) { emu.vsServiceButton(button); }); }

    bool isNsfLoaded() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfLoaded;
    }

    bool nsfIsPlaying() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfPlaying;
    }

    bool nsfIsPaused() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfPaused;
    }

    bool nsfHasEnded() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfEnded;
    }

    int nsfTotalSongs() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfTotalSongs;
    }

    int nsfCurrentSong() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.nsfCurrentSong;
    }

    void nsfPlay() override { postCommand([](GeraNESEmu& emu) { emu.nsfPlay(); }); }
    void nsfPause() override { postCommand([](GeraNESEmu& emu) { emu.nsfPause(); }); }
    void nsfStop() override { postCommand([](GeraNESEmu& emu) { emu.nsfStop(); }); }
    void nsfNextSong() override { postCommand([](GeraNESEmu& emu) { emu.nsfNextSong(); }); }
    void nsfPrevSong() override { postCommand([](GeraNESEmu& emu) { emu.nsfPrevSong(); }); }
    void nsfSetSong(int song1Based) override { postCommand([=](GeraNESEmu& emu) { emu.nsfSetSong(song1Based); }); }

    bool isRewinding() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.rewinding;
    }

    uint32_t frameCount() const override
    {
        if(hasDirectEmuAccess()) {
            return m_emu.frameCount();
        }
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.frameCount;
    }

    uint32_t manualResetGeneration() const override;
    uint32_t manualLoadStateGeneration() const override;
    uint32_t exactEmulationFrame() const override;
    uint32_t getRegionFPS() const override;
    void configureInputBufferCapacity(size_t capacity) override
    {
        if(hasDirectEmuAccess()) {
            m_emu.configureInputBufferCapacity(capacity);
            return;
        }
        postCommand([capacity](GeraNESEmu& emu) {
            emu.configureInputBufferCapacity(capacity);
        });
    }

    uint32_t inputTimelineEpoch() const override
    {
        if(hasDirectEmuAccess()) {
            return m_emu.inputTimelineEpoch();
        }
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.inputTimelineEpoch;
    }

    void setInputTimelineEpoch(uint32_t timelineEpoch) override
    {
        if(hasDirectEmuAccess()) {
            m_emu.setInputTimelineEpoch(timelineEpoch);
            refreshSnapshotLocked();
            return;
        }
        postCommand([timelineEpoch](GeraNESEmu& emu) {
            emu.setInputTimelineEpoch(timelineEpoch);
        });
    }

    void discardQueuedInputFramesAfter(uint32_t frame) override
    {
        if(hasDirectEmuAccess()) {
            m_emu.discardQueuedInputFramesAfter(frame);
            return;
        }
        postCommand([frame](GeraNESEmu& emu) {
            emu.discardQueuedInputFramesAfter(frame);
        });
    }

    const uint32_t* getFramebuffer() const override;
    void copyFramebuffer(std::vector<uint32_t>& out) const override;
    bool getModRenderSnapshot(ModRenderSnapshot& out) const override;
    bool getModRenderFrame(ModRenderSnapshot& snapshot, std::vector<uint32_t>& framebuffer) const override;
    void beginPresentationHoldUntilNextFrameReady() override;
    void setPresenterLockActive(bool active) override;
    void setSimulationSuspended(bool suspended) override;
    bool update(uint32_t dt) override;
    void updateUntilFrame(uint32_t dt) override;
    void configureNetplaySnapshots(size_t snapshotCapacity) override;
    std::vector<uint8_t> saveStateToMemory() override;
    std::vector<uint8_t> saveNetplayStateToMemory() override;
    bool loadStateFromMemory(const std::vector<uint8_t>& data) override;
    bool loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& data) override;
    bool loadStateFromMemoryAsManualStateChange(const std::vector<uint8_t>& data) override;

    template<typename InputProvider>
    bool resimulateToFrame(uint32_t targetFrame, InputProvider&& inputProvider)
    {
        std::scoped_lock emuLock(m_emuMutex);
        if(!m_emu.valid()) return false;

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, m_emu.getRegionFPS()));
        ReplayFrameInput lastReplayInput{};
        bool hasLastReplayInput = false;
        while(m_emu.frameCount() < targetFrame) {
            const uint32_t nextFrame = m_emu.frameCount() + 1;
            const ReplayFrameInput replayInput = std::forward<InputProvider>(inputProvider)(nextFrame);
            {
                std::scoped_lock pendingInputLock(m_pendingInputMutex);
                m_pendingInput = replayInput.state;
            }
            queueReplayFrameInputToEmu(m_emu, nextFrame, replayInput);
            lastReplayInput = replayInput;
            hasLastReplayInput = true;
            m_emu.updateUntilFrame(frameDt);
        }

        if(hasLastReplayInput) {
            std::scoped_lock pendingInputLock(m_pendingInputMutex);
            m_pendingInput = lastReplayInput.state;
        }

        refreshSnapshotLocked();
        return true;
    }

    uint32_t canonicalStateCrc32() override;
    uint32_t canonicalNetplayStateCrc32() override;
    uint32_t lastFrameReadyFrame() const override;
    uint32_t lastFrameReadyNetplayCrc32() const override;
    void setAuthoritativeFrameReadyState(uint32_t frame, uint32_t canonicalCrc32) override;
    std::optional<std::shared_ptr<const std::vector<uint8_t>>> netplaySnapshotForFrame(uint32_t frame) const override;
    std::optional<uint32_t> netplaySnapshotCrc32ForFrame(uint32_t frame) const override;
    bool updateNetplaySnapshotCrc32ForFrame(uint32_t frame, uint32_t canonicalCrc32) override;
    void seedNetplaySnapshot(uint32_t frame,
                             const std::vector<uint8_t>& data,
                             std::optional<uint32_t> canonicalCrc32 = std::nullopt) override;
    NetplayDiagnosticsSnapshot getNetplayDiagnostics() const override;
};
