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

void ThreadedEmulationHost::rebuildNetplaySnapshotIndexLocked()
{
    m_netplaySnapshotIndexByFrame.clear();
    for(size_t i = 0; i < m_netplaySnapshots.size(); ++i) {
        m_netplaySnapshotIndexByFrame[m_netplaySnapshots[i].frame] = i;
    }
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
    const auto crcStart = HostTimingClock::now();
    const uint32_t crc32 = emu.canonicalNetplayStateCrc32();
    const uint64_t crcElapsedUs = elapsedMicrosSince(crcStart);
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
        m_netplayDiagnostics.netplayCrcTiming.record(crcElapsedUs);
        return;
    }

    const auto snapshotSaveStart = HostTimingClock::now();
    std::vector<uint8_t> snapshotData = emu.saveNetplayRollbackStateToMemory();
    const uint64_t snapshotSaveElapsedUs = elapsedMicrosSince(snapshotSaveStart);
    if(snapshotData.empty()) {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        m_netplayDiagnostics.netplayCrcTiming.record(crcElapsedUs);
        m_netplayDiagnostics.netplayRollbackSnapshotSaveTiming.record(snapshotSaveElapsedUs);
        m_netplayDiagnostics.netplayRollbackSnapshotSerializedBytes.record(0);
        return;
    }
    const size_t snapshotDataSize = snapshotData.size();

    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt != m_netplaySnapshotIndexByFrame.end() &&
       indexIt->second < m_netplaySnapshots.size() &&
       m_netplaySnapshots[indexIt->second].frame == frame) {
        auto& existing = m_netplaySnapshots[indexIt->second];
        existing.crc32 = crc32;
        existing.data = std::move(snapshotData);
    } else {
        m_netplaySnapshots.push_back(NetplayStoredSnapshot{frame, crc32, std::move(snapshotData)});
        while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity) {
            m_netplaySnapshots.pop_front();
        }
    }
    rebuildNetplaySnapshotIndexLocked();

    m_netplayDiagnostics.enabled = true;
    m_netplayDiagnostics.currentFrame = frame;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    m_netplayDiagnostics.latestSnapshotCrc32 = crc32;
    m_netplayDiagnostics.netplayCrcTiming.record(crcElapsedUs);
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

void ThreadedEmulationHost::refreshSnapshotLocked()
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
    snapshot.manualResetGeneration = m_emu.manualResetGeneration();
    snapshot.manualLoadStateGeneration = m_emu.manualLoadStateGeneration();
    snapshot.region = m_emu.region();
    snapshot.cartridgeSystem =
        m_emu.valid()
            ? m_emu.getConsole().cartridge().system()
            : GameDatabase::System::Unknown;
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
    snapshot.lastFrameReadyFrame = m_lastFrameReadyFrameValue;
    snapshot.lastFrameReadyNetplayCrc32 = m_lastFrameReadyNetplayCrc32Value;

    const bool holdPresentedFramebuffer =
        m_holdPresentedFramebufferUntilFrameReady.load(std::memory_order_acquire);
    if(!holdPresentedFramebuffer) {
        const int backIndex = 1 - m_frontFramebufferIndex.load(std::memory_order_relaxed);
        std::memcpy(
            m_framebuffers[backIndex].data(),
            m_emu.getFramebuffer(),
            m_framebuffers[backIndex].size() * sizeof(uint32_t)
        );
        m_frontFramebufferIndex.store(backIndex, std::memory_order_release);
    }

    {
        std::scoped_lock snapshotLock(m_snapshotMutex);
        m_snapshot = snapshot;
    }
}

void ThreadedEmulationHost::onFrameReadyLocked()
{
    recordFrameReadyNetplayState(m_emu);
    m_holdPresentedFramebufferUntilFrameReady.store(false, std::memory_order_release);
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
    {
        std::scoped_lock pendingInputLock(m_pendingInputMutex);
        m_pendingInput = input;
    }
    m_workerWakeRequested.store(true, std::memory_order_release);
    m_presenterCv.notify_one();
}

