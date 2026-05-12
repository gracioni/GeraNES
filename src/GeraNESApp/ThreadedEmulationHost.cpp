#include "GeraNESApp/ThreadedEmulationHost.h"

#include <cstring>

namespace
{
using HostTimingClock = std::chrono::steady_clock;

uint64_t elapsedMicrosSince(HostTimingClock::time_point start)
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            HostTimingClock::now() - start
        ).count()
    );
}
}

thread_local const ThreadedEmulationHost* ThreadedEmulationHost::t_directAccessHost = nullptr;

std::optional<size_t> ThreadedEmulationHost::snapshotIndexForFrameLocked(uint32_t frame) const
{
    const auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt == m_netplaySnapshotIndexByFrame.end()) {
        return std::nullopt;
    }
    const uint64_t position = indexIt->second;
    if(position < m_netplaySnapshotHeadPosition || position >= m_netplaySnapshotNextPosition) {
        return std::nullopt;
    }
    const uint64_t relative = position - m_netplaySnapshotHeadPosition;
    if(relative >= m_netplaySnapshots.size()) {
        return std::nullopt;
    }
    const size_t index = static_cast<size_t>(relative);
    if(m_netplaySnapshots[index].frame != frame) {
        return std::nullopt;
    }
    return index;
}

void ThreadedEmulationHost::discardNetplaySnapshotsAfter(uint32_t frame)
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    while(!m_netplaySnapshots.empty() && m_netplaySnapshots.back().frame > frame) {
        m_netplaySnapshotIndexByFrame.erase(m_netplaySnapshots.back().frame);
        m_netplaySnapshots.pop_back();
        if(m_netplaySnapshotNextPosition > m_netplaySnapshotHeadPosition) {
            --m_netplaySnapshotNextPosition;
        }
    }
    if(m_hasCachedNetplayCrc && m_cachedNetplayCrcFrame > frame) {
        m_hasCachedNetplayCrc = false;
    }
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    m_netplayDiagnostics.currentFrame = frame;
}

void ThreadedEmulationHost::onResetExecutedLocked(uint32_t frame)
{
    std::scoped_lock eventLock(m_manualStateChangeMutex);
    m_manualStateChanges.push_back(ManualStateChangeRecord{ManualStateChangeKind::Reset, frame});
}

void ThreadedEmulationHost::onLoadExecutedLocked(uint32_t frame)
{
    std::scoped_lock eventLock(m_manualStateChangeMutex);
    m_manualStateChanges.push_back(ManualStateChangeRecord{ManualStateChangeKind::LoadState, frame});
}

void ThreadedEmulationHost::recordFrameReadyNetplayState(GeraNESEmu& emu)
{
    const uint32_t frame = emu.frameCount();
    const auto snapshotSaveStart = HostTimingClock::now();
    GeraNESEmu::NetplaySnapshotWithCrc32 snapshot = emu.saveNetplaySnapshotWithCrc32();
    const uint64_t snapshotSaveElapsedUs = elapsedMicrosSince(snapshotSaveStart);
    const uint32_t crc32 = snapshot.crc32;
    m_lastFrameReadyFrameValue = frame;
    m_lastFrameReadyNetplayCrc32Value = crc32;
    m_hasCachedNetplayCrc = true;
    m_cachedNetplayCrcFrame = frame;
    m_cachedNetplayCrcValue = crc32;

    size_t snapshotCapacity = 0;
    {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        snapshotCapacity = m_netplaySnapshotCapacity;
    }

    if(snapshotCapacity == 0) {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        m_netplayDiagnostics.netplayStateSaveTiming.record(snapshotSaveElapsedUs);
        m_netplayDiagnostics.netplayStateSerializedBytes.record(snapshot.data.size());
        return;
    }

    if(snapshot.data.empty()) {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        m_netplayDiagnostics.netplayRollbackSnapshotSaveTiming.record(snapshotSaveElapsedUs);
        m_netplayDiagnostics.netplayRollbackSnapshotSerializedBytes.record(0);
        return;
    }
    const size_t snapshotDataSize = snapshot.data.size();

    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    if(const std::optional<size_t> index = snapshotIndexForFrameLocked(frame); index.has_value()) {
        auto& existing = m_netplaySnapshots[*index];
        existing.crc32 = crc32;
        existing.data = std::make_shared<std::vector<uint8_t>>(std::move(snapshot.data));
    } else {
        m_netplaySnapshots.push_back(NetplayStoredSnapshot{
            frame,
            crc32,
            std::make_shared<std::vector<uint8_t>>(std::move(snapshot.data))
        });
        m_netplaySnapshotIndexByFrame[frame] = m_netplaySnapshotNextPosition++;
        while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity) {
            m_netplaySnapshotIndexByFrame.erase(m_netplaySnapshots.front().frame);
            m_netplaySnapshots.pop_front();
            ++m_netplaySnapshotHeadPosition;
        }
    }

    m_netplayDiagnostics.enabled = true;
    m_netplayDiagnostics.currentFrame = frame;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    m_netplayDiagnostics.latestSnapshotCrc32 = crc32;
    m_netplayDiagnostics.netplayRollbackSnapshotSaveTiming.record(snapshotSaveElapsedUs);
    m_netplayDiagnostics.netplayRollbackSnapshotSerializedBytes.record(snapshotDataSize);
}

