#pragma once

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "GeraNESApp/IEmulationHost.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/PPU.h"


class SingleThreadEmulationHost : public IEmulationHost
{
public:
    using InputState = IEmulationHost::InputState;
    using ReplayFrameInput = IEmulationHost::ReplayFrameInput;
    using FrameInputResolver = IEmulationHost::FrameInputResolver;
    using NetplayDiagnosticsSnapshot = IEmulationHost::NetplayDiagnosticsSnapshot;
    using ManualStateChangeKind = IEmulationHost::ManualStateChangeKind;
    using ManualStateChangeRecord = IEmulationHost::ManualStateChangeRecord;
    using InputTopologySnapshot = IEmulationHost::InputTopologySnapshot;

private:
    static InputFrame buildInputFrameForEmu(GeraNESEmu& emu,
                                            uint32_t frameNumber,
                                            const InputState& input,
                                            bool speculative = false)
    {
        InputFrame frame = emu.createInputFrame(frameNumber);
        frame.speculative = speculative;
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
            : buildInputFrameForEmu(emu, targetFrame, input.state, input.speculative);
        frame.frame = targetFrame;
        frame.speculative = input.speculative;
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
        std::vector<uint8_t> data;
    };

    std::deque<NetplayStoredSnapshot> m_netplaySnapshots;
    size_t m_netplaySnapshotCapacity = 0;
    NetplayDiagnosticsSnapshot m_netplayDiagnostics;
    uint32_t m_lastFrameReadyFrameValue = 0;
    uint32_t m_lastFrameReadyNetplayCrc32Value = 0;
    std::deque<ManualStateChangeRecord> m_manualStateChanges;

    void onResetExecutedLocked(uint32_t frame)
    {
        m_manualStateChanges.push_back(ManualStateChangeRecord{ManualStateChangeKind::Reset, frame});
    }

    void onLoadExecutedLocked(uint32_t frame)
    {
        m_manualStateChanges.push_back(ManualStateChangeRecord{ManualStateChangeKind::LoadState, frame});
    }

    void recordFrameReadyNetplayState(GeraNESEmu& emu)
    {
        const uint32_t frame = emu.frameCount();
        const uint32_t crc32 = emu.canonicalNetplayStateCrc32();
        m_lastFrameReadyFrameValue = frame;
        m_lastFrameReadyNetplayCrc32Value = crc32;

        if(m_netplaySnapshotCapacity == 0) {
            return;
        }

        std::vector<uint8_t> snapshotData = emu.saveNetplayRollbackStateToMemory();
        if(snapshotData.empty()) {
            return;
        }

        auto existing = std::find_if(
            m_netplaySnapshots.begin(),
            m_netplaySnapshots.end(),
            [frame](const NetplayStoredSnapshot& stored) { return stored.frame == frame; }
        );
        if(existing != m_netplaySnapshots.end()) {
            existing->crc32 = crc32;
            existing->data = std::move(snapshotData);
        } else {
            m_netplaySnapshots.push_back(NetplayStoredSnapshot{frame, crc32, std::move(snapshotData)});
            while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity) {
                m_netplaySnapshots.pop_front();
            }
        }

        m_netplayDiagnostics.enabled = true;
        m_netplayDiagnostics.currentFrame = frame;
        m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
        m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
        m_netplayDiagnostics.latestSnapshotCrc32 = crc32;
    }

    GeraNESEmu m_emu;
    IAudioOutput& m_audioOutput;
    InputState m_pendingInput;
    FrameInputResolver m_frameInputResolver;
    bool m_autoQueuePendingInputOnFrameStart = true;
    std::function<void(GeraNESEmu&)> m_preAdvanceHook;

    void applyPendingInput()
    {
        if(!m_autoQueuePendingInputOnFrameStart && !m_frameInputResolver) {
            return;
        }

        ReplayFrameInput input;
        if(m_frameInputResolver) {
            if(!m_frameInputResolver(m_emu.frameCount() + 1u, input)) {
                return;
            }
            m_emu.queueInputFrame(buildInputFrameForEmu(m_emu, m_emu.frameCount() + 1u, input.state, input.speculative));
            m_emu.setRewind(input.state.rewind);
            m_emu.setSpeedBoost(input.state.speedBoost);
            return;
        }

        applyInputStateToEmu(m_emu, m_pendingInput);
    }

    void runPreAdvanceHook()
    {
        if(m_preAdvanceHook) {
            m_preAdvanceHook(m_emu);
        }
    }

    void onFrameReady()
    {
        recordFrameReadyNetplayState(m_emu);
    }

