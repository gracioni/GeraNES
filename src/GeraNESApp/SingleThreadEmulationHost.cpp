#include "GeraNESApp/SingleThreadEmulationHost.h"

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

void SingleThreadEmulationHost::rebuildNetplaySnapshotIndex()
{
    m_netplaySnapshotIndexByFrame.clear();
    for(size_t i = 0; i < m_netplaySnapshots.size(); ++i) {
        m_netplaySnapshotIndexByFrame[m_netplaySnapshots[i].frame] = i;
    }
}

void SingleThreadEmulationHost::onResetExecutedLocked(uint32_t frame)
{
    m_manualStateChanges.push_back(ManualStateChangeRecord{ManualStateChangeKind::Reset, frame});
}

void SingleThreadEmulationHost::onLoadExecutedLocked(uint32_t frame)
{
    m_manualStateChanges.push_back(ManualStateChangeRecord{ManualStateChangeKind::LoadState, frame});
}

void SingleThreadEmulationHost::recordFrameReadyNetplayState(GeraNESEmu& emu)
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

    if(m_netplaySnapshotCapacity == 0) {
        m_netplayDiagnostics.netplayStateSaveTiming.record(snapshotSaveElapsedUs);
        m_netplayDiagnostics.netplayStateSerializedBytes.record(snapshot.data.size());
        return;
    }

    if(snapshot.data.empty()) {
        m_netplayDiagnostics.netplayRollbackSnapshotSaveTiming.record(snapshotSaveElapsedUs);
        m_netplayDiagnostics.netplayRollbackSnapshotSerializedBytes.record(0);
        return;
    }
    const size_t snapshotDataSize = snapshot.data.size();

    auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt != m_netplaySnapshotIndexByFrame.end() &&
       indexIt->second < m_netplaySnapshots.size() &&
       m_netplaySnapshots[indexIt->second].frame == frame) {
        auto& existing = m_netplaySnapshots[indexIt->second];
        existing.crc32 = crc32;
        existing.data = std::move(snapshot.data);
    } else {
        m_netplaySnapshots.push_back(NetplayStoredSnapshot{frame, crc32, std::move(snapshot.data)});
        while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity) {
            m_netplaySnapshots.pop_front();
        }
    }
    rebuildNetplaySnapshotIndex();

    m_netplayDiagnostics.enabled = true;
    m_netplayDiagnostics.currentFrame = frame;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    m_netplayDiagnostics.latestSnapshotCrc32 = crc32;
    m_netplayDiagnostics.netplayRollbackSnapshotSaveTiming.record(snapshotSaveElapsedUs);
    m_netplayDiagnostics.netplayRollbackSnapshotSerializedBytes.record(snapshotDataSize);
}

void SingleThreadEmulationHost::resetFreeRunningPacing()
{
    m_freeRunningClockInitialized = false;
}

void SingleThreadEmulationHost::applyPendingInput()
{
    if(!m_autoQueuePendingInputOnFrameStart && !m_frameInputResolver) {
        return;
    }

    ReplayFrameInput input;
    if(m_frameInputResolver) {
        const uint32_t targetFrame = m_emu.frameCount() + 1u;
        if(!m_frameInputResolver(targetFrame, input)) {
            return;
        }
        queueReplayFrameInputToEmu(m_emu, targetFrame, input);
        return;
    }

    applyInputStateToEmu(m_emu, m_pendingInput);
}

bool SingleThreadEmulationHost::runPreAdvanceHook()
{
    if(m_preAdvanceHook) {
        const uint32_t beforeFrame = m_emu.frameCount();
        m_preAdvanceHook(m_emu);
        return m_emu.frameCount() != beforeFrame;
    }
    return false;
}

void SingleThreadEmulationHost::dispatchQueuedCommands()
{
    while(!m_pendingCommands.empty()) {
        std::function<void(GeraNESEmu&)> command = std::move(m_pendingCommands.front());
        m_pendingCommands.pop_front();
        if(command) {
            command(m_emu);
        }
    }
}

bool SingleThreadEmulationHost::pumpFreeRunningWorkerSteps()
{
    using clock = std::chrono::steady_clock;
    constexpr uint32_t STEP_MS = 1;
    const uint32_t fps = std::max<uint32_t>(1u, m_emu.getRegionFPS());
    const uint32_t maxCatchupSteps = ((1000u + fps - 1u) / fps) + 1u;

    auto now = clock::now();
    if(!m_freeRunningClockInitialized) {
        m_freeRunningClockInitialized = true;
        m_freeRunningNextTick = now;
    }

    const uint32_t frameBefore = m_emu.frameCount();
    uint32_t stepsToRun = 0;
    if(now >= m_freeRunningNextTick) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_freeRunningNextTick).count();
        stepsToRun = static_cast<uint32_t>(elapsed) + 1u;
        stepsToRun = std::min<uint32_t>(stepsToRun, maxCatchupSteps);
        m_freeRunningNextTick += std::chrono::milliseconds(stepsToRun * STEP_MS);
    }

    if(stepsToRun == 0u) {
        dispatchQueuedCommands();
    } else {
        for(uint32_t step = 0; step < stepsToRun; ++step) {
            dispatchQueuedCommands();
            if(runPreAdvanceHook()) {
                return m_emu.frameCount() != frameBefore;
            }
            if(m_emu.valid()) {
                m_emu.update(STEP_MS);
            }
        }
    }

    return m_emu.frameCount() != frameBefore;
}

