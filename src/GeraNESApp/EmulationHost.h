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

        bool p2A = false;
        bool p2B = false;
        bool p2Select = false;
        bool p2Start = false;
        bool p2Up = false;
        bool p2Down = false;
        bool p2Left = false;
        bool p2Right = false;

        int zapperX = -1;
        int zapperY = -1;
        float arkanoidPosition = 0.5f;
        bool zapperP1Trigger = false;
        bool zapperP2Trigger = false;
        bool bandaiTrigger = false;
        bool mousePrimaryButton = false;

        bool rewind = false;
        bool speedBoost = false;
    };

private:
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
        int nsfTotalSongs = 0;
        int nsfCurrentSong = 0;
        std::string audioDeviceName;
        std::vector<std::string> audioDevices;
        float audioVolume = 1.0f;
        std::string audioChannelsJson = "{\"channels\":[]}";
    };

#ifdef __EMSCRIPTEN__
    GeraNESEmu m_emu;
    IAudioOutput& m_audioOutput;
    InputState m_pendingInput;

    void applyPendingInput()
    {
        const InputState input = m_pendingInput;
        m_emu.setController1Buttons(input.p1A, input.p1B, input.p1Select, input.p1Start, input.p1Up, input.p1Down, input.p1Left, input.p1Right);
        m_emu.setController2Buttons(input.p2A, input.p2B, input.p2Select, input.p2Start, input.p2Up, input.p2Down, input.p2Left, input.p2Right);
        m_emu.setBandaiHyperShotButtons(input.p2A, input.p2B, input.p2Select, input.p2Start, input.p2Up, input.p2Down, input.p2Left, input.p2Right);
        m_emu.setZapper(Settings::Port::P_1, input.zapperX, input.zapperY, input.zapperP1Trigger);
        m_emu.setZapper(Settings::Port::P_2, input.zapperX, input.zapperY, input.zapperP2Trigger);
        m_emu.setBandaiHyperShot(input.zapperX, input.zapperY, input.bandaiTrigger);
        m_emu.setArkanoidController(Settings::Port::P_1, input.arkanoidPosition, input.mousePrimaryButton);
        m_emu.setArkanoidController(Settings::Port::P_2, input.arkanoidPosition, input.mousePrimaryButton);
        m_emu.setArkanoidControllerFamicom(input.arkanoidPosition, input.mousePrimaryButton);
        m_emu.setRewind(input.rewind);
        m_emu.setSpeedBoost(input.speedBoost);
    }
#else
    GeraNESEmu m_emu;
    IAudioOutput& m_audioOutput;
    mutable std::mutex m_emuMutex;
    mutable std::mutex m_snapshotMutex;
    mutable std::mutex m_workerReadyMutex;
    std::condition_variable m_workerReadyCv;
    bool m_workerReady = false;
    std::atomic<bool> m_shutdownStarted{false};
    std::jthread m_workerThread;
    SigSlot::Signal<InputState> m_signalInputState;
    SigSlot::Signal<std::function<void(GeraNESEmu&)>> m_signalCommand;
    std::atomic<int> m_frontFramebufferIndex{0};
    std::array<std::vector<uint32_t>, 2> m_framebuffers{
        std::vector<uint32_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0),
        std::vector<uint32_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0)
    };
    Snapshot m_snapshot;
    InputState m_pendingInput;

    void onSetPendingInput(InputState input)
    {
        m_pendingInput = input;
    }

    void applyPendingInputLocked()
    {
        const InputState input = m_pendingInput;
        m_emu.setController1Buttons(input.p1A, input.p1B, input.p1Select, input.p1Start, input.p1Up, input.p1Down, input.p1Left, input.p1Right);
        m_emu.setController2Buttons(input.p2A, input.p2B, input.p2Select, input.p2Start, input.p2Up, input.p2Down, input.p2Left, input.p2Right);
        m_emu.setBandaiHyperShotButtons(input.p2A, input.p2B, input.p2Select, input.p2Start, input.p2Up, input.p2Down, input.p2Left, input.p2Right);
        m_emu.setZapper(Settings::Port::P_1, input.zapperX, input.zapperY, input.zapperP1Trigger);
        m_emu.setZapper(Settings::Port::P_2, input.zapperX, input.zapperY, input.zapperP2Trigger);
        m_emu.setBandaiHyperShot(input.zapperX, input.zapperY, input.bandaiTrigger);
        m_emu.setArkanoidController(Settings::Port::P_1, input.arkanoidPosition, input.mousePrimaryButton);
        m_emu.setArkanoidController(Settings::Port::P_2, input.arkanoidPosition, input.mousePrimaryButton);
        m_emu.setArkanoidControllerFamicom(input.arkanoidPosition, input.mousePrimaryButton);
        m_emu.setRewind(input.rewind);
        m_emu.setSpeedBoost(input.speedBoost);
    }

    void onCommand(std::function<void(GeraNESEmu&)> command)
    {
        if(command) {
            command(m_emu);
            refreshSnapshotLocked();
        }
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
                        refreshSnapshotLocked();
                    } else {
                        refreshSnapshotLocked();
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
#else
        m_emu.signalFrameStart.bind(&EmulationHost::applyPendingInputLocked, this);
        m_signalInputState.bind_auto(&EmulationHost::onSetPendingInput, this);
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
            m_workerThread.join();
        }
#endif
    }

    void setPendingInput(const InputState& input)
    {
#ifdef __EMSCRIPTEN__
        m_pendingInput = input;
#else
        m_signalInputState(input);
#endif
    }

    void postCommand(std::function<void(GeraNESEmu&)> command)
    {
#ifdef __EMSCRIPTEN__
        if(command) command(m_emu);
#else
        m_signalCommand(std::move(command));
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
        return m_emu.open(path);
#else
        std::scoped_lock emuLock(m_emuMutex);
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
        m_emu.reset();
#else
        postCommand([](GeraNESEmu& emu) {
            if(!emu.valid()) return;
            emu.reset();
        });
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

    void setExpansionDevice(Settings::ExpansionDevice device)
    {
        postCommand([=](GeraNESEmu& emu) {
            emu.setExpansionDevice(device);
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

    bool update(uint32_t dt)
    {
#ifdef __EMSCRIPTEN__
        return m_emu.update(dt);
#else
        (void)dt;
        return true;
#endif
    }

    void updateUntilFrame(uint32_t dt)
    {
#ifdef __EMSCRIPTEN__
        m_emu.updateUntilFrame(dt);
#else
        (void)dt;
#endif
    }
};