void ThreadedEmulationHost::setFrameInputResolver(FrameInputResolver resolver)
{
    {
        std::scoped_lock resolverLock(m_frameInputResolverMutex);
        m_frameInputResolver = std::move(resolver);
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
    std::scoped_lock snapshotLock(m_snapshotMutex);
    return m_snapshot.regionFps;
}

const uint32_t* ThreadedEmulationHost::getFramebuffer() const
{
    return m_framebuffers[m_frontFramebufferIndex.load(std::memory_order_acquire)].data();
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
        m_netplaySnapshots.pop_front();
    }
    rebuildNetplaySnapshotIndexLocked();
    m_netplayDiagnostics.enabled = m_netplaySnapshotCapacity > 0;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
}

bool ThreadedEmulationHost::rollbackToFrame(uint32_t frame)
{
    std::vector<uint8_t> snapshotData;
    {
        std::scoped_lock netplayLock(m_netplaySnapshotMutex);
        auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
        if(indexIt == m_netplaySnapshotIndexByFrame.end() ||
           indexIt->second >= m_netplaySnapshots.size() ||
           m_netplaySnapshots[indexIt->second].frame != frame) {
            return false;
        }
        snapshotData = m_netplaySnapshots[indexIt->second].data;
        m_netplayDiagnostics.rollbackSnapshotCopyBytes.record(snapshotData.size());
    }

    uint32_t rollbackFrom = 0;
    uint64_t rollbackLoadElapsedUs = 0;
    {
        std::scoped_lock emuLock(m_emuMutex);
        if(snapshotData.empty()) return false;
        m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
        rollbackFrom = m_emu.frameCount();
        const auto rollbackLoadStart = HostTimingClock::now();
        m_emu.loadStateFromMemoryWithAudioPolicy(
            snapshotData,
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
    if(data.empty()) return false;
    std::scoped_lock emuLock(m_emuMutex);
    m_holdPresentedFramebufferUntilFrameReady.store(true, std::memory_order_release);
    if(!m_emu.loadStateFromMemoryOnCleanBoot(data)) {
        return false;
    }
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

std::optional<std::vector<uint8_t>> ThreadedEmulationHost::netplaySnapshotForFrame(uint32_t frame) const
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt == m_netplaySnapshotIndexByFrame.end() ||
       indexIt->second >= m_netplaySnapshots.size() ||
       m_netplaySnapshots[indexIt->second].frame != frame) {
        return std::nullopt;
    }
    const auto& data = m_netplaySnapshots[indexIt->second].data;
    m_netplayDiagnostics.snapshotLookupCopyBytes.record(data.size());
    return data;
}

std::optional<uint32_t> ThreadedEmulationHost::netplaySnapshotCrc32ForFrame(uint32_t frame) const
{
    std::scoped_lock netplayLock(m_netplaySnapshotMutex);
    auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt == m_netplaySnapshotIndexByFrame.end() ||
       indexIt->second >= m_netplaySnapshots.size() ||
       m_netplaySnapshots[indexIt->second].frame != frame) {
        return std::nullopt;
    }
    return m_netplaySnapshots[indexIt->second].crc32;
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
    auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt != m_netplaySnapshotIndexByFrame.end() &&
       indexIt->second < m_netplaySnapshots.size() &&
       m_netplaySnapshots[indexIt->second].frame == frame) {
        auto& existing = m_netplaySnapshots[indexIt->second];
        existing.crc32 = crc32;
        existing.data = data;
    } else {
        m_netplaySnapshots.push_back(NetplayStoredSnapshot{frame, crc32, data});
        while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity && !m_netplaySnapshots.empty()) {
            m_netplaySnapshots.pop_front();
        }
    }
    rebuildNetplaySnapshotIndexLocked();
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