void SingleThreadEmulationHost::serviceBackgroundWork()
{
    if(m_framePacingMode == FramePacingMode::Suspended) {
        dispatchQueuedCommands();
        (void)runPreAdvanceHook();
        return;
    }
    if(m_framePacingMode == FramePacingMode::FreeRunning) {
        (void)pumpFreeRunningWorkerSteps();
        return;
    }
    dispatchQueuedCommands();
}

void SingleThreadEmulationHost::onFrameReady()
{
    recordFrameReadyNetplayState(m_emu);
    m_holdPresentedFramebufferUntilFrameReady = false;
    std::memcpy(
        m_presentedFramebuffer.data(),
        m_emu.getFramebuffer(),
        m_presentedFramebuffer.size() * sizeof(uint32_t)
    );
}

void SingleThreadEmulationHost::refreshPresentedFramebuffer()
{
    if(m_holdPresentedFramebufferUntilFrameReady) return;
    std::memcpy(
        m_presentedFramebuffer.data(),
        m_emu.getFramebuffer(),
        m_presentedFramebuffer.size() * sizeof(uint32_t)
    );
}

SingleThreadEmulationHost::SingleThreadEmulationHost(IAudioOutput& audioOutput)
    : m_emu(audioOutput)
    , m_audioOutput(audioOutput)
{
    m_emu.signalResetExecuted.bind(&SingleThreadEmulationHost::onResetExecutedLocked, this);
    m_emu.signalLoadExecuted.bind(&SingleThreadEmulationHost::onLoadExecutedLocked, this);
    m_emu.signalFrameStart.bind(&SingleThreadEmulationHost::applyPendingInput, this);
    m_emu.signalFrameReady.bind(&SingleThreadEmulationHost::onFrameReady, this);
}

SingleThreadEmulationHost::~SingleThreadEmulationHost()
{
    shutdown();
}

void SingleThreadEmulationHost::shutdown()
{
    return;
}

void SingleThreadEmulationHost::setPendingInput(const InputState& input)
{
    m_pendingInput = input;
}

void SingleThreadEmulationHost::setFrameInputResolver(FrameInputResolver resolver)
{
    m_frameInputResolver = std::move(resolver);
}

void SingleThreadEmulationHost::queueInputForFrame(uint32_t frameNumber, const InputState& input)
{
    postCommand([frameNumber, input](GeraNESEmu& emu) {
        emu.queueInputFrame(buildInputFrameForEmu(emu, frameNumber, input));
    });
}

void SingleThreadEmulationHost::queueInputFrames(const std::vector<std::pair<uint32_t, InputState>>& inputs)
{
    if(inputs.empty()) return;
    postCommand([inputs](GeraNESEmu& emu) {
        for(const auto& [frameNumber, input] : inputs) {
            emu.queueInputFrame(buildInputFrameForEmu(emu, frameNumber, input));
        }
    });
}

void SingleThreadEmulationHost::setAutoQueuePendingInputOnFrameStart(bool enabled)
{
    m_autoQueuePendingInputOnFrameStart = enabled;
}

void SingleThreadEmulationHost::setAllowPresenterTimeoutAdvance(bool enabled)
{
    m_allowPresenterTimeoutAdvance = enabled;
}

void SingleThreadEmulationHost::setPreAdvanceHook(std::function<void(GeraNESEmu&)> hook)
{
    m_preAdvanceHook = std::move(hook);
}

void SingleThreadEmulationHost::setDebugTraceSink(std::function<void(const std::string&)> sink)
{
    (void)sink;
}

void SingleThreadEmulationHost::postCommand(std::function<void(GeraNESEmu&)> command)
{
    if(command) {
        m_pendingCommands.push_back(std::move(command));
    }
}

bool SingleThreadEmulationHost::open(const std::string& path)
{
    resetFreeRunningPacing();
    m_hasCachedNetplayCrc = false;
    const bool opened = m_emu.open(path);
    if(opened) {
        const uint32_t bootstrapFrame = m_emu.frameCount();
        ReplayFrameInput bootstrapInput;
        if(m_frameInputResolver && m_frameInputResolver(bootstrapFrame, bootstrapInput)) {
            queueReplayFrameInputToEmu(m_emu, bootstrapFrame, bootstrapInput);
        } else {
            InputFrame frame = buildInputFrameForEmu(m_emu, bootstrapFrame, m_pendingInput);
            m_emu.queueInputFrame(frame);
        }
    }
    refreshPresentedFramebuffer();
    return opened;
}