void ThreadedEmulationHost::runPreAdvanceHookLocked()
{
    std::function<void(GeraNESEmu&)> hook;
    {
        std::scoped_lock hookLock(m_preAdvanceHookMutex);
        hook = m_preAdvanceHook;
    }
    if(hook) {
        hook(m_emu);
    }
}

void ThreadedEmulationHost::applyPendingInputLocked()
{
    if(!m_autoQueuePendingInputOnFrameStart.load(std::memory_order_acquire)) {
        std::scoped_lock resolverLock(m_frameInputResolverMutex);
        if(!m_frameInputResolver) {
            return;
        }
    }

    const uint32_t targetFrame = m_emu.frameCount() + 1u;
    ReplayFrameInput input;
    {
        std::scoped_lock resolverLock(m_frameInputResolverMutex);
        if(m_frameInputResolver) {
            if(!m_frameInputResolver(targetFrame, input)) {
                return;
            }
            queueReplayFrameInputToEmu(m_emu, targetFrame, input);
            return;
        }
    }

    {
        std::scoped_lock pendingInputLock(m_pendingInputMutex);
        input.state = m_pendingInput;
    }
    applyInputStateToEmu(m_emu, input.state);
}

void ThreadedEmulationHost::onCommand(std::function<void(GeraNESEmu&)> command)
{
    if(command) {
        command(m_emu);
        refreshSnapshotLocked();
    }

    m_workerWakeRequested.store(true, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::refreshPpuViewerSnapshotLocked(uint32_t frameCount)
{
    if(!m_ppuViewerCaptureEnabled) {
        return;
    }

    const PPU& ppu = m_emu.getConsole().ppu();
    const int ppuScanline = ppu.scanline();
    const int ppuCycle = ppu.cycle();

    std::scoped_lock snapshotLock(m_ppuViewerSnapshotMutex);
    if(m_ppuViewerSnapshot.valid &&
       m_ppuViewerSnapshot.frameCount == frameCount &&
       (!m_ppuViewerScanlineCaptureEnabled ||
        (m_ppuViewerSnapshot.ppuScanline == ppuScanline &&
         m_ppuViewerSnapshot.ppuCycle == ppuCycle))) {
        return;
    }

    PpuViewerSnapshot snapshot;
    snapshot.valid = m_emu.valid();
    snapshot.frameCount = frameCount;
    snapshot.ppuScanline = ppuScanline;
    snapshot.ppuCycle = ppuCycle;
    if(snapshot.valid) {
        snapshot.rgbPalette = ppu.colorPalette();
        snapshot.scrollX = ppu.getCursorX();
        snapshot.scrollY = ppu.getCursorY();
        snapshot.backgroundPatternTableAddress = ppu.debugBackgroundPatternTableAddress();
        for(size_t i = 0; i < snapshot.chrData.size(); ++i) {
            snapshot.chrData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(i));
        }
        for(size_t i = 0; i < snapshot.nametableData.size(); ++i) {
            snapshot.nametableData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x2000 + i));
        }
        for(size_t i = 0; i < snapshot.paletteData.size(); ++i) {
            snapshot.paletteData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x3F00 + i));
        }
    }

    m_ppuViewerSnapshot = std::move(snapshot);
}

void ThreadedEmulationHost::refreshPpuEventViewerSnapshotLocked(uint32_t frameCount)
{
    const bool traceEnabled = m_ppuEventViewerCaptureEnabled;
    if(m_emu.ppuEventTraceEnabled() != traceEnabled) {
        m_emu.enablePpuEventTrace(traceEnabled);
    }

    if(!traceEnabled) {
        std::scoped_lock snapshotLock(m_ppuEventViewerSnapshotMutex);
        m_ppuEventViewerSnapshot.valid = false;
        m_ppuEventViewerSnapshot.traceEnabled = false;
        m_ppuEventViewerSnapshot.frameCount = frameCount;
        m_ppuEventViewerSnapshot.events.clear();
        m_ppuEventViewerSnapshot.framebuffer.clear();
        return;
    }

    std::scoped_lock snapshotLock(m_ppuEventViewerSnapshotMutex);
    if(m_ppuEventViewerSnapshot.valid &&
       m_ppuEventViewerSnapshot.traceEnabled &&
       m_ppuEventViewerSnapshot.frameCount == frameCount) {
        return;
    }

    PpuEventViewerSnapshot snapshot;
    snapshot.valid = m_emu.valid();
    snapshot.traceEnabled = traceEnabled;
    snapshot.frameCount = frameCount;
    snapshot.events = m_emu.ppuRegisterAccessEvents();
    if(snapshot.valid) {
        const uint32_t* framebuffer = m_emu.getConsole().ppu().getFramebuffer();
        snapshot.framebuffer.assign(
            framebuffer,
            framebuffer + (PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT)
        );
    }

    m_ppuEventViewerSnapshot = std::move(snapshot);
}

