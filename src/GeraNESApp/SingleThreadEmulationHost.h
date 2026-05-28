#pragma once

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GeraNESApp/IEmulationHost.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/PPU.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif


class SingleThreadEmulationHost : public IEmulationHost
{
public:
    using InputState = IEmulationHost::InputState;
    using ReplayFrameInput = IEmulationHost::ReplayFrameInput;
    using FrameInputResolver = IEmulationHost::FrameInputResolver;
    using QueuedInputObserver = IEmulationHost::QueuedInputObserver;
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
        frame.setPortButtons(1, {input.p1A, input.p1B, input.p1Select, input.p1Start, input.p1Up, input.p1Down, input.p1Left, input.p1Right,
                                 input.p1X, input.p1Y, input.p1L, input.p1R, input.p1Up2, input.p1Down2, input.p1Left2, input.p1Right2});
        frame.setPortButtons(2, {input.p2A, input.p2B, input.p2Select, input.p2Start, input.p2Up, input.p2Down, input.p2Left, input.p2Right,
                                 input.p2X, input.p2Y, input.p2L, input.p2R, input.p2Up2, input.p2Down2, input.p2Left2, input.p2Right2});
        frame.setPortButtons(3, {input.p3A, input.p3B, input.p3Select, input.p3Start, input.p3Up, input.p3Down, input.p3Left, input.p3Right});
        frame.setPortButtons(4, {input.p4A, input.p4B, input.p4Select, input.p4Start, input.p4Up, input.p4Down, input.p4Left, input.p4Right});
        frame.setPowerPadButtons(1, input.p1PowerPadButtons);
        frame.setPowerPadButtons(2, input.p2PowerPadButtons);
        frame.setSuborKeyboardKeys(input.suborKeyboardKeys);
        frame.setFamilyBasicKeyboardKeys(input.familyBasicKeyboardKeys);
        frame.setBandaiButtons({input.p2A, input.p2B, input.p2Select, input.p2Start, input.p2Up, input.p2Down, input.p2Left, input.p2Right});
        frame.setZapper(1, {input.zapperX, input.zapperY, input.zapperP1Trigger});
        frame.setZapper(2, {input.zapperX, input.zapperY, input.zapperP2Trigger});
        frame.setBandaiPointer({input.zapperX, input.zapperY, input.bandaiTrigger});
        frame.setArkanoidController(1, {input.arkanoidNesPosition, input.mousePrimaryButton});
        frame.setArkanoidController(2, {input.arkanoidNesPosition, input.mousePrimaryButton});
        frame.setArkanoidExpansion({input.arkanoidFamicomPosition, input.mousePrimaryButton});
        frame.setKonamiHyperShot({input.konamiP1Run, input.konamiP1Jump, input.konamiP2Run, input.konamiP2Jump});
        frame.setSnesMouse(1, {input.mouseDeltaX, input.mouseDeltaY, input.mousePrimaryButton, input.mouseSecondaryButton});
        frame.setSnesMouse(2, {input.mouseDeltaX, input.mouseDeltaY, input.mousePrimaryButton, input.mouseSecondaryButton});
        frame.setSuborMouse(1, {input.mouseDeltaX, input.mouseDeltaY, input.mousePrimaryButton, input.mouseSecondaryButton});
        frame.setSuborMouse(2, {input.mouseDeltaX, input.mouseDeltaY, input.mousePrimaryButton, input.mouseSecondaryButton});
        return frame;
    }

    static InputFrame applyInputStateToEmu(GeraNESEmu& emu, const InputState& input)
    {
        InputFrame frame = buildInputFrameForEmu(emu, emu.frameCount() + 1u, input);
        emu.queueInputFrame(frame);
        emu.setRewind(input.rewind);
        emu.setSpeedBoost(input.speedBoost);
        return frame;
    }

    static InputFrame queueReplayFrameInputToEmu(GeraNESEmu& emu,
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
        return frame;
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
        float audioVolume = 1.0f;
        std::string audioChannelsJson = "{\"channels\":[]}";
    };

    struct NetplayStoredSnapshot
    {
        uint32_t frame = 0;
        uint32_t crc32 = 0;
        std::shared_ptr<std::vector<uint8_t>> data;
    };

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
    std::deque<ManualStateChangeRecord> m_manualStateChanges;
    std::deque<std::function<void(GeraNESEmu&)>> m_pendingCommands;

    void onResetExecutedLocked(uint32_t frame);
    void onLoadExecutedLocked(uint32_t frame);
    void recordFrameReadyNetplayState(GeraNESEmu& emu);
    std::optional<size_t> snapshotIndexForFrame(uint32_t frame) const;
    void discardNetplaySnapshotsAfter(uint32_t frame);

    enum class FramePacingMode : uint8_t
    {
        Suspended,
        FreeRunning,
        PresenterLocked
    };

    GeraNESEmu m_emu;
    IAudioOutput& m_audioOutput;
    bool m_holdPresentedFramebufferUntilFrameReady = false;
    std::vector<uint32_t> m_presentedFramebuffer =
        std::vector<uint32_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0);
    InputState m_pendingInput;
    FrameInputResolver m_frameInputResolver;
    QueuedInputObserver m_queuedInputObserver;
    bool m_autoQueuePendingInputOnFrameStart = true;
    FramePacingMode m_framePacingMode = FramePacingMode::FreeRunning;
    uint32_t m_presenterTickDtMs = 16;
    uint32_t m_pendingPresenterTicks = 0;
    bool m_allowPresenterTimeoutAdvance = true;
    bool m_freeRunningClockInitialized = false;
    std::chrono::steady_clock::time_point m_freeRunningNextTick{};
    std::function<void(GeraNESEmu&)> m_preAdvanceHook;
    ModFrameCaptureHook m_modFrameCaptureHook;
    PpuViewerSnapshot m_ppuViewerSnapshot;
    PpuEventViewerSnapshot m_ppuEventViewerSnapshot;
    ModRenderSnapshot m_modRenderSnapshot;
    std::vector<uint32_t> m_presentedModFramebuffer;
    bool m_ppuViewerCaptureEnabled = false;
    bool m_ppuViewerScanlineCaptureEnabled = false;
    bool m_ppuEventViewerCaptureEnabled = false;

    void resetFreeRunningPacing();
    void applyPendingInput();
    void notifyQueuedInputObserver(const InputFrame& frame);
    bool runPreAdvanceHook();
    void dispatchQueuedCommands();
    bool pumpFreeRunningWorkerSteps();
    void serviceBackgroundWork();
    void onFrameReady();
    void refreshPresentedFramebuffer();
    void refreshPpuViewerSnapshot();
    void refreshPpuEventViewerSnapshot();