uint32_t SingleThreadEmulationHost::manualResetGeneration() const
{
    return m_emu.manualResetGeneration();
}

uint32_t SingleThreadEmulationHost::manualLoadStateGeneration() const
{
    return m_emu.manualLoadStateGeneration();
}

uint32_t SingleThreadEmulationHost::exactEmulationFrame() const
{
    const_cast<SingleThreadEmulationHost*>(this)->serviceBackgroundWork();
    return m_emu.frameCount();
}

uint32_t SingleThreadEmulationHost::getRegionFPS() const
{
    return m_emu.getRegionFPS();
}

const uint32_t* SingleThreadEmulationHost::getFramebuffer() const
{
    return m_presentedFramebuffer.data();
}

void SingleThreadEmulationHost::beginPresentationHoldUntilNextFrameReady()
{
    m_holdPresentedFramebufferUntilFrameReady = true;
}

void SingleThreadEmulationHost::setPresenterLockActive(bool active)
{
    if(active) {
        m_framePacingMode = FramePacingMode::PresenterLocked;
    } else {
        m_framePacingMode = FramePacingMode::FreeRunning;
        m_pendingPresenterTicks = 0;
        resetFreeRunningPacing();
    }
}

void SingleThreadEmulationHost::setSimulationSuspended(bool suspended)
{
    if(suspended) {
        m_framePacingMode = FramePacingMode::Suspended;
        m_pendingPresenterTicks = 0;
        resetFreeRunningPacing();
    } else if(m_framePacingMode == FramePacingMode::Suspended) {
        m_framePacingMode = FramePacingMode::FreeRunning;
        resetFreeRunningPacing();
    }
}

bool SingleThreadEmulationHost::update(uint32_t dt)
{
    (void)dt;
    if(m_framePacingMode != FramePacingMode::Suspended) {
        m_framePacingMode = FramePacingMode::FreeRunning;
    }
    m_pendingPresenterTicks = 0;
    if(m_framePacingMode == FramePacingMode::Suspended) {
        dispatchQueuedCommands();
        (void)runPreAdvanceHook();
        refreshPresentedFramebuffer();
        return false;
    }
    const bool advanced = pumpFreeRunningWorkerSteps();
    refreshPresentedFramebuffer();
    return advanced;
}

void SingleThreadEmulationHost::updateUntilFrame(uint32_t dt)
{
    m_presenterTickDtMs = std::max<uint32_t>(1u, dt);
    m_framePacingMode = FramePacingMode::PresenterLocked;
    ++m_pendingPresenterTicks;
    dispatchQueuedCommands();
    if(m_framePacingMode == FramePacingMode::Suspended) {
        (void)runPreAdvanceHook();
        refreshPresentedFramebuffer();
        return;
    }
    const uint32_t frameBefore = m_emu.frameCount();
    if(runPreAdvanceHook()) {
        refreshPresentedFramebuffer();
        return;
    }
    if(m_emu.valid() && m_pendingPresenterTicks > 0u) {
        m_emu.updateUntilFrame(m_presenterTickDtMs);
        m_pendingPresenterTicks = 0u;
    } else if(m_emu.valid() && m_allowPresenterTimeoutAdvance) {
        m_emu.update(m_presenterTickDtMs);
    }
    refreshPresentedFramebuffer();
    (void)frameBefore;
}

void SingleThreadEmulationHost::configureNetplaySnapshots(size_t snapshotCapacity)
{
    m_netplaySnapshotCapacity = snapshotCapacity;
    while(m_netplaySnapshots.size() > m_netplaySnapshotCapacity) {
        m_netplaySnapshots.pop_front();
    }
    rebuildNetplaySnapshotIndex();
    m_netplayDiagnostics.enabled = m_netplaySnapshotCapacity > 0;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
}