void ThreadedEmulationHost::refreshSnapshotLocked()
{
    const bool valid = m_emu.valid();
    const bool paused = m_emu.paused();
    const bool rewinding = m_emu.isRewinding();
    const bool spriteLimitDisabled = m_emu.spriteLimitDisabled();
    const bool overclocked = m_emu.overclocked();
    const bool nsfLoaded = m_emu.isNsfLoaded();
    const bool nsfPlaying = m_emu.nsfIsPlaying();
    const bool nsfPaused = m_emu.nsfIsPaused();
    const bool nsfEnded = m_emu.nsfHasEnded();
    const uint32_t frameCount = m_emu.frameCount();
    const uint32_t inputTimelineEpoch = m_emu.inputTimelineEpoch();
    const uint32_t regionFps = m_emu.getRegionFPS();
    const uint32_t manualResetGeneration = m_emu.manualResetGeneration();
    const uint32_t manualLoadStateGeneration = m_emu.manualLoadStateGeneration();
    const Settings::Region region = m_emu.region();
    const GameDatabase::System cartridgeSystem =
        valid
            ? m_emu.getConsole().cartridge().system()
            : GameDatabase::System::Unknown;
    const std::optional<Settings::Device> port1Device = m_emu.getPortDevice(Settings::Port::P_1);
    const std::optional<Settings::Device> port2Device = m_emu.getPortDevice(Settings::Port::P_2);
    const Settings::ExpansionDevice expansionDevice = m_emu.getExpansionDevice();
    const Settings::NesMultitapDevice nesMultitapDevice = m_emu.getNesMultitapDevice();
    const Settings::FamicomMultitapDevice famicomMultitapDevice = m_emu.getFamicomMultitapDevice();
    const int nsfTotalSongs = m_emu.nsfTotalSongs();
    const int nsfCurrentSong = m_emu.nsfCurrentSong();
    const uint32_t lastFrameReadyFrame = m_lastFrameReadyFrameValue;
    const uint32_t lastFrameReadyNetplayCrc32 = m_lastFrameReadyNetplayCrc32Value;

    refreshPpuViewerSnapshotLocked(frameCount);
    refreshPpuEventViewerSnapshotLocked(frameCount);

    constexpr auto kSlowSnapshotRefreshInterval = std::chrono::milliseconds(250);
    const auto now = std::chrono::steady_clock::now();
    const bool refreshSlowFields =
        m_snapshotConfigDirty ||
        m_lastSlowSnapshotRefresh.time_since_epoch().count() == 0 ||
        (now - m_lastSlowSnapshotRefresh) >= kSlowSnapshotRefreshInterval;

    std::string audioDeviceName;
    std::vector<std::string> audioDevices;
    IAudioOutput::AudioFormatOptions audioFormatOptions;
    int audioSampleRate = 44100;
    int audioSampleSize = 16;
    float audioVolume = 1.0f;
    std::string audioChannelsJson;
    if(refreshSlowFields) {
        audioDeviceName = m_audioOutput.currentDeviceName();
        audioDevices = m_audioOutput.getAudioList();
        audioFormatOptions = m_audioOutput.getAudioFormatOptions(audioDeviceName);
        audioSampleRate = m_audioOutput.currentSampleRate();
        audioSampleSize = m_audioOutput.currentSampleSize();
        audioVolume = m_audioOutput.getVolume();
        audioChannelsJson = m_audioOutput.getAudioChannelsJson();
        m_snapshotConfigDirty = false;
        m_lastSlowSnapshotRefresh = now;
    }

    const bool holdPresentedFramebuffer =
        m_holdPresentedFramebufferUntilFrameReady.load(std::memory_order_acquire);
    if(!holdPresentedFramebuffer && m_framebufferDirty) {
        {
            std::scoped_lock framebufferLock(m_framebufferMutex);
            if(valid) {
                const int backIndex = 1 - m_frontFramebufferIndex.load(std::memory_order_relaxed);
                std::memcpy(
                    m_framebuffers[backIndex].data(),
                    m_emu.getFramebuffer(),
                    m_framebuffers[backIndex].size() * sizeof(uint32_t)
                );
                m_frontFramebufferIndex.store(backIndex, std::memory_order_release);
            } else {
                for(auto& framebuffer : m_framebuffers) {
                    std::fill(framebuffer.begin(), framebuffer.end(), 0u);
                }
                m_frontFramebufferIndex.store(0, std::memory_order_release);
            }
        }
        m_framebufferDirty = false;
    }

    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        m_snapshot.valid = valid;
        m_snapshot.paused = paused;
        m_snapshot.rewinding = rewinding;
        m_snapshot.spriteLimitDisabled = spriteLimitDisabled;
        m_snapshot.overclocked = overclocked;
        m_snapshot.nsfLoaded = nsfLoaded;
        m_snapshot.nsfPlaying = nsfPlaying;
        m_snapshot.nsfPaused = nsfPaused;
        m_snapshot.nsfEnded = nsfEnded;
        m_snapshot.frameCount = frameCount;
        m_snapshot.inputTimelineEpoch = inputTimelineEpoch;
        m_snapshot.regionFps = regionFps;
        m_snapshot.manualResetGeneration = manualResetGeneration;
        m_snapshot.manualLoadStateGeneration = manualLoadStateGeneration;
        m_snapshot.region = region;
        m_snapshot.cartridgeSystem = cartridgeSystem;
        m_snapshot.port1Device = port1Device;
        m_snapshot.port2Device = port2Device;
        m_snapshot.expansionDevice = expansionDevice;
        m_snapshot.nesMultitapDevice = nesMultitapDevice;
        m_snapshot.famicomMultitapDevice = famicomMultitapDevice;
        m_snapshot.nsfTotalSongs = nsfTotalSongs;
        m_snapshot.nsfCurrentSong = nsfCurrentSong;
        m_snapshot.lastFrameReadyFrame = lastFrameReadyFrame;
        m_snapshot.lastFrameReadyNetplayCrc32 = lastFrameReadyNetplayCrc32;
        if(refreshSlowFields) {
            m_snapshot.audioDeviceName = std::move(audioDeviceName);
            m_snapshot.audioDevices = std::move(audioDevices);
            m_snapshot.audioFormatOptions = std::move(audioFormatOptions);
            m_snapshot.audioSampleRate = audioSampleRate;
            m_snapshot.audioSampleSize = audioSampleSize;
            m_snapshot.audioVolume = audioVolume;
            m_snapshot.audioChannelsJson = std::move(audioChannelsJson);
        }
    }
}