public:
    explicit SingleThreadEmulationHost(IAudioOutput& audioOutput);
    ~SingleThreadEmulationHost();

    void shutdown() override;
    void setPendingInput(const InputState& input) override;
    InputState pendingInputSnapshot() const override
    {
        return m_pendingInput;
    }
    void setFrameInputResolver(FrameInputResolver resolver) override;
    void setQueuedInputObserver(QueuedInputObserver observer) override;
    void queueInputForFrame(uint32_t frameNumber, const InputState& input) override;
    void queueInputFrames(const std::vector<std::pair<uint32_t, InputState>>& inputs) override;
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
        dispatchQueuedCommands();
        if constexpr(std::is_void_v<std::invoke_result_t<Fn, GeraNESEmu&>>) {
            std::forward<Fn>(fn)(m_emu);
            refreshPresentedFramebuffer();
        } else {
            auto result = std::forward<Fn>(fn)(m_emu);
            refreshPresentedFramebuffer();
            return result;
        }
    }

    template<typename Fn>
    decltype(auto) withExclusiveAccess(Fn&& fn) const
    {
        return std::forward<Fn>(fn)(m_emu);
    }

    bool open(const std::string& path) override;

    std::vector<std::string> getAudioList() const override
    {
        return m_audioOutput.getAudioList();
    }

    IAudioOutput::AudioFormatOptions getAudioFormatOptions(const std::string& deviceName) const override
    {
        return m_audioOutput.getAudioFormatOptions(deviceName);
    }

    std::string currentAudioDeviceName() const override
    {
        return m_audioOutput.currentDeviceName();
    }

    int currentAudioSampleRate() const override
    {
        return m_audioOutput.currentSampleRate();
    }

    int currentAudioSampleSize() const override
    {
        return m_audioOutput.currentSampleSize();
    }

    float getAudioVolume() const override
    {
        return m_audioOutput.getVolume();
    }

    std::string getAudioChannelsJson() const override
    {
        return m_audioOutput.getAudioChannelsJson();
    }

    void configAudioDevice(const std::string& deviceName)
     override{
        m_audioOutput.config(deviceName);
    }

    void configAudioDevice(const std::string& deviceName, int sampleRate, int sampleSize)
     override{
        m_audioOutput.config(deviceName, sampleRate, sampleSize);
    }

    void restartAudio()
     override{
        m_audioOutput.restart();
    }

    void discardQueuedAudio()
     override{
        m_audioOutput.discardQueuedAudio();
        m_audioOutput.clearAudioBuffers();
    }

    void discardQueuedNetplayInputsAfter(uint32_t frame)
     override{
        dispatchQueuedCommands();
        m_emu.discardQueuedInputFramesAfter(frame);
        discardNetplaySnapshotsAfter(frame);
    }

    std::vector<ManualStateChangeRecord> consumeManualStateChanges()
     override{
        std::vector<ManualStateChangeRecord> events;
        events.assign(m_manualStateChanges.begin(), m_manualStateChanges.end());
        m_manualStateChanges.clear();
        return events;
    }

    void setAudioVolume(float volume)
     override{
        m_audioOutput.setVolume(volume);
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume)
     override{
        return m_audioOutput.setAudioChannelVolumeById(id, volume);
    }

    void setColorPalette(const std::array<uint32_t, 64>& palette) override
    {
        m_emu.getConsole().ppu().setColorPalette(palette);
        refreshPresentedFramebuffer();
    }

    bool valid() const override
    {
        return m_emu.valid();
    }

    void setupRewindSystem(bool enabled, int maxSeconds)
     override{
        m_emu.setupRewindSystem(enabled, maxSeconds);
    }

    void disableSpriteLimit(bool disabled)
     override{
        m_emu.disableSpriteLimit(disabled);
    }

    bool spriteLimitDisabled() const override
    {
        return m_emu.spriteLimitDisabled();
    }

    void enableOverclock(bool enabled)
     override{
        m_emu.enableOverclock(enabled);
    }

    bool overclocked() const override
    {
        return m_emu.overclocked();
    }

    Settings::Region region() const override
    {
        return m_emu.region();
    }

    void setRegion(Settings::Region region)
     override{
        if(!m_emu.valid()) return;
        m_emu.setRegion(region);
    }

    bool paused() const override
    {
        return m_emu.paused();
    }

    void togglePaused()
     override{
        if(!m_emu.valid()) return;
        m_emu.togglePaused();
    }

    void reset()
     override{
        m_holdPresentedFramebufferUntilFrameReady = true;
        postCommand([this](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            resetFreeRunningPacing();
            emu.reset();
        });
    }

    void closeRom() override
    {
        resetFreeRunningPacing();
        m_hasCachedNetplayCrc = false;
        m_emu.closeRom();
        refreshPresentedFramebuffer();
    }

    void saveState(uint8_t slot = 0)
     override{
        postCommand([slot](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.saveState(slot);
        });
    }

    void loadState(uint8_t slot = 0)
     override{
        m_holdPresentedFramebufferUntilFrameReady = true;
        postCommand([slot](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.loadState(slot);
        });
    }

    std::optional<Settings::Device> getPortDevice(Settings::Port port) const override
    {
        return m_emu.getPortDevice(port);
    }

    void setPortDevice(Settings::Port port, Settings::Device device)
     override{
        postCommand([=](GeraNESEmu& emu) {
            emu.setPortDevice(port, device);
        });
    }

    Settings::ExpansionDevice getExpansionDevice() const override
    {
        return m_emu.getExpansionDevice();
    }

    Settings::NesMultitapDevice getNesMultitapDevice() const override
    {
        return m_emu.getNesMultitapDevice();
    }

    Settings::FamicomMultitapDevice getFamicomMultitapDevice() const override
    {
        return m_emu.getFamicomMultitapDevice();
    }

    InputTopologySnapshot getInputTopologySnapshot() const override
    {
        InputTopologySnapshot snapshot;
        snapshot.port1Device = m_emu.getPortDevice(Settings::Port::P_1);
        snapshot.port2Device = m_emu.getPortDevice(Settings::Port::P_2);
        snapshot.expansionDevice = m_emu.getExpansionDevice();
        snapshot.nesMultitapDevice = m_emu.getNesMultitapDevice();
        snapshot.famicomMultitapDevice = m_emu.getFamicomMultitapDevice();
        return snapshot;
    }

    GameDatabase::System currentCartridgeSystem() const override
    {
        return m_emu.getConsole().cartridge().system();
    }

    void setExpansionDevice(Settings::ExpansionDevice device)
     override{
        postCommand([=](GeraNESEmu& emu) {
            emu.setExpansionDevice(device);
        });
    }

    void setNesMultitapDevice(Settings::NesMultitapDevice device)
     override{
        postCommand([=](GeraNESEmu& emu) {
            emu.setNesMultitapDevice(device);
        });
    }

    void setFamicomMultitapDevice(Settings::FamicomMultitapDevice device)
     override{
        postCommand([=](GeraNESEmu& emu) {
            emu.setFamicomMultitapDevice(device);
        });
    }

    void fdsSwitchDiskSide()  override{ postCommand([](GeraNESEmu& emu) { emu.fdsSwitchDiskSide(); }); }
    void fdsEjectDisk()  override{ postCommand([](GeraNESEmu& emu) { emu.fdsEjectDisk(); }); }
    void fdsInsertNextDisk()  override{ postCommand([](GeraNESEmu& emu) { emu.fdsInsertNextDisk(); }); }
    void vsInsertCoin(int slot)  override{ postCommand([=](GeraNESEmu& emu) { emu.vsInsertCoin(slot); }); }
    void vsServiceButton(int button)  override{ postCommand([=](GeraNESEmu& emu) { emu.vsServiceButton(button); }); }

    bool isNsfLoaded() const override
    {
        return m_emu.isNsfLoaded();
    }

    bool nsfIsPlaying() const override
    {
        return m_emu.nsfIsPlaying();
    }

    bool nsfIsPaused() const override
    {
        return m_emu.nsfIsPaused();
    }

    bool nsfHasEnded() const override
    {
        return m_emu.nsfHasEnded();
    }

    int nsfTotalSongs() const override
    {
        return m_emu.nsfTotalSongs();
    }

    int nsfCurrentSong() const override
    {
        return m_emu.nsfCurrentSong();
    }

    void nsfPlay()  override{ postCommand([](GeraNESEmu& emu) { emu.nsfPlay(); }); }
    void nsfPause()  override{ postCommand([](GeraNESEmu& emu) { emu.nsfPause(); }); }
    void nsfStop()  override{ postCommand([](GeraNESEmu& emu) { emu.nsfStop(); }); }
    void nsfNextSong()  override{ postCommand([](GeraNESEmu& emu) { emu.nsfNextSong(); }); }
    void nsfPrevSong()  override{ postCommand([](GeraNESEmu& emu) { emu.nsfPrevSong(); }); }
    void nsfSetSong(int song1Based)  override{ postCommand([=](GeraNESEmu& emu) { emu.nsfSetSong(song1Based); }); }

    bool isRewinding() const override
    {
        return m_emu.isRewinding();
    }

    uint32_t frameCount() const override
    {
        return m_emu.frameCount();
    }

    uint32_t manualResetGeneration() const override;
    uint32_t manualLoadStateGeneration() const override;
    uint32_t exactEmulationFrame() const override;
    uint32_t getRegionFPS() const override;
    void configureInputBufferCapacity(size_t capacity) override
    {
        m_emu.configureInputBufferCapacity(capacity);
    }

    uint32_t inputTimelineEpoch() const override
    {
        return m_emu.inputTimelineEpoch();
    }

    void setInputTimelineEpoch(uint32_t timelineEpoch) override
    {
        m_emu.setInputTimelineEpoch(timelineEpoch);
    }

    void discardQueuedInputFramesAfter(uint32_t frame) override
    {
        m_emu.discardQueuedInputFramesAfter(frame);
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
    bool fastForwardReplayToFrame(uint32_t targetFrame,
                                  const std::vector<InputFrame>& replayFrames,
                                  std::optional<uint32_t> expectedCurrentStateCrc32) override
    {
        if(expectedCurrentStateCrc32.has_value() && canonicalStateCrc32() != *expectedCurrentStateCrc32) {
            return false;
        }
        const FrameInputResolver previousResolver = m_frameInputResolver;
        m_frameInputResolver = [&replayFrames](uint32_t nextFrame, ReplayFrameInput& input) {
            if(nextFrame == 0 || nextFrame > replayFrames.size()) {
                return false;
            }
            input.hasFrameOverride = true;
            input.frameOverride = replayFrames[static_cast<size_t>(nextFrame - 1u)];
            return true;
        };

        const bool wasPaused = m_emu.paused();
        if(wasPaused) {
            m_emu.togglePaused();
        }

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, m_emu.getRegionFPS()));
        while(m_emu.valid() && m_emu.frameCount() < targetFrame) {
            m_emu.updateUntilFrame(frameDt, false);
        }

        if(wasPaused && !m_emu.paused()) {
            m_emu.togglePaused();
        }
        m_frameInputResolver = previousResolver;
        return m_emu.valid();
    }

    template<typename InputProvider>
    bool resimulateToFrame(uint32_t targetFrame, InputProvider&& inputProvider)
    {
        if(!m_emu.valid()) return false;

        const bool wasPaused = m_emu.paused();
        if(wasPaused) {
            m_emu.togglePaused();
        }

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, m_emu.getRegionFPS()));
        ReplayFrameInput lastReplayInput{};
        bool hasLastReplayInput = false;
        while(m_emu.frameCount() < targetFrame) {
            const uint32_t nextFrame = m_emu.frameCount() + 1;
            const ReplayFrameInput replayInput = std::forward<InputProvider>(inputProvider)(nextFrame);
            m_pendingInput = replayInput.state;
            queueReplayFrameInputToEmu(m_emu, nextFrame, replayInput);
            lastReplayInput = replayInput;
            hasLastReplayInput = true;
            m_emu.updateUntilFrame(frameDt, false);
        }
        if(hasLastReplayInput) {
            m_pendingInput = lastReplayInput.state;
        }
        if(wasPaused && !m_emu.paused()) {
            m_emu.togglePaused();
        }
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

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