bool SingleThreadEmulationHost::rollbackToFrame(uint32_t frame)
{
    const std::vector<uint8_t>* snapshotData = nullptr;
    auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt == m_netplaySnapshotIndexByFrame.end() ||
       indexIt->second >= m_netplaySnapshots.size() ||
       m_netplaySnapshots[indexIt->second].frame != frame) {
        return false;
    }
    snapshotData = &m_netplaySnapshots[indexIt->second].data;

    if(snapshotData == nullptr || snapshotData->empty()) return false;
    m_holdPresentedFramebufferUntilFrameReady = true;
    const uint32_t rollbackFrom = m_emu.frameCount();
    const auto rollbackLoadStart = HostTimingClock::now();
    m_emu.loadStateFromMemoryWithAudioPolicy(
        *snapshotData,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    const uint64_t rollbackLoadElapsedUs = elapsedMicrosSince(rollbackLoadStart);
    m_netplayDiagnostics.rollbackLoadTiming.record(rollbackLoadElapsedUs);
    const bool loaded = m_emu.valid();
    if(loaded) {
        resetFreeRunningPacing();
        m_hasCachedNetplayCrc = false;
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

std::vector<uint8_t> SingleThreadEmulationHost::saveStateToMemory()
{
    return m_emu.saveStateToMemory();
}

std::vector<uint8_t> SingleThreadEmulationHost::saveNetplayStateToMemory()
{
    const auto saveStart = HostTimingClock::now();
    std::vector<uint8_t> data = m_emu.saveNetplayStateToMemory();
    m_netplayDiagnostics.netplayStateSaveTiming.record(elapsedMicrosSince(saveStart));
    m_netplayDiagnostics.netplayStateSerializedBytes.record(data.size());
    return data;
}

bool SingleThreadEmulationHost::loadStateFromMemory(const std::vector<uint8_t>& data)
{
    if(data.empty()) return false;
    m_holdPresentedFramebufferUntilFrameReady = true;
    const bool loaded = m_emu.loadStateFromMemoryOnCleanBoot(data);
    if(loaded) {
        resetFreeRunningPacing();
        m_hasCachedNetplayCrc = false;
        refreshPresentedFramebuffer();
    }
    return loaded;
}

bool SingleThreadEmulationHost::loadStateFromMemoryAsManualStateChange(const std::vector<uint8_t>& data)
{
    if(data.empty()) return false;
    m_holdPresentedFramebufferUntilFrameReady = true;
    const bool loaded = m_emu.loadStateFromMemoryOnCleanBoot(data);
    if(loaded) {
        resetFreeRunningPacing();
        m_hasCachedNetplayCrc = false;
        onLoadExecutedLocked(m_emu.frameCount());
        refreshPresentedFramebuffer();
    }
    return loaded;
}

uint32_t SingleThreadEmulationHost::canonicalStateCrc32()
{
    return m_emu.canonicalStateCrc32();
}

uint32_t SingleThreadEmulationHost::canonicalNetplayStateCrc32()
{
    const uint32_t frame = m_emu.frameCount();
    if(m_hasCachedNetplayCrc && m_cachedNetplayCrcFrame == frame) {
        return m_cachedNetplayCrcValue;
    }
    const auto crcStart = HostTimingClock::now();
    const uint32_t crc = m_emu.canonicalNetplayStateCrc32();
    m_netplayDiagnostics.netplayCrcTiming.record(elapsedMicrosSince(crcStart));
    m_hasCachedNetplayCrc = true;
    m_cachedNetplayCrcFrame = frame;
    m_cachedNetplayCrcValue = crc;
    return crc;
}

uint32_t SingleThreadEmulationHost::lastFrameReadyFrame() const
{
    return m_lastFrameReadyFrameValue;
}

uint32_t SingleThreadEmulationHost::lastFrameReadyNetplayCrc32() const
{
    return m_lastFrameReadyNetplayCrc32Value;
}

void SingleThreadEmulationHost::setAuthoritativeFrameReadyState(uint32_t frame, uint32_t canonicalCrc32)
{
    m_lastFrameReadyFrameValue = frame;
    m_lastFrameReadyNetplayCrc32Value = canonicalCrc32;
}

std::optional<std::vector<uint8_t>> SingleThreadEmulationHost::netplaySnapshotForFrame(uint32_t frame) const
{
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

std::optional<uint32_t> SingleThreadEmulationHost::netplaySnapshotCrc32ForFrame(uint32_t frame) const
{
    auto indexIt = m_netplaySnapshotIndexByFrame.find(frame);
    if(indexIt == m_netplaySnapshotIndexByFrame.end() ||
       indexIt->second >= m_netplaySnapshots.size() ||
       m_netplaySnapshots[indexIt->second].frame != frame) {
        return std::nullopt;
    }
    return m_netplaySnapshots[indexIt->second].crc32;
}

void SingleThreadEmulationHost::seedNetplaySnapshot(
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
    rebuildNetplaySnapshotIndex();
    m_netplayDiagnostics.enabled = m_netplaySnapshotCapacity > 0;
    m_netplayDiagnostics.currentFrame = frame;
    m_netplayDiagnostics.snapshotCapacity = m_netplaySnapshotCapacity;
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    m_netplayDiagnostics.latestSnapshotCrc32 = crc32;
    m_netplayDiagnostics.seededSnapshotCopyBytes.record(data.size());
}

SingleThreadEmulationHost::NetplayDiagnosticsSnapshot SingleThreadEmulationHost::getNetplayDiagnostics() const
{
    return m_netplayDiagnostics;
}