void ThreadedEmulationHost::onFrameReadyLocked()
{
    recordFrameReadyNetplayState(m_emu);
    m_holdPresentedFramebufferUntilFrameReady.store(false, std::memory_order_release);
    m_framebufferDirty = true;
    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        m_snapshot.lastFrameReadyFrame = m_lastFrameReadyFrameValue;
        m_snapshot.lastFrameReadyNetplayCrc32 = m_lastFrameReadyNetplayCrc32Value;
    }
}

void ThreadedEmulationHost::workerLoop(std::stop_token stopToken)
{
    using clock = std::chrono::steady_clock;
    constexpr uint32_t STEP_MS = 1;

    move_to_current_thread();
    {
        std::scoped_lock readyLock(m_workerReadyMutex);
        m_workerThreadId = std::this_thread::get_id();
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
                runPreAdvanceHookLocked();
                m_workerWakeRequested.store(false, std::memory_order_release);
                refreshSnapshotLocked();
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

                runPreAdvanceHookLocked();

                if(m_emu.valid() && ticksToConsume > 0) {
                    m_emu.updateUntilFrame(dtMs);
                }
                else if(m_emu.valid() &&
                        timedOutWaitingPresenter &&
                        !wakeOnly &&
                        m_allowPresenterTimeoutAdvance.load(std::memory_order_acquire)) {
                    m_emu.update(dtMs);
                }

                (void)wakeOnly;
                refreshSnapshotLocked();
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
        const uint32_t fps = std::max<uint32_t>(1u, m_emu.getRegionFPS());
        const uint32_t maxCatchupSteps = ((1000u + fps - 1u) / fps) + 1u;
        while(now >= nextTick && catchupSteps < maxCatchupSteps && !stopToken.stop_requested()) {
            ++catchupSteps;
            nextTick += std::chrono::milliseconds(STEP_MS);

            {
                std::scoped_lock emuLock(m_emuMutex);
                dispatch_queued_calls();
                runPreAdvanceHookLocked();
                if(m_emu.valid()) {
                    m_emu.update(STEP_MS);
                    refreshSnapshotLocked();
                } else {
                    refreshSnapshotLocked();
                }
            }
        }

        if(catchupSteps == maxCatchupSteps) {
            nextTick = clock::now();
        }
    }
}

ThreadedEmulationHost::ThreadedEmulationHost(IAudioOutput& audioOutput)
    : m_emu(audioOutput)
    , m_audioOutput(audioOutput)
{
    m_emu.signalResetExecuted.bind(&ThreadedEmulationHost::onResetExecutedLocked, this);
    m_emu.signalLoadExecuted.bind(&ThreadedEmulationHost::onLoadExecutedLocked, this);
    m_emu.signalFrameStart.bind(&ThreadedEmulationHost::applyPendingInputLocked, this);
    m_emu.signalFrameReady.bind(&ThreadedEmulationHost::onFrameReadyLocked, this);
    m_signalCommand.bind_auto(&ThreadedEmulationHost::onCommand, this);
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
}

