#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ConsoleNetplay/NetplayRuntimeTypes.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/InputState.h"
#include "GeraNES/InputTopology.h"
#include "signal/signal.h"

struct EmulationHostTypes
{
    using InputState = ::InputState;

    struct ReplayFrameInput
    {
        InputState state{};
        bool rewind = false;
        bool hasFrameOverride = false;
        InputFrame frameOverride{};
    };

    struct RuntimeControls
    {
        bool rewind = false;
    };

    using FrameInputResolver = std::function<bool(uint32_t, ReplayFrameInput&)>;
    using QueuedInputObserver = std::function<void(const InputFrame&)>;
    using SelectedInputObserver = std::function<void(const InputFrame&)>;
    using ReplaySnapshotObserver = std::function<void(uint32_t, std::vector<uint8_t>)>;
    struct ReplayPlaybackStatus
    {
        bool loaded = false;
        bool playing = false;
        uint32_t cursorFrame = 0;
        uint32_t loadedFrameCount = 0;
        std::optional<uint32_t> cursorCanonicalCrc32;
    };
    struct ModRenderSnapshot;
    using ModFrameCaptureHook = std::function<bool(GeraNESEmu&, ModRenderSnapshot&, std::vector<uint32_t>&)>;

    using InputTopology = ::InputTopology;
    using InputTopologySnapshot = ::InputTopology;

    struct PpuViewerSnapshot
    {
        bool valid = false;
        uint32_t frameCount = 0;
        int ppuScanline = 0;
        int ppuCycle = 0;
        std::array<uint8_t, 0x2000> chrData = {};
        std::array<uint8_t, 0x1000> nametableData = {};
        std::array<uint8_t, 0x20> paletteData = {};
        std::array<uint32_t, 64> rgbPalette = {};
        int scrollX = 0;
        int scrollY = 0;
        int backgroundPatternTableAddress = 0x0000;
    };

    struct PpuEventViewerSnapshot
    {
        bool valid = false;
        bool traceEnabled = false;
        uint32_t frameCount = 0;
        std::vector<GeraNESEmu::PpuRegisterAccessEvent> events;
        std::vector<uint32_t> framebuffer;
    };

    struct ModRenderSnapshot
    {
        struct FrameConditionState
        {
            uint32_t frameCount = 0;
            std::vector<uint32_t> memoryValues;
        };

        bool valid = false;
        uint32_t frameCount = 0;
        int scale = 1;
        int scrollX = 0;
        int scrollY = 0;
        std::array<int, PPU::SCREEN_HEIGHT> scrollXByLine = {};
        std::array<int, PPU::SCREEN_HEIGHT> scrollYByLine = {};
        uint8_t universalBgColor = 0;
        std::array<uint32_t, 64> paletteColors = {};
        std::array<uint32_t, 512> tileHashes = {};
        std::vector<PPU::DebugModBackgroundPixel> backgroundPixels;
        std::vector<PPU::DebugModSpritePixel> spritePixels;
        FrameConditionState frameConditionState;
    };
};

class IEmulationHost : public SigSlot::SigSlotBase
{
public:
    using InputState = EmulationHostTypes::InputState;
    using ReplayFrameInput = EmulationHostTypes::ReplayFrameInput;
    using RuntimeControls = EmulationHostTypes::RuntimeControls;
    using FrameInputResolver = EmulationHostTypes::FrameInputResolver;
    using QueuedInputObserver = EmulationHostTypes::QueuedInputObserver;
    using SelectedInputObserver = EmulationHostTypes::SelectedInputObserver;
    using ReplaySnapshotObserver = EmulationHostTypes::ReplaySnapshotObserver;
    using ReplayPlaybackStatus = EmulationHostTypes::ReplayPlaybackStatus;
    using ModFrameCaptureHook = EmulationHostTypes::ModFrameCaptureHook;
    using NetplayDiagnosticsSnapshot = ConsoleNetplay::NetplayRuntimeDiagnostics;
    using ManualStateChangeKind = ConsoleNetplay::NetplayManualStateChangeKind;
    using ManualStateChangeRecord = ConsoleNetplay::NetplayManualStateChangeRecord;
    using InputTopology = EmulationHostTypes::InputTopology;
    using InputTopologySnapshot = EmulationHostTypes::InputTopologySnapshot;
    using ModRenderSnapshot = EmulationHostTypes::ModRenderSnapshot;