public:
    explicit SingleThreadEmulationHost(IAudioOutput& audioOutput)
        : m_emu(audioOutput)
        , m_audioOutput(audioOutput)
    {
        m_emu.signalResetExecuted.bind(&SingleThreadEmulationHost::onResetExecutedLocked, this);
        m_emu.signalLoadExecuted.bind(&SingleThreadEmulationHost::onLoadExecutedLocked, this);
        m_emu.signalFrameStart.bind(&SingleThreadEmulationHost::applyPendingInput, this);
        m_emu.signalFrameReady.bind(&SingleThreadEmulationHost::onFrameReady, this);
    }

    ~SingleThreadEmulationHost()
    {
        shutdown();
    }

    void shutdown()
    {
        return;
    }

    void setPendingInput(const InputState& input)
    {
        m_pendingInput = input;
    }

    void setFrameInputResolver(FrameInputResolver resolver)
    {
        m_frameInputResolver = std::move(resolver);
    }

    void queueInputForFrame(uint32_t frameNumber, const InputState& input)
    {
        m_emu.queueInputFrame(buildInputFrameForEmu(m_emu, frameNumber, input));
    }

    void queueInputFrames(const std::vector<std::pair<uint32_t, InputState>>& inputs)
    {
        if(inputs.empty()) return;
        for(const auto& [frameNumber, input] : inputs) {
            m_emu.queueInputFrame(buildInputFrameForEmu(m_emu, frameNumber, input));
        }
    }

    void setAutoQueuePendingInputOnFrameStart(bool enabled)
    {
        m_autoQueuePendingInputOnFrameStart = enabled;
    }

    void setAllowPresenterTimeoutAdvance(bool enabled)
    {
        (void)enabled;
    }

    void setPreAdvanceHook(std::function<void(GeraNESEmu&)> hook)
    {
        m_preAdvanceHook = std::move(hook);
    }

    void postCommand(std::function<void(GeraNESEmu&)> command)
    {
        if(command) command(m_emu);
    }

    template<typename Fn>
    decltype(auto) withExclusiveAccess(Fn&& fn)
    {
        return std::forward<Fn>(fn)(m_emu);
    }

    template<typename Fn>
    decltype(auto) withExclusiveAccess(Fn&& fn) const
    {
        return std::forward<Fn>(fn)(m_emu);
    }

    bool open(const std::string& path)
    {
        return m_emu.open(path);
    }

    std::vector<std::string> getAudioList() const
    {
        return m_audioOutput.getAudioList();
    }

    std::string currentAudioDeviceName() const
    {
        return m_audioOutput.currentDeviceName();
    }

    float getAudioVolume() const
    {
        return m_audioOutput.getVolume();
    }

    std::string getAudioChannelsJson() const
    {
        return m_audioOutput.getAudioChannelsJson();
    }

    void configAudioDevice(const std::string& deviceName)
    {
        m_audioOutput.config(deviceName);
    }

    void restartAudio()
    {
        m_audioOutput.restart();
    }

    void discardQueuedAudio()
    {
        m_audioOutput.discardQueuedAudio();
        m_audioOutput.clearAudioBuffers();
    }

    void discardQueuedNetplayInputsAfter(uint32_t frame)
    {
        m_emu.discardQueuedInputFramesAfter(frame);
    }

    std::vector<ManualStateChangeRecord> consumeManualStateChanges()
    {
        std::vector<ManualStateChangeRecord> events;
        events.assign(m_manualStateChanges.begin(), m_manualStateChanges.end());
        m_manualStateChanges.clear();
        return events;
    }

    void setAudioVolume(float volume)
    {
        m_audioOutput.setVolume(volume);
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume)
    {
        return m_audioOutput.setAudioChannelVolumeById(id, volume);
    }

    bool valid() const
    {
        return m_emu.valid();
    }

    void setupRewindSystem(bool enabled, int maxSeconds)
    {
        m_emu.setupRewindSystem(enabled, maxSeconds);
    }

    void disableSpriteLimit(bool disabled)
    {
        m_emu.disableSpriteLimit(disabled);
    }

    bool spriteLimitDisabled() const
    {
        return m_emu.spriteLimitDisabled();
    }

    void enableOverclock(bool enabled)
    {
        m_emu.enableOverclock(enabled);
    }

    bool overclocked() const
    {
        return m_emu.overclocked();
    }

    Settings::Region region() const
    {
        return m_emu.region();
    }

    void setRegion(Settings::Region region)
    {
        if(!m_emu.valid()) return;
        m_emu.setRegion(region);
    }

    bool paused() const
    {
        return m_emu.paused();
    }

    void togglePaused()
    {
        if(!m_emu.valid()) return;
        m_emu.togglePaused();
    }

    void reset()
    {
        if(!m_emu.valid()) return;
        m_emu.reset();
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
    }

    std::optional<Settings::Device> getPortDevice(Settings::Port port) const
    {
        return m_emu.getPortDevice(port);
    }

    void setPortDevice(Settings::Port port, Settings::Device device)
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setPortDevice(port, device);
        });
    }

    Settings::ExpansionDevice getExpansionDevice() const
    {
        return m_emu.getExpansionDevice();
    }

    Settings::NesMultitapDevice getNesMultitapDevice() const
    {
        return m_emu.getNesMultitapDevice();
    }

    Settings::FamicomMultitapDevice getFamicomMultitapDevice() const
    {
        return m_emu.getFamicomMultitapDevice();
    }

    InputTopologySnapshot getInputTopologySnapshot() const
    {
        InputTopologySnapshot snapshot;
        snapshot.port1Device = m_emu.getPortDevice(Settings::Port::P_1);
        snapshot.port2Device = m_emu.getPortDevice(Settings::Port::P_2);
        snapshot.expansionDevice = m_emu.getExpansionDevice();
        snapshot.nesMultitapDevice = m_emu.getNesMultitapDevice();
        snapshot.famicomMultitapDevice = m_emu.getFamicomMultitapDevice();
        return snapshot;
    }

    GameDatabase::System currentCartridgeSystem() const
    {
        return m_emu.getConsole().cartridge().system();
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
        return m_emu.isNsfLoaded();
    }

    bool nsfIsPlaying() const
    {
        return m_emu.nsfIsPlaying();
    }

    bool nsfIsPaused() const
    {
        return m_emu.nsfIsPaused();
    }

    bool nsfHasEnded() const
    {
        return m_emu.nsfHasEnded();
    }

    int nsfTotalSongs() const
    {
        return m_emu.nsfTotalSongs();
    }

    int nsfCurrentSong() const
    {
        return m_emu.nsfCurrentSong();
    }

    void nsfPlay() { postCommand([](GeraNESEmu& emu) { emu.nsfPlay(); }); }
    void nsfPause() { postCommand([](GeraNESEmu& emu) { emu.nsfPause(); }); }
    void nsfStop() { postCommand([](GeraNESEmu& emu) { emu.nsfStop(); }); }
    void nsfNextSong() { postCommand([](GeraNESEmu& emu) { emu.nsfNextSong(); }); }
    void nsfPrevSong() { postCommand([](GeraNESEmu& emu) { emu.nsfPrevSong(); }); }
    void nsfSetSong(int song1Based) { postCommand([=](GeraNESEmu& emu) { emu.nsfSetSong(song1Based); }); }

    bool isRewinding() const
    {
        return m_emu.isRewinding();
    }

    uint32_t frameCount() const
    {
        return m_emu.frameCount();
    }

    uint32_t manualResetGeneration() const
    {
        return m_emu.manualResetGeneration();
    }

    uint32_t manualLoadStateGeneration() const
    {
        return m_emu.manualLoadStateGeneration();
    }

    uint32_t exactEmulationFrame() const
    {
        return m_emu.frameCount();
    }

    uint32_t getRegionFPS() const
    {
        return m_emu.getRegionFPS();
    }

    const uint32_t* getFramebuffer() const
    {
        return m_emu.getFramebuffer();
    }

    void setPresenterLockActive(bool active)
    {
        (void)active;
    }

    void setSimulationSuspended(bool suspended)
    {
        (void)suspended;
    }

    bool update(uint32_t dt)
    {
        runPreAdvanceHook();
        const bool advanced = m_emu.update(dt);
        return advanced;
    }

    void updateUntilFrame(uint32_t dt)
    {
        runPreAdvanceHook();
        m_emu.updateUntilFrame(dt);
    }

    void configureNetplaySnapshots(size_t snapshotCapacity)
    {
        m_netplaySnapshotCapacity = snapshotCapacity;
        while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity) {
            m_netplaySnapshots.pop_front();
        }
        m_netplayDiagnostics.enabled = m_netplaySnapshotCapacity > 0;
        m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
        m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    }

    bool rollbackToFrame(uint32_t frame)
    {
        std::vector<uint8_t> snapshotData;
        auto it = std::find_if(
            m_netplaySnapshots.rbegin(),
            m_netplaySnapshots.rend(),
            [frame](const NetplayStoredSnapshot& stored) { return stored.frame == frame; }
        );
        if(it == m_netplaySnapshots.rend()) {
            return false;
        }
        snapshotData = it->data;

        if(snapshotData.empty()) return false;
        const uint32_t rollbackFrom = m_emu.frameCount();
        m_emu.loadStateFromMemoryWithAudioPolicy(
            snapshotData,
            GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
        const bool loaded = m_emu.valid();
        if(loaded) {
            ++m_netplayDiagnostics.rollbackStats.rollbackCount;
            m_netplayDiagnostics.rollbackStats.lastRollbackFromFrame = rollbackFrom;
            m_netplayDiagnostics.rollbackStats.lastRollbackToFrame = frame;
            if(rollbackFrom > frame) {
                m_netplayDiagnostics.rollbackStats.maxRollbackDistance =
                    std::max<uint32_t>(m_netplayDiagnostics.rollbackStats.maxRollbackDistance, rollbackFrom - frame);
            }
        }
        return loaded;
    }

    std::vector<uint8_t> saveStateToMemory()
    {
        return m_emu.saveStateToMemory();
    }

    std::vector<uint8_t> saveNetplayStateToMemory()
    {
        return m_emu.saveNetplayStateToMemory();
    }

    bool loadStateFromMemory(const std::vector<uint8_t>& data)
    {
        if(data.empty()) return false;
        return m_emu.loadStateFromMemoryOnCleanBoot(data);
    }

    bool loadStateFromMemoryAsManualStateChange(const std::vector<uint8_t>& data)
    {
        if(data.empty()) return false;
        const bool loaded = m_emu.loadStateFromMemoryOnCleanBoot(data);
        if(loaded) {
            onLoadExecutedLocked(m_emu.frameCount());
        }
        return loaded;
    }

    template<typename InputProvider>
    bool resimulateToFrame(uint32_t targetFrame, InputProvider&& inputProvider)
    {
        if(!m_emu.valid()) return false;

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
            m_emu.updateUntilFrame(frameDt);
        }
        if(hasLastReplayInput) {
            m_pendingInput = lastReplayInput.state;
        }
        return true;
    }

    uint32_t canonicalStateCrc32()
    {
        return m_emu.canonicalStateCrc32();
    }

    uint32_t canonicalNetplayStateCrc32()
    {
        return m_emu.canonicalNetplayStateCrc32();
    }

    uint32_t lastFrameReadyFrame() const
    {
        return m_lastFrameReadyFrameValue;
    }

    uint32_t lastFrameReadyNetplayCrc32() const
    {
        return m_lastFrameReadyNetplayCrc32Value;
    }

    std::optional<std::vector<uint8_t>> netplaySnapshotForFrame(uint32_t frame) const
    {
        auto it = std::find_if(
            m_netplaySnapshots.rbegin(),
            m_netplaySnapshots.rend(),
            [frame](const NetplayStoredSnapshot& stored) { return stored.frame == frame; }
        );
        if(it == m_netplaySnapshots.rend()) {
            return std::nullopt;
        }
        return it->data;
    }

    std::optional<uint32_t> netplaySnapshotCrc32ForFrame(uint32_t frame) const
    {
        auto it = std::find_if(
            m_netplaySnapshots.rbegin(),
            m_netplaySnapshots.rend(),
            [frame](const NetplayStoredSnapshot& stored) { return stored.frame == frame; }
        );
        if(it == m_netplaySnapshots.rend()) {
            return std::nullopt;
        }
        return it->crc32;
    }

    void seedNetplaySnapshot(uint32_t frame,
                             const std::vector<uint8_t>& data,
                             std::optional<uint32_t> canonicalCrc32 = std::nullopt)
    {
        if(data.empty()) return;

        const uint32_t crc32 =
            canonicalCrc32.has_value()
                ? *canonicalCrc32
                : Crc32::calc(reinterpret_cast<const char*>(data.data()), data.size());
        auto existing = std::find_if(
            m_netplaySnapshots.begin(),
            m_netplaySnapshots.end(),
            [frame](const NetplayStoredSnapshot& stored) { return stored.frame == frame; }
        );
        if(existing != m_netplaySnapshots.end()) {
            existing->crc32 = crc32;
            existing->data = data;
        } else {
            m_netplaySnapshots.push_back(NetplayStoredSnapshot{frame, crc32, data});
            while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity && !m_netplaySnapshots.empty()) {
                m_netplaySnapshots.pop_front();
            }
        }
        m_netplayDiagnostics.enabled = m_netplaySnapshotCapacity > 0;
        m_netplayDiagnostics.currentFrame = frame;
        m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
        m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
        m_netplayDiagnostics.latestSnapshotCrc32 = crc32;
    }

    NetplayDiagnosticsSnapshot getNetplayDiagnostics() const
    {
        return m_netplayDiagnostics;
    }
};