ThreadedEmulationHost::~ThreadedEmulationHost()
{
    shutdown();
}

void ThreadedEmulationHost::shutdown()
{
    if(m_shutdownStarted.exchange(true)) {
        return;
    }

    if(m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_presenterCv.notify_all();
        m_workerThread.join();
    }
}

void ThreadedEmulationHost::setPendingInput(const InputState& input)
{
    bool playbackResolverActive = false;
    {
        std::scoped_lock resolverLock(m_frameInputResolverMutex);
        playbackResolverActive = static_cast<bool>(m_frameInputResolver);
    }
    {
        std::scoped_lock pendingInputLock(m_pendingInputMutex);
        m_pendingInput = playbackResolverActive ? InputState{} : input;
    }
    m_workerWakeRequested.store(true, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setFrameInputResolver(FrameInputResolver resolver)
{
    bool playbackResolverActive = false;
    {
        std::scoped_lock resolverLock(m_frameInputResolverMutex);
        m_frameInputResolver = std::move(resolver);
        playbackResolverActive = static_cast<bool>(m_frameInputResolver);
    }
    {
        std::scoped_lock pendingInputLock(m_pendingInputMutex);
        if(playbackResolverActive) {
            m_pendingInput = {};
        }
    }
    m_workerWakeRequested.store(true, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setAutoQueuePendingInputOnFrameStart(bool enabled)
{
    m_autoQueuePendingInputOnFrameStart.store(enabled, std::memory_order_release);
    m_workerWakeRequested.store(true, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setAllowPresenterTimeoutAdvance(bool enabled)
{
    m_allowPresenterTimeoutAdvance.store(enabled, std::memory_order_release);
    m_workerWakeRequested.store(true, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setPreAdvanceHook(std::function<void(GeraNESEmu&)> hook)
{
    {
        std::scoped_lock hookLock(m_preAdvanceHookMutex);
        m_preAdvanceHook = std::move(hook);
    }
    m_workerWakeRequested.store(true, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setDebugTraceSink(std::function<void(const std::string&)> sink)
{
    (void)sink;
}

void ThreadedEmulationHost::postCommand(std::function<void(GeraNESEmu&)> command)
{
    m_workerWakeRequested.store(true, std::memory_order_release);
    m_signalCommand(std::move(command));
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setPpuViewerCaptureEnabled(bool enabled, bool scanlineTrace)
{
    if(m_ppuViewerCaptureEnabled == enabled && m_ppuViewerScanlineCaptureEnabled == scanlineTrace) {
        return;
    }
    std::scoped_lock emuLock(m_emuMutex);
    m_ppuViewerCaptureEnabled = enabled;
    m_ppuViewerScanlineCaptureEnabled = enabled && scanlineTrace;
    if(!enabled) {
        std::scoped_lock snapshotLock(m_ppuViewerSnapshotMutex);
        m_ppuViewerSnapshot = {};
    } else {
        refreshPpuViewerSnapshotLocked(m_emu.frameCount());
    }
    refreshSnapshotLocked();
}

bool ThreadedEmulationHost::getPpuViewerSnapshot(PpuViewerSnapshot& out) const
{
    std::scoped_lock snapshotLock(m_ppuViewerSnapshotMutex);
    out = m_ppuViewerSnapshot;
    return out.valid;
}

void ThreadedEmulationHost::setPpuEventViewerCaptureEnabled(bool enabled)
{
    if(m_ppuEventViewerCaptureEnabled == enabled) {
        return;
    }
    std::scoped_lock emuLock(m_emuMutex);
    m_ppuEventViewerCaptureEnabled = enabled;
    refreshPpuEventViewerSnapshotLocked(m_emu.frameCount());
    refreshSnapshotLocked();
}

bool ThreadedEmulationHost::getPpuEventViewerSnapshot(PpuEventViewerSnapshot& out) const
{
    std::scoped_lock snapshotLock(m_ppuEventViewerSnapshotMutex);
    out = m_ppuEventViewerSnapshot;
    return out.valid || out.traceEnabled;
}

uint32_t ThreadedEmulationHost::manualResetGeneration() const
{
    std::scoped_lock snapshotLock(m_snapshotMutex);
    return m_snapshot.manualResetGeneration;
}

uint32_t ThreadedEmulationHost::manualLoadStateGeneration() const
{
    std::scoped_lock snapshotLock(m_snapshotMutex);
    return m_snapshot.manualLoadStateGeneration;
}

uint32_t ThreadedEmulationHost::exactEmulationFrame() const
{
    std::scoped_lock emuLock(m_emuMutex);
    return m_emu.frameCount();
}

uint32_t ThreadedEmulationHost::getRegionFPS() const
{
    if(hasDirectEmuAccess()) {
        return m_emu.getRegionFPS();
    }
    std::scoped_lock snapshotLock(m_snapshotMutex);
    return m_snapshot.regionFps;
}

const uint32_t* ThreadedEmulationHost::getFramebuffer() const
{
    return m_framebuffers[m_frontFramebufferIndex.load(std::memory_order_acquire)].data();
}

void ThreadedEmulationHost::copyFramebuffer(std::vector<uint32_t>& out) const
{
    std::scoped_lock framebufferLock(m_framebufferMutex);
    const auto& framebuffer = m_framebuffers[m_frontFramebufferIndex.load(std::memory_order_acquire)];
    out.assign(framebuffer.begin(), framebuffer.end());
}

void ThreadedEmulationHost::beginPresentationHoldUntilNextFrameReady()
{
    m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
}

void ThreadedEmulationHost::setPresenterLockActive(bool active)
{
    if(active) {
        m_framePacingMode.store(FramePacingMode::PresenterLocked, std::memory_order_release);
    } else {
        m_framePacingMode.store(FramePacingMode::FreeRunning, std::memory_order_release);
        m_pendingPresenterTicks.store(0, std::memory_order_release);
    }
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setSimulationSuspended(bool suspended)
{
    if(suspended) {
        m_framePacingMode.store(FramePacingMode::Suspended, std::memory_order_release);
        m_pendingPresenterTicks.store(0, std::memory_order_release);
    } else if(m_framePacingMode.load(std::memory_order_acquire) == FramePacingMode::Suspended) {
        m_framePacingMode.store(FramePacingMode::FreeRunning, std::memory_order_release);
    }
    m_presenterCv.notify_one();
}

bool ThreadedEmulationHost::update(uint32_t dt)
{
    (void)dt;
    if(m_framePacingMode.load(std::memory_order_acquire) != FramePacingMode::Suspended) {
        m_framePacingMode.store(FramePacingMode::FreeRunning, std::memory_order_release);
    }
    m_pendingPresenterTicks.store(0, std::memory_order_release);
    m_presenterCv.notify_one();
    return true;
}

void ThreadedEmulationHost::updateUntilFrame(uint32_t dt)
{
    m_presenterTickDtMs.store(std::max<uint32_t>(1, dt), std::memory_order_release);
    m_framePacingMode.store(FramePacingMode::PresenterLocked, std::memory_order_release);
    m_pendingPresenterTicks.store(1, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::configureNetplaySnapshots(size_t snapshotCapacity)
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    m_netplaySnapshotCapacity = snapshotCapacity;
    while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity) {
        m_netplaySnapshotIndexByFrame.erase(m_netplaySnapshots.front().frame);
        m_netplaySnapshots.pop_front();
        ++m_netplaySnapshotHeadPosition;
    }
    if(m_netplaySnapshots.empty()) {
        m_netplaySnapshotIndexByFrame.clear();
        m_netplaySnapshotHeadPosition = 0;
        m_netplaySnapshotNextPosition = 0;
    }
    m_netplayDiagnostics.enabled = m_netplaySnapshotCapacity > 0;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
}

bool ThreadedEmulationHost::rollbackToFrame(uint32_t frame)
{
    std::shared_ptr<const std::vector<uint8_t>> snapshotData;
    {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        const std::optional<size_t> index = snapshotIndexForFrameLocked(frame);
        if(!index.has_value()) {
            return false;
        }
        snapshotData = m_netplaySnapshots[*index].data;
        m_netplayDiagnostics.rollbackSnapshotCopyBytes.record(0);
    }

    uint32_t rollbackFrom = 0;
    uint64_t rollbackLoadElapsedUs = 0;
    {
        std::scoped_lock emuLock(m_emuMutex);
        if(!snapshotData || snapshotData->empty()) return false;
        m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
        rollbackFrom = m_emu.frameCount();
        const auto rollbackLoadStart = HostTimingClock::now();
        m_emu.loadStateFromMemoryWithAudioPolicy(
            *snapshotData,
            GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
        rollbackLoadElapsedUs = elapsedMicrosSince(rollbackLoadStart);
        if(!m_emu.valid()) {
            std::scoped_lock netplayLock(m_netplaySnapshotMutex);
            m_netplayDiagnostics.rollbackLoadTiming.record(rollbackLoadElapsedUs);
            return false;
        }
        m_hasCachedNetplayCrc = false;
        refreshSnapshotLocked();
    }

    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    m_netplayDiagnostics.rollbackLoadTiming.record(rollbackLoadElapsedUs);
    ++m_netplayDiagnostics.rollbackStats.rollbackCount;
    m_netplayDiagnostics.rollbackStats.lastRollbackFromFrame = rollbackFrom;
    m_netplayDiagnostics.rollbackStats.lastRollbackToFrame = frame;
    if(rollbackFrom > frame) {
        m_netplayDiagnostics.rollbackStats.maxRollbackDistance =
            std::max<uint32_t>(m_netplayDiagnostics.rollbackStats.maxRollbackDistance, rollbackFrom - frame);
    }
    return true;
}

std::vector<uint8_t> ThreadedEmulationHost::saveStateToMemory()
{
    std::scoped_lock emuLock(m_emuMutex);
    return m_emu.saveStateToMemory();
}

std::vector<uint8_t> ThreadedEmulationHost::saveNetplayStateToMemory()
{
    if(hasDirectEmuAccess()) {
        const auto saveStart = HostTimingClock::now();
        std::vector<uint8_t> data = m_emu.saveNetplayStateToMemory();
        const uint64_t elapsedUs = elapsedMicrosSince(saveStart);
        {
            std::scoped_lock netplayLock(m_netplaySnapshotMutex);
            m_netplayDiagnostics.netplayStateSaveTiming.record(elapsedUs);
            m_netplayDiagnostics.netplayStateSerializedBytes.record(data.size());
        }
        return data;
    }

    std::vector<uint8_t> data;
    uint64_t elapsedUs = 0;
    {
        std::scoped_lock emuLock(m_emuMutex);
        const auto saveStart = HostTimingClock::now();
        data = m_emu.saveNetplayStateToMemory();
        elapsedUs = elapsedMicrosSince(saveStart);
    }
    {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        m_netplayDiagnostics.netplayStateSaveTiming.record(elapsedUs);
        m_netplayDiagnostics.netplayStateSerializedBytes.record(data.size());
    }
    return data;
}

bool ThreadedEmulationHost::loadStateFromMemory(const std::vector<uint8_t>& data)
{
    return loadStateFromMemoryOnCleanBoot(data);
}

bool ThreadedEmulationHost::loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& data)
{
    if(data.empty()) return false;
    if(hasDirectEmuAccess()) {
        m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
        if(!m_emu.loadStateFromMemoryOnCleanBoot(data)) {
            return false;
        }
        const uint32_t loadedFrame = m_emu.frameCount();
        m_emu.discardQueuedInputFramesAfter(loadedFrame > 0u ? (loadedFrame - 1u) : 0u);
        m_hasCachedNetplayCrc = false;
        refreshSnapshotLocked();
        return true;
    }

    std::scoped_lock emuLock(m_emuMutex);
    m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
    if(!m_emu.loadStateFromMemoryOnCleanBoot(data)) {
        return false;
    }
    const uint32_t loadedFrame = m_emu.frameCount();
    m_emu.discardQueuedInputFramesAfter(loadedFrame > 0u ? (loadedFrame - 1u) : 0u);
    m_hasCachedNetplayCrc = false;
    refreshSnapshotLocked();
    return true;
}

bool ThreadedEmulationHost::loadStateFromMemoryAsManualStateChange(const std::vector<uint8_t>& data)
{
    if(data.empty()) return false;
    std::scoped_lock emuLock(m_emuMutex);
    m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
    if(!m_emu.loadStateFromMemoryOnCleanBoot(data)) {
        return false;
    }
    const uint32_t loadedFrame = m_emu.frameCount();
    m_emu.discardQueuedInputFramesAfter(loadedFrame > 0u ? (loadedFrame - 1u) : 0u);
    m_hasCachedNetplayCrc = false;
    refreshSnapshotLocked();
    onLoadExecutedLocked(m_emu.frameCount());
    return true;
}

uint32_t ThreadedEmulationHost::canonicalStateCrc32()
{
    std::scoped_lock emuLock(m_emuMutex);
    return m_emu.canonicalStateCrc32();
}

uint32_t ThreadedEmulationHost::canonicalNetplayStateCrc32()
{
    if(hasDirectEmuAccess()) {
        const uint32_t frame = m_emu.frameCount();
        if(m_hasCachedNetplayCrc && m_cachedNetplayCrcFrame == frame) {
            return m_cachedNetplayCrcValue;
        }
        const auto crcStart = HostTimingClock::now();
        const uint32_t crc = m_emu.canonicalNetplayStateCrc32();
        const uint64_t elapsedUs = elapsedMicrosSince(crcStart);
        m_hasCachedNetplayCrc = true;
        m_cachedNetplayCrcFrame = frame;
        m_cachedNetplayCrcValue = crc;
        {
            std::scoped_lock netplayLock(m_netplaySnapshotMutex);
            m_netplayDiagnostics.netplayCrcTiming.record(elapsedUs);
        }
        return crc;
    }

    uint32_t crc = 0;
    uint64_t elapsedUs = 0;
    bool generatedCrc = false;
    {
        std::scoped_lock emuLock(m_emuMutex);
        const uint32_t frame = m_emu.frameCount();
        if(m_hasCachedNetplayCrc && m_cachedNetplayCrcFrame == frame) {
            return m_cachedNetplayCrcValue;
        }
        const auto crcStart = HostTimingClock::now();
        crc = m_emu.canonicalNetplayStateCrc32();
        elapsedUs = elapsedMicrosSince(crcStart);
        generatedCrc = true;
        m_hasCachedNetplayCrc = true;
        m_cachedNetplayCrcFrame = frame;
        m_cachedNetplayCrcValue = crc;
    }
    if(generatedCrc) {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        m_netplayDiagnostics.netplayCrcTiming.record(elapsedUs);
    }
    return crc;
}

uint32_t ThreadedEmulationHost::lastFrameReadyFrame() const
{
    std::scoped_lock snapshotLock(m_snapshotMutex);
    return m_snapshot.lastFrameReadyFrame;
}

uint32_t ThreadedEmulationHost::lastFrameReadyNetplayCrc32() const
{
    std::scoped_lock snapshotLock(m_snapshotMutex);
    return m_snapshot.lastFrameReadyNetplayCrc32;
}

void ThreadedEmulationHost::setAuthoritativeFrameReadyState(uint32_t frame, uint32_t canonicalCrc32)
{
    m_lastFrameReadyFrameValue = frame;
    m_lastFrameReadyNetplayCrc32Value = canonicalCrc32;
    std::scoped_lock snapshotLock(m_snapshotMutex);
    m_snapshot.lastFrameReadyFrame = frame;
    m_snapshot.lastFrameReadyNetplayCrc32 = canonicalCrc32;
}

std::optional<std::shared_ptr<const std::vector<uint8_t>>> ThreadedEmulationHost::netplaySnapshotForFrame(uint32_t frame) const
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    const std::optional<size_t> index = snapshotIndexForFrameLocked(frame);
    if(!index.has_value()) {
        return std::nullopt;
    }
    const std::shared_ptr<const std::vector<uint8_t>> data = m_netplaySnapshots[*index].data;
    m_netplayDiagnostics.snapshotLookupCopyBytes.record(0);
    return data;
}

std::optional<uint32_t> ThreadedEmulationHost::netplaySnapshotCrc32ForFrame(uint32_t frame) const
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    const std::optional<size_t> index = snapshotIndexForFrameLocked(frame);
    if(!index.has_value()) {
        return std::nullopt;
    }
    return m_netplaySnapshots[*index].crc32;
}

bool ThreadedEmulationHost::updateNetplaySnapshotCrc32ForFrame(uint32_t frame, uint32_t canonicalCrc32)
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    const std::optional<size_t> index = snapshotIndexForFrameLocked(frame);
    if(!index.has_value()) {
        return false;
    }
    m_netplaySnapshots[*index].crc32 = canonicalCrc32;
    m_netplayDiagnostics.latestSnapshotCrc32 = canonicalCrc32;
    m_netplayDiagnostics.currentFrame = frame;
    return true;
}

void ThreadedEmulationHost::seedNetplaySnapshot(
    uint32_t frame,
    const std::vector<uint8_t>& data,
    std::optional<uint32_t> canonicalCrc32)
{
    if(data.empty()) return;

    if(!canonicalCrc32.has_value()) {
        Logger::instance().log(
            "Netplay snapshot seeded without canonical CRC; using payload CRC bookkeeping only",
            Logger::Type::WARNING
        );
    }

    const uint32_t crc32 = canonicalCrc32.value_or(
        Crc32::calc(reinterpret_cast<const char*>(data.data()), data.size())
    );
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    if(const std::optional<size_t> index = snapshotIndexForFrameLocked(frame); index.has_value()) {
        auto& existing = m_netplaySnapshots[*index];
        existing.crc32 = crc32;
        existing.data = std::make_shared<std::vector<uint8_t>>(data);
    } else {
        m_netplaySnapshots.push_back(NetplayStoredSnapshot{
            frame,
            crc32,
            std::make_shared<std::vector<uint8_t>>(data)
        });
        m_netplaySnapshotIndexByFrame[frame] = m_netplaySnapshotNextPosition++;
        while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity && !m_netplaySnapshots.empty()) {
            m_netplaySnapshotIndexByFrame.erase(m_netplaySnapshots.front().frame);
            m_netplaySnapshots.pop_front();
            ++m_netplaySnapshotHeadPosition;
        }
    }
    m_netplayDiagnostics.enabled = m_netplaySnapshotCapacity > 0;
    m_netplayDiagnostics.currentFrame = frame;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    m_netplayDiagnostics.latestSnapshotCrc32 = crc32;
    m_netplayDiagnostics.seededSnapshotCopyBytes.record(data.size());
}

ThreadedEmulationHost::NetplayDiagnosticsSnapshot ThreadedEmulationHost::getNetplayDiagnostics() const
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    return m_netplayDiagnostics;
}