    virtual ~IEmulationHost() = default;

    virtual void shutdown() = 0;
    virtual void setPendingInput(const InputState& input) = 0;
    virtual InputState pendingInputSnapshot() const = 0;
    virtual void setPendingRuntimeControls(const RuntimeControls& controls) = 0;
    virtual RuntimeControls pendingRuntimeControlsSnapshot() const = 0;
    virtual void setFrameInputResolver(FrameInputResolver resolver) = 0;
    virtual void setQueuedInputObserver(QueuedInputObserver observer) = 0;
    virtual void setSelectedInputObserver(SelectedInputObserver observer) = 0;
    virtual void queueReplayInputFrame(const InputFrame& frame) = 0;
    virtual void queueInputForFrame(uint32_t frameNumber, const InputState& input) = 0;
    virtual void queueInputFrames(const std::vector<std::pair<uint32_t, InputState>>& inputs) = 0;
    virtual void setAutoQueuePendingInputOnFrameStart(bool enabled) = 0;
    virtual void setAllowPresenterTimeoutAdvance(bool enabled) = 0;
    virtual void setPreAdvanceHook(std::function<void(GeraNESEmu&)> hook) = 0;
    virtual void setModFrameCaptureHook(ModFrameCaptureHook hook) = 0;
    virtual void postCommand(std::function<void(GeraNESEmu&)> command) = 0;
    virtual bool open(const std::string& path, bool autoConfigureInputTopologyOnRomLoad = true) = 0;
    virtual std::vector<std::string> getAudioList() const = 0;
    virtual IAudioOutput::AudioFormatOptions getAudioFormatOptions(const std::string& deviceName) const = 0;
    virtual std::string currentAudioDeviceName() const = 0;
    virtual int currentAudioSampleRate() const = 0;
    virtual int currentAudioSampleSize() const = 0;
    virtual float getAudioVolume() const = 0;
    virtual std::string getAudioChannelsJson() const = 0;
    virtual void setAudioPlaybackSpeed(double speed) = 0;
    virtual void setForceSkipAudioRender(bool skip) = 0;
    virtual void configAudioDevice(const std::string& deviceName) = 0;
    virtual void configAudioDevice(const std::string& deviceName, int sampleRate, int sampleSize) = 0;
    virtual void restartAudio() = 0;
    virtual void discardQueuedAudio() = 0;
    virtual void discardQueuedNetplayInputsAfter(uint32_t frame) = 0;
    virtual std::vector<ManualStateChangeRecord> consumeManualStateChanges() = 0;
    virtual void setAudioVolume(float volume) = 0;
    virtual bool setAudioChannelVolumeById(const std::string& id, float volume) = 0;
    virtual void setColorPalette(const std::array<uint32_t, 64>& palette) = 0;
    virtual bool valid() const = 0;
    virtual void setupRewindSystem(bool enabled, int maxSeconds) = 0;
    virtual void disableSpriteLimit(bool disabled) = 0;
    virtual bool spriteLimitDisabled() const = 0;
    virtual void enableOverclock(bool enabled) = 0;
    virtual bool overclocked() const = 0;
    virtual Settings::Region region() const = 0;
    virtual void setRegion(Settings::Region region) = 0;
    virtual bool paused() const = 0;
    virtual void togglePaused() = 0;
    virtual void reset() = 0;
    virtual void closeRom() = 0;
    virtual void saveState(uint8_t slot = 0) = 0;
    virtual void loadState(uint8_t slot = 0) = 0;
    virtual std::optional<Settings::Device> getPortDevice(Settings::Port port) const = 0;
    virtual Settings::ExpansionDevice getExpansionDevice() const = 0;
    virtual Settings::NesMultitapDevice getNesMultitapDevice() const = 0;
    virtual Settings::FamicomMultitapDevice getFamicomMultitapDevice() const = 0;
    virtual InputTopology getInputTopologySnapshot() const = 0;
    virtual GameDatabase::System currentCartridgeSystem() const = 0;
    virtual void fdsSwitchDiskSide() = 0;
    virtual void fdsEjectDisk() = 0;
    virtual void fdsInsertNextDisk() = 0;
    virtual void vsInsertCoin(int slot) = 0;
    virtual void vsServiceButton(int button) = 0;
    virtual bool isNsfLoaded() const = 0;
    virtual bool nsfIsPlaying() const = 0;
    virtual bool nsfIsPaused() const = 0;
    virtual bool nsfHasEnded() const = 0;
    virtual int nsfTotalSongs() const = 0;
    virtual int nsfCurrentSong() const = 0;
    virtual void nsfPlay() = 0;
    virtual void nsfPause() = 0;
    virtual void nsfStop() = 0;
    virtual void nsfNextSong() = 0;
    virtual void nsfPrevSong() = 0;
    virtual void nsfSetSong(int song1Based) = 0;
    virtual bool isRewinding() const = 0;
    virtual uint32_t frameCount() const = 0;
    virtual uint32_t exactEmulationFrame() const = 0;
    virtual uint32_t getRegionFPS() const = 0;
    virtual void configureInputBufferCapacity(size_t capacity) = 0;
    virtual uint32_t inputTimelineEpoch() const = 0;
    virtual void setInputTimelineEpoch(uint32_t timelineEpoch) = 0;
    virtual void discardQueuedInputFramesAfter(uint32_t frame) = 0;
    virtual void clearQueuedInputFrames() = 0;
    virtual const uint32_t* getFramebuffer() const = 0;
    virtual void copyFramebuffer(std::vector<uint32_t>& out) const = 0;
    virtual bool getModRenderSnapshot(ModRenderSnapshot& out) const = 0;
    virtual bool getModRenderFrame(ModRenderSnapshot& snapshot, std::vector<uint32_t>& framebuffer) const = 0;
    virtual void beginPresentationHoldUntilNextFrameReady() = 0;
    virtual void setPresenterLockActive(bool active) = 0;
    virtual void setSimulationSuspended(bool suspended) = 0;
    virtual bool update(uint32_t dt) = 0;
    virtual void updateUntilFrame(uint32_t dt) = 0;
    virtual void configureNetplaySnapshots(size_t snapshotCapacity) = 0;
    virtual std::vector<uint8_t> saveStateToMemory() = 0;
    virtual bool loadStateFromMemory(const std::vector<uint8_t>& data) = 0;
    virtual bool loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& data) = 0;
    virtual bool loadStateFromMemoryAsManualStateChange(const std::vector<uint8_t>& data) = 0;
    virtual void loadReplayPlayback(const std::vector<InputFrame>& frames) = 0;
    virtual void clearReplayPlayback() = 0;
    virtual ReplayPlaybackStatus replayPlaybackStatus() const = 0;
    virtual bool replayPlay() = 0;
    virtual bool replayPause(bool pauseEmulation) = 0;
    virtual bool replaySeekToFrame(uint32_t frame) = 0;
    virtual bool replayStopToStart() = 0;
    virtual bool fastForwardReplayToFrame(uint32_t targetFrame,
                                          const std::vector<InputFrame>& replayFrames,
                                          std::optional<uint32_t> expectedCurrentStateCrc32,
                                          const std::vector<uint32_t>& snapshotFramesToCapture = {},
                                          ReplaySnapshotObserver snapshotObserver = {}) = 0;
    virtual uint32_t lastFrameReadyFrame() const = 0;
    virtual uint32_t lastFrameReadyNetplayCrc32() const = 0;
    virtual std::optional<std::shared_ptr<const std::vector<uint8_t>>> lastFrameReadyStateSnapshot() const = 0;
    virtual void setAuthoritativeFrameReadyState(uint32_t frame, uint32_t canonicalCrc32) = 0;
    virtual std::optional<std::shared_ptr<const std::vector<uint8_t>>> netplaySnapshotForFrame(uint32_t frame) const = 0;
    virtual std::optional<uint32_t> netplaySnapshotCrc32ForFrame(uint32_t frame) const = 0;
    virtual bool updateNetplaySnapshotCrc32ForFrame(uint32_t frame, uint32_t canonicalCrc32) = 0;
    virtual void seedNetplaySnapshot(uint32_t frame,
                                     const std::vector<uint8_t>& data,
                                     std::optional<uint32_t> canonicalCrc32 = std::nullopt) = 0;
    virtual NetplayDiagnosticsSnapshot getNetplayDiagnostics() const = 0;
};
