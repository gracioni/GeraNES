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
#include "GeraNESApp/PendingInputFrames.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/PPU.h"


class ThreadedEmulationHost : public IEmulationHost
{
public:
    using InputState = IEmulationHost::InputState;
    using InputTopology = IEmulationHost::InputTopology;
    using ReplayFrameInput = IEmulationHost::ReplayFrameInput;
    using FrameInputResolver = IEmulationHost::FrameInputResolver;
    using QueuedInputObserver = IEmulationHost::QueuedInputObserver;
    using SelectedInputObserver = IEmulationHost::SelectedInputObserver;
    using ReplaySnapshotObserver = IEmulationHost::ReplaySnapshotObserver;
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
        frame.state.serializedInputData = input.serializedInputData;
        return frame;
    }

    static uint32_t replayFrameCountFromFrames(const std::vector<InputFrame>& frames)
    {
        uint32_t maxFramePlusOne = 0u;
        for(const InputFrame& frame : frames) {
            maxFramePlusOne = std::max(maxFramePlusOne, frame.frame + 1u);
        }
        return maxFramePlusOne;
    }

    static const InputFrame* findReplayFrameByNumber(const std::vector<InputFrame>& frames, uint32_t targetFrame)
    {
        if(targetFrame < frames.size()) {
            const InputFrame& indexed = frames[static_cast<size_t>(targetFrame)];
            if(indexed.frame == targetFrame) {
                return &indexed;
            }
        }

        const auto it = std::lower_bound(
            frames.begin(),
            frames.end(),
            targetFrame,
            [](const InputFrame& frame, uint32_t frameNumber) {
                return frame.frame < frameNumber;
            });
        if(it == frames.end() || it->frame != targetFrame) {
            return nullptr;
        }
        return &*it;
    }

    static std::optional<InputFrame> queueReplayFrameInputToEmu(GeraNESEmu& emu,
                                                                uint32_t targetFrame,
                                                                const ReplayFrameInput& input)
    {
        InputFrame frame = input.hasFrameOverride
            ? input.frameOverride
            : buildInputFrameForEmu(emu, targetFrame, input.state);
        frame.frame = targetFrame;
        if(!emu.setPlaybackInputFrame(frame)) {
            return std::nullopt;
        }
        emu.setRewind(input.rewind);
        return frame;
    }

    std::optional<InputFrame> queueResolvedOrPendingInputLocked(uint32_t targetFrame)
    {
        if(m_emu.hasPlaybackInputFrame(targetFrame)) {
            return std::nullopt;
        }

        ReplayFrameInput input;
        bool hasResolvedInput = false;
        bool handledByReplayPlayback = false;
        if(!resolveReplayPlaybackInputLocked(targetFrame, input, handledByReplayPlayback)) {
            return std::nullopt;
        }
        hasResolvedInput = handledByReplayPlayback;
        {
            std::scoped_lock resolverLock(m_frameInputResolverMutex);
            if(!hasResolvedInput && m_frameInputResolver) {
                if(!m_frameInputResolver(targetFrame, input)) {
                    return std::nullopt;
                }
                hasResolvedInput = true;
            }
        }

        if(!hasResolvedInput) {
            std::scoped_lock pendingInputLock(m_pendingInputMutex);
            if(const std::optional<InputFrame> queuedFrame = m_pendingInputFrames.take(targetFrame);
               queuedFrame.has_value()) {
                input.hasFrameOverride = true;
                input.frameOverride = *queuedFrame;
            } else if(!m_autoQueuePendingInputOnFrameStart.load(std::memory_order_acquire)) {
                return std::nullopt;
            } else {
                input.state = m_pendingInput;
                input.rewind = m_pendingRuntimeControls.rewind;
            }
        }

        return queueReplayFrameInputToEmu(m_emu, targetFrame, input);
    }

    std::optional<InputFrame> prepareCurrentFrameInputLocked()
    {
        const uint32_t targetFrame = m_emu.frameCount();
        if(m_emu.hasPlaybackInputFrame(targetFrame)) {
            return std::nullopt;
        }
        m_pendingInputFrames.eraseFramesBefore(targetFrame);
        const std::optional<InputFrame> queuedFrame =
            queueResolvedOrPendingInputLocked(targetFrame);
        if(queuedFrame.has_value()) {
            notifyQueuedInputObserverLocked(*queuedFrame);
            notifySelectedInputObserverLocked(*queuedFrame);
        }
        return queuedFrame;
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
        uint32_t regionFps = 60;
        bool replayLoaded = false;
        bool replayPlaying = false;
        uint32_t replayCursorFrame = 0;
        uint32_t replayLoadedFrameCount = 0;
        std::optional<uint32_t> replayCursorCanonicalCrc32;
        Settings::Region region = Settings::Region::NTSC;
        GameDatabase::System cartridgeSystem = GameDatabase::System::Unknown;
        InputTopology inputTopology = {};
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
    struct ReplayPlaybackState
    {
        bool loaded = false;
        bool playing = false;
        bool seeking = false;
        uint32_t cursorFrame = 0;
        std::optional<uint32_t> cursorCanonicalCrc32;
        std::vector<InputFrame> frames;
        struct StoredSnapshot
        {
            uint32_t frame = 0;
            uint32_t crc32 = 0;
            std::shared_ptr<std::vector<uint8_t>> data;
        };
        std::vector<uint32_t> scheduledSnapshotFrames;
        std::deque<StoredSnapshot> snapshots;
    };

    mutable std::mutex m_netplaySnapshotMutex;
    std::deque<NetplayStoredSnapshot> m_netplaySnapshots;
    std::unordered_map<uint32_t, uint64_t> m_netplaySnapshotIndexByFrame;
    uint64_t m_netplaySnapshotHeadPosition = 0;
    uint64_t m_netplaySnapshotNextPosition = 0;
    size_t m_netplaySnapshotCapacity = 0;
    mutable NetplayDiagnosticsSnapshot m_netplayDiagnostics;
    uint32_t m_lastFrameReadyFrameValue = 0;
    uint32_t m_lastFrameReadyNetplayCrc32Value = 0;
    std::shared_ptr<const std::vector<uint8_t>> m_lastFrameReadyStateSnapshot;
    ReplayPlaybackState m_replayPlayback;
    mutable std::mutex m_manualStateChangeMutex;
    std::deque<ManualStateChangeRecord> m_manualStateChanges;
    void onResetExecutedLocked(uint32_t frame);
    void onLoadExecutedLocked(uint32_t frame);
    void recordFrameReadyNetplayState(GeraNESEmu& emu);
    std::optional<size_t> snapshotIndexForFrameLocked(uint32_t frame) const;
    void discardNetplaySnapshotsAfter(uint32_t frame);
    bool resolveReplayPlaybackInputLocked(uint32_t targetFrame, ReplayFrameInput& input, bool& handled);
    void resetReplayPlaybackSnapshotsLocked();
    void captureReplayPlaybackSnapshotLocked(uint32_t frame, const std::vector<uint8_t>& state, uint32_t crc32);
    void updateReplayPlaybackFrameReadyStateLocked();
    bool seekReplayPlaybackLocked(uint32_t targetFrame);

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
    RuntimeControls m_pendingRuntimeControls;
    PendingInputFrames m_pendingInputFrames;
    uint32_t m_pendingInputTimelineEpoch = 0;
    mutable std::mutex m_frameInputResolverMutex;
    FrameInputResolver m_frameInputResolver;
    mutable std::mutex m_queuedInputObserverMutex;
    QueuedInputObserver m_queuedInputObserver;
    mutable std::mutex m_selectedInputObserverMutex;
    SelectedInputObserver m_selectedInputObserver;
    std::atomic<bool> m_autoQueuePendingInputOnFrameStart{true};
    std::atomic<bool> m_allowPresenterTimeoutAdvance{true};
    mutable std::mutex m_preAdvanceHookMutex;
    std::function<void(GeraNESEmu&)> m_preAdvanceHook;
    bool m_snapshotConfigDirty = true;
    std::chrono::steady_clock::time_point m_lastSlowSnapshotRefresh = {};

    void runPreAdvanceHookLocked();
    void notifyQueuedInputObserverLocked(const InputFrame& frame);
    void notifySelectedInputObserverLocked(const InputFrame& frame);
    void onCommand(std::function<void(GeraNESEmu&)> command);
    void refreshSnapshotLocked();
    void refreshPpuViewerSnapshotLocked(uint32_t frameCount);
    void refreshPpuEventViewerSnapshotLocked(uint32_t frameCount);
    void refreshModRenderSnapshotLocked();
    void publishPresentedStateLocked(bool recordNetplayState);
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
    InputState pendingInputSnapshot() const override
    {
        std::scoped_lock pendingInputLock(m_pendingInputMutex);
        return m_pendingInput;
    }
    void setPendingRuntimeControls(const RuntimeControls& controls) override
    {
        {
            std::scoped_lock pendingInputLock(m_pendingInputMutex);
            m_pendingRuntimeControls = controls;
        }
        m_workerWakeRequested.store(true, std::memory_order_release);
        m_presenterCv.notify_one();
    }
    RuntimeControls pendingRuntimeControlsSnapshot() const override
    {
        std::scoped_lock pendingInputLock(m_pendingInputMutex);
        return m_pendingRuntimeControls;
    }
    void setFrameInputResolver(FrameInputResolver resolver) override;
    void setQueuedInputObserver(QueuedInputObserver observer) override;
    void setSelectedInputObserver(SelectedInputObserver observer) override;
    void queueReplayInputFrame(const InputFrame& frame) override
    {
        if(hasDirectEmuAccess()) {
            InputFrame replayFrame = frame;
            m_pendingInputFrames.set(replayFrame);
            return;
        }
        postCommand([this, frame](GeraNESEmu& emu) {
            (void)emu;
            InputFrame replayFrame = frame;
            m_pendingInputFrames.set(replayFrame);
        });
    }

    void queueInputForFrame(uint32_t frameNumber, const InputState& input) override
    {
        postCommand([this, frameNumber, input](GeraNESEmu& emu) {
            m_pendingInputFrames.set(buildInputFrameForEmu(emu, frameNumber, input));
        });
    }

    void queueInputFrames(const std::vector<std::pair<uint32_t, InputState>>& inputs) override
    {
        if(inputs.empty()) return;
        postCommand([this, inputs](GeraNESEmu& emu) {
            for(const auto& [frameNumber, input] : inputs) {
                m_pendingInputFrames.set(buildInputFrameForEmu(emu, frameNumber, input));
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

    bool open(const std::string& path, bool autoConfigureInputTopologyOnRomLoad = true) override
    {
        std::scoped_lock emuLock(m_emuMutex);
        const bool opened = m_emu.openRom(path, autoConfigureInputTopologyOnRomLoad);
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

    void setAudioPlaybackSpeed(double speed) override
    {
        postCommand([speed, this](GeraNESEmu&) {
            m_audioOutput.setPlaybackSpeed(speed);
            m_audioOutput.discardQueuedAudio();
            m_audioOutput.clearAudioBuffers();
        });
    }

    void setForceSkipAudioRender(bool skip) override
    {
        postCommand([skip, this](GeraNESEmu& emu) {
            emu.setForceSkipAudioRender(skip);
            if(skip) {
                m_audioOutput.discardQueuedAudio();
                m_audioOutput.clearAudioBuffers();
            }
        });
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
            (void)emu;
            m_pendingInputFrames.eraseFramesAfter(frame);
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
            emu.closeRom();
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
        return port == Settings::Port::P_1
            ? m_snapshot.inputTopology.port1Device
            : m_snapshot.inputTopology.port2Device;
    }

    Settings::ExpansionDevice getExpansionDevice() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.inputTopology.expansionDevice;
    }

    Settings::NesMultitapDevice getNesMultitapDevice() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.inputTopology.nesMultitapDevice;
    }

    Settings::FamicomMultitapDevice getFamicomMultitapDevice() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.inputTopology.famicomMultitapDevice;
    }

    InputTopologySnapshot getInputTopologySnapshot() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.inputTopology;
    }

    GameDatabase::System currentCartridgeSystem() const override
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        return m_snapshot.cartridgeSystem;
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

    uint32_t exactEmulationFrame() const override;
    uint32_t getRegionFPS() const override;
    void configureInputBufferCapacity(size_t capacity) override
    {
        if(hasDirectEmuAccess()) {
            m_pendingInputFrames.reconfigureCapacity(capacity);
            return;
        }
        postCommand([this, capacity](GeraNESEmu& emu) {
            (void)emu;
            m_pendingInputFrames.reconfigureCapacity(capacity);
        });
    }

    uint32_t inputTimelineEpoch() const override
    {
        return m_pendingInputTimelineEpoch;
    }

    void setInputTimelineEpoch(uint32_t timelineEpoch) override
    {
        if(hasDirectEmuAccess()) {
            if(m_pendingInputTimelineEpoch != timelineEpoch) {
                m_pendingInputTimelineEpoch = timelineEpoch;
                m_pendingInputFrames.clear();
            }
            return;
        }
        postCommand([this, timelineEpoch](GeraNESEmu& emu) {
            (void)emu;
            if(m_pendingInputTimelineEpoch != timelineEpoch) {
                m_pendingInputTimelineEpoch = timelineEpoch;
                m_pendingInputFrames.clear();
            }
        });
    }

    void discardQueuedInputFramesAfter(uint32_t frame) override
    {
        if(hasDirectEmuAccess()) {
            m_pendingInputFrames.eraseFramesAfter(frame);
            return;
        }
        postCommand([this, frame](GeraNESEmu& emu) {
            (void)emu;
            m_pendingInputFrames.eraseFramesAfter(frame);
        });
    }

    void clearQueuedInputFrames() override
    {
        if(hasDirectEmuAccess()) {
            m_pendingInputFrames.clear();
            return;
        }
        postCommand([this](GeraNESEmu& emu) {
            (void)emu;
            m_pendingInputFrames.clear();
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
    bool loadStateFromMemory(const std::vector<uint8_t>& data) override;
    bool loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& data) override;
    bool loadStateFromMemoryAsManualStateChange(const std::vector<uint8_t>& data) override;
    void loadReplayPlayback(const std::vector<InputFrame>& frames) override
    {
        std::scoped_lock emuLock(m_emuMutex);
        m_replayPlayback.loaded = true;
        m_replayPlayback.playing = false;
        m_replayPlayback.seeking = false;
        m_replayPlayback.frames = frames;
        std::stable_sort(
            m_replayPlayback.frames.begin(),
            m_replayPlayback.frames.end(),
            [](const InputFrame& lhs, const InputFrame& rhs) {
                return lhs.frame < rhs.frame;
            });
        m_pendingInputFrames.clear();
        resetReplayPlaybackSnapshotsLocked();
        refreshSnapshotLocked();
    }
    void clearReplayPlayback() override
    {
        std::scoped_lock emuLock(m_emuMutex);
        m_replayPlayback = {};
        m_pendingInputFrames.clear();
        refreshSnapshotLocked();
    }
    ReplayPlaybackStatus replayPlaybackStatus() const override
    {
        ReplayPlaybackStatus status;
        if(hasDirectEmuAccess()) {
            status.loaded = m_replayPlayback.loaded;
            status.playing = m_replayPlayback.playing;
            status.cursorFrame = m_replayPlayback.cursorFrame;
            status.loadedFrameCount = replayFrameCountFromFrames(m_replayPlayback.frames);
            status.cursorCanonicalCrc32 = m_replayPlayback.cursorCanonicalCrc32;
            return status;
        }

        std::scoped_lock snapshotLock(m_snapshotMutex);
        status.loaded = m_snapshot.replayLoaded;
        status.playing = m_snapshot.replayPlaying;
        status.cursorFrame = m_snapshot.replayCursorFrame;
        status.loadedFrameCount = m_snapshot.replayLoadedFrameCount;
        status.cursorCanonicalCrc32 = m_snapshot.replayCursorCanonicalCrc32;
        return status;
    }
    bool replayPlay() override
    {
        std::scoped_lock emuLock(m_emuMutex);
        if(!m_replayPlayback.loaded || !m_emu.valid()) return false;
        m_pendingInputFrames.clear();
        m_replayPlayback.playing = true;
        m_replayPlayback.seeking = false;
        m_emu.setPaused(false);
        refreshSnapshotLocked();
        return true;
    }
    bool replayPause(bool pauseEmulation) override
    {
        std::scoped_lock emuLock(m_emuMutex);
        if(!m_replayPlayback.loaded) return false;
        m_replayPlayback.playing = false;
        m_replayPlayback.seeking = false;
        m_pendingInputFrames.clear();
        if(pauseEmulation && m_emu.valid()) {
            m_emu.setPaused(true);
        }
        m_replayPlayback.cursorFrame = m_emu.frameCount();
        m_replayPlayback.cursorCanonicalCrc32.reset();
        refreshSnapshotLocked();
        return true;
    }
    bool replaySeekToFrame(uint32_t frame) override
    {
        std::scoped_lock emuLock(m_emuMutex);
        if(!seekReplayPlaybackLocked(frame)) return false;
        refreshSnapshotLocked();
        return true;
    }
    bool replayStopToStart() override
    {
        return replaySeekToFrame(0);
    }
    bool fastForwardReplayToFrame(uint32_t targetFrame,
                                  const std::vector<InputFrame>& replayFrames,
                                  std::optional<uint32_t> expectedCurrentStateCrc32,
                                  const std::vector<uint32_t>& snapshotFramesToCapture = {},
                                  ReplaySnapshotObserver snapshotObserver = {}) override
    {
        if(expectedCurrentStateCrc32.has_value()) {
            std::vector<uint8_t> currentState;
            {
                std::scoped_lock emuLock(m_emuMutex);
                if(!m_emu.valid()) {
                    return false;
                }
                currentState = m_emu.saveStateToMemory();
            }
            const uint32_t currentStateCrc32 =
                currentState.empty()
                    ? 0u
                    : Crc32::calc(reinterpret_cast<const char*>(currentState.data()), currentState.size());
            if(currentStateCrc32 != *expectedCurrentStateCrc32) {
                return false;
            }
        }
        FrameInputResolver previousResolver;
        {
            std::scoped_lock resolverLock(m_frameInputResolverMutex);
            previousResolver = m_frameInputResolver;
            m_frameInputResolver = [&replayFrames](uint32_t targetFrame, ReplayFrameInput& input) {
                if(targetFrame >= replayFrames.size()) {
                    return false;
                }
                input.hasFrameOverride = true;
                input.frameOverride = replayFrames[static_cast<size_t>(targetFrame)];
                return true;
            };
        }

        std::scoped_lock emuLock(m_emuMutex);
        if(!m_emu.valid()) {
            std::scoped_lock resolverLock(m_frameInputResolverMutex);
            m_frameInputResolver = previousResolver;
            return false;
        }

        const bool wasPaused = m_emu.paused();
        if(wasPaused) {
            m_emu.togglePaused();
        }

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, m_emu.getRegionFPS()));
        size_t nextSnapshotIndex = 0;
        while(m_emu.frameCount() < targetFrame) {
            const uint32_t frameBefore = m_emu.frameCount();
            runPreAdvanceHookLocked();
            prepareCurrentFrameInputLocked();
            m_emu.updateUntilFrame(frameDt, false);
            if(m_emu.frameCount() <= frameBefore) {
                {
                    std::scoped_lock resolverLock(m_frameInputResolverMutex);
                    m_frameInputResolver = previousResolver;
                }
                refreshSnapshotLocked();
                return false;
            }
            onFrameReadyLocked();
            while(nextSnapshotIndex < snapshotFramesToCapture.size() &&
                  m_emu.frameCount() >= snapshotFramesToCapture[nextSnapshotIndex]) {
                if(snapshotObserver) {
                    snapshotObserver(snapshotFramesToCapture[nextSnapshotIndex], m_emu.saveStateToMemory());
                }
                ++nextSnapshotIndex;
            }
        }

        if(wasPaused && !m_emu.paused()) {
            m_emu.togglePaused();
        }

        {
            std::scoped_lock resolverLock(m_frameInputResolverMutex);
            m_frameInputResolver = previousResolver;
        }
        refreshSnapshotLocked();
        return true;
    }

    template<typename InputProvider>
    bool resimulateToFrame(uint32_t targetFrame, InputProvider&& inputProvider)
    {
        std::scoped_lock emuLock(m_emuMutex);
        if(!m_emu.valid()) return false;

        const bool wasPaused = m_emu.paused();
        if(wasPaused) {
            m_emu.togglePaused();
        }

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, m_emu.getRegionFPS()));
        ReplayFrameInput lastReplayInput{};
        bool hasLastReplayInput = false;
        while(m_emu.frameCount() < targetFrame) {
            const uint32_t currentFrame = m_emu.frameCount();
            runPreAdvanceHookLocked();
            const ReplayFrameInput replayInput = std::forward<InputProvider>(inputProvider)(currentFrame);
            {
                std::scoped_lock pendingInputLock(m_pendingInputMutex);
                m_pendingInput = replayInput.state;
            }
            const std::optional<InputFrame> queuedFrame = queueReplayFrameInputToEmu(m_emu, currentFrame, replayInput);
            if(!queuedFrame.has_value()) {
                return false;
            }
            notifyQueuedInputObserverLocked(*queuedFrame);
            notifySelectedInputObserverLocked(*queuedFrame);
            lastReplayInput = replayInput;
            hasLastReplayInput = true;
            const uint32_t frameBefore = m_emu.frameCount();
            m_emu.updateUntilFrame(frameDt, false);
            if(m_emu.frameCount() > frameBefore) {
                onFrameReadyLocked();
            }
        }

        if(hasLastReplayInput) {
            std::scoped_lock pendingInputLock(m_pendingInputMutex);
            m_pendingInput = lastReplayInput.state;
        }

        if(wasPaused && !m_emu.paused()) {
            m_emu.togglePaused();
        }

        refreshSnapshotLocked();
        return true;
    }

    uint32_t lastFrameReadyFrame() const override;
    uint32_t lastFrameReadyNetplayCrc32() const override;
    std::optional<std::shared_ptr<const std::vector<uint8_t>>> lastFrameReadyStateSnapshot() const override;
    void setAuthoritativeFrameReadyState(uint32_t frame, uint32_t canonicalCrc32) override;
    std::optional<std::shared_ptr<const std::vector<uint8_t>>> netplaySnapshotForFrame(uint32_t frame) const override;
    std::optional<uint32_t> netplaySnapshotCrc32ForFrame(uint32_t frame) const override;
    bool updateNetplaySnapshotCrc32ForFrame(uint32_t frame, uint32_t canonicalCrc32) override;
    void seedNetplaySnapshot(uint32_t frame,
                             const std::vector<uint8_t>& data,
                             std::optional<uint32_t> canonicalCrc32 = std::nullopt) override;
    NetplayDiagnosticsSnapshot getNetplayDiagnostics() const override;
};
