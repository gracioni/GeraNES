#include "GeraNESApp/SingleThreadEmulationHost.h"

#include <cstring>

namespace
{
using HostTimingClock = std::chrono::steady_clock;

struct SaveStateWithCrc32
{
    std::vector<uint8_t> data;
    uint32_t crc32 = 0;
};

uint64_t elapsedMicrosSince(HostTimingClock::time_point start)
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            HostTimingClock::now() - start
        ).count()
    );
}

SaveStateWithCrc32 captureSaveStateWithCrc32(GeraNESEmu& emu)
{
    SaveStateWithCrc32 snapshot;
    snapshot.data = emu.saveStateToMemory();
    snapshot.crc32 = snapshot.data.empty()
        ? 0u
        : Crc32::calc(reinterpret_cast<const char*>(snapshot.data.data()), snapshot.data.size());
    return snapshot;
}

}

std::optional<size_t> SingleThreadEmulationHost::snapshotIndexForFrame(uint32_t frame) const
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

void SingleThreadEmulationHost::discardNetplaySnapshotsAfter(uint32_t frame)
{
    while(!m_netplaySnapshots.empty() && m_netplaySnapshots.back().frame > frame) {
        m_netplaySnapshotIndexByFrame.erase(m_netplaySnapshots.back().frame);
        m_netplaySnapshots.pop_back();
        if(m_netplaySnapshotNextPosition > m_netplaySnapshotHeadPosition) {
            --m_netplaySnapshotNextPosition;
        }
    }
    m_netplayDiagnostics.storedSnapshots = m_netplaySnapshots.size();
    m_netplayDiagnostics.currentFrame = frame;
}

bool SingleThreadEmulationHost::resolveReplayPlaybackInput(uint32_t targetFrame,
                                                           ReplayFrameInput& input,
                                                           bool& handled)
{
    handled = false;
    if(!m_replayPlayback.loaded) {
        return true;
    }

    if(!m_replayPlayback.playing && !m_replayPlayback.seeking) {
        handled = true;
        m_pendingInputFrames.clear();
        m_emu.setPaused(true);
        return false;
    }

    handled = true;
    const InputFrame* replayFrame = findReplayFrameByNumber(m_replayPlayback.frames, targetFrame);
    if(replayFrame == nullptr) {
        m_replayPlayback.playing = false;
        m_replayPlayback.seeking = false;
        m_pendingInputFrames.clear();
        m_emu.setPaused(true);
        return false;
    }

    input.hasFrameOverride = true;
    input.frameOverride = *replayFrame;
    return true;
}

void SingleThreadEmulationHost::resetReplayPlaybackSnapshots()
{
    m_replayPlayback.cursorFrame = m_emu.frameCount();
    m_replayPlayback.cursorCanonicalCrc32.reset();
    m_replayPlayback.scheduledSnapshotFrames.clear();
    m_replayPlayback.snapshots.clear();

    const uint32_t replayFrameCount = replayFrameCountFromFrames(m_replayPlayback.frames);
    if(replayFrameCount > 1u) {
        constexpr size_t kReplaySnapshotCapacity = 10u;
        for(uint32_t i = 1; i <= kReplaySnapshotCapacity; ++i) {
            const uint32_t frame = (i * replayFrameCount) / (static_cast<uint32_t>(kReplaySnapshotCapacity) + 1u);
            if(frame == 0 || frame >= replayFrameCount) {
                continue;
            }
            if(!m_replayPlayback.scheduledSnapshotFrames.empty() &&
               m_replayPlayback.scheduledSnapshotFrames.back() == frame) {
                continue;
            }
            m_replayPlayback.scheduledSnapshotFrames.push_back(frame);
        }
    }

    const SaveStateWithCrc32 baseline = captureSaveStateWithCrc32(m_emu);
    captureReplayPlaybackSnapshot(m_emu.frameCount(), baseline.data, baseline.crc32);
    m_replayPlayback.cursorCanonicalCrc32 = baseline.crc32;
}

void SingleThreadEmulationHost::captureReplayPlaybackSnapshot(uint32_t frame,
                                                              const std::vector<uint8_t>& state,
                                                              uint32_t crc32)
{
    if(state.empty()) {
        return;
    }

    const auto existing = std::find_if(
        m_replayPlayback.snapshots.begin(),
        m_replayPlayback.snapshots.end(),
        [frame](const ReplayPlaybackState::StoredSnapshot& snapshot) {
            return snapshot.frame == frame;
        });
    if(existing != m_replayPlayback.snapshots.end()) {
        existing->crc32 = crc32;
        existing->data = std::make_shared<std::vector<uint8_t>>(state);
        return;
    }

    m_replayPlayback.snapshots.push_back(ReplayPlaybackState::StoredSnapshot{
        frame,
        crc32,
        std::make_shared<std::vector<uint8_t>>(state)
    });
}

void SingleThreadEmulationHost::updateReplayPlaybackFrameReadyState()
{
    if(!m_replayPlayback.loaded) {
        return;
    }

    const uint32_t frame = m_emu.frameCount();
    m_replayPlayback.cursorFrame = frame;
    m_replayPlayback.cursorCanonicalCrc32.reset();
    const bool shouldCapture = std::find(
        m_replayPlayback.scheduledSnapshotFrames.begin(),
        m_replayPlayback.scheduledSnapshotFrames.end(),
        frame) != m_replayPlayback.scheduledSnapshotFrames.end();
    if(!shouldCapture || !m_lastFrameReadyStateSnapshot) {
        return;
    }

    const SaveStateWithCrc32 snapshot = captureSaveStateWithCrc32(m_emu);
    captureReplayPlaybackSnapshot(frame, snapshot.data, snapshot.crc32);
    m_replayPlayback.cursorCanonicalCrc32 = snapshot.crc32;
}

bool SingleThreadEmulationHost::seekReplayPlayback(uint32_t targetFrame)
{
    if(!m_replayPlayback.loaded || !m_emu.valid()) {
        return false;
    }

    const uint32_t clampedTarget =
        std::min(targetFrame, replayFrameCountFromFrames(m_replayPlayback.frames));
    const auto bestSnapshot = std::find_if(
        m_replayPlayback.snapshots.rbegin(),
        m_replayPlayback.snapshots.rend(),
        [clampedTarget](const ReplayPlaybackState::StoredSnapshot& snapshot) {
            return snapshot.frame <= clampedTarget && snapshot.data && !snapshot.data->empty();
        });
    if(bestSnapshot == m_replayPlayback.snapshots.rend()) {
        return false;
    }

    m_replayPlayback.playing = false;
    m_replayPlayback.seeking = true;
    m_pendingInputFrames.clear();
    m_emu.loadStateFromMemory(*bestSnapshot->data);
    if(!m_emu.valid()) {
        m_replayPlayback.seeking = false;
        return false;
    }

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, m_emu.getRegionFPS()));
    if(m_emu.frameCount() < clampedTarget && m_emu.paused()) {
        m_emu.setPaused(false);
    }
    while(m_emu.frameCount() < clampedTarget) {
        runPreAdvanceHook();
        if(!prepareCurrentFrameInput()) {
            m_replayPlayback.seeking = false;
            return false;
        }
        const uint32_t frameBefore = m_emu.frameCount();
        m_emu.updateUntilFrame(frameDt, false);
        if(m_emu.frameCount() <= frameBefore) {
            m_replayPlayback.seeking = false;
            return false;
        }
        onFrameReady();
    }

    m_replayPlayback.seeking = false;
    m_pendingInputFrames.clear();
    m_emu.setPaused(true);
    const SaveStateWithCrc32 settledState = captureSaveStateWithCrc32(m_emu);
    m_replayPlayback.cursorFrame = m_emu.frameCount();
    m_replayPlayback.cursorCanonicalCrc32 = settledState.crc32;
    if(std::find(
           m_replayPlayback.scheduledSnapshotFrames.begin(),
           m_replayPlayback.scheduledSnapshotFrames.end(),
           m_replayPlayback.cursorFrame) != m_replayPlayback.scheduledSnapshotFrames.end()) {
        captureReplayPlaybackSnapshot(m_replayPlayback.cursorFrame, settledState.data, settledState.crc32);
    }

    return true;
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
    const bool captureReplaySnapshot =
        m_replayPlayback.loaded &&
        std::find(
            m_replayPlayback.scheduledSnapshotFrames.begin(),
            m_replayPlayback.scheduledSnapshotFrames.end(),
            frame) != m_replayPlayback.scheduledSnapshotFrames.end();
    if(m_netplaySnapshotCapacity == 0 && !captureReplaySnapshot) {
        m_lastFrameReadyStateSnapshot.reset();
        m_lastFrameReadyFrameValue = frame;
        m_lastFrameReadyNetplayCrc32Value = 0;
        return;
    }

    const auto snapshotSaveStart = HostTimingClock::now();
    SaveStateWithCrc32 snapshot = captureSaveStateWithCrc32(emu);
    const uint64_t snapshotSaveElapsedUs = elapsedMicrosSince(snapshotSaveStart);
    const uint32_t crc32 = snapshot.crc32;
    const size_t snapshotDataSize = snapshot.data.size();
    m_lastFrameReadyStateSnapshot = std::make_shared<std::vector<uint8_t>>(snapshot.data);
    m_lastFrameReadyFrameValue = frame;
    m_lastFrameReadyNetplayCrc32Value = crc32;
    if(m_netplaySnapshotCapacity == 0) {
        m_netplayDiagnostics.netplayStateSaveTiming.record(snapshotSaveElapsedUs);
        m_netplayDiagnostics.netplayStateSerializedBytes.record(snapshotDataSize);
        return;
    }

    if(snapshot.data.empty()) {
        m_netplayDiagnostics.netplaySnapshotSaveTiming.record(snapshotSaveElapsedUs);
        m_netplayDiagnostics.netplaySnapshotSerializedBytes.record(0);
        return;
    }

    if(const std::optional<size_t> index = snapshotIndexForFrame(frame); index.has_value()) {
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
    m_netplayDiagnostics.netplaySnapshotSaveTiming.record(snapshotSaveElapsedUs);
    m_netplayDiagnostics.netplaySnapshotSerializedBytes.record(snapshotDataSize);
}

void SingleThreadEmulationHost::resetFreeRunningPacing()
{
    m_freeRunningClockInitialized = false;
}

void SingleThreadEmulationHost::notifyQueuedInputObserver(const InputFrame& frame)
{
    if(m_queuedInputObserver) {
        m_queuedInputObserver(frame);
    }
}

void SingleThreadEmulationHost::notifySelectedInputObserver(const InputFrame& frame)
{
    if(m_selectedInputObserver) {
        m_selectedInputObserver(frame);
    }
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
    const uint32_t fps = std::max<uint32_t>(1u, m_emu.getRegionFPS());
    const uint32_t frameDtMs = std::max<uint32_t>(1u, 1000u / fps);
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
        stepsToRun = static_cast<uint32_t>(elapsed / std::max<int64_t>(1, frameDtMs)) + 1u;
        stepsToRun = std::min<uint32_t>(stepsToRun, maxCatchupSteps);
        m_freeRunningNextTick += std::chrono::milliseconds(stepsToRun * frameDtMs);
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
                const uint32_t stepFrameBefore = m_emu.frameCount();
                prepareCurrentFrameInput();
                m_emu.updateUntilFrame(frameDtMs);
                const uint32_t stepFrameAfter = m_emu.frameCount();
                if(stepFrameAfter != stepFrameBefore) {
                    onFrameReady();
                }
            }
        }
    }

    return m_emu.frameCount() != frameBefore;
}

void SingleThreadEmulationHost::serviceBackgroundWork()
{
    if(m_framePacingMode == FramePacingMode::Suspended) {
        dispatchQueuedCommands();
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
    updateReplayPlaybackFrameReadyState();
    m_holdPresentedFramebufferUntilFrameReady = false;
    if(!m_emu.valid()) {
        std::fill(m_presentedFramebuffer.begin(), m_presentedFramebuffer.end(), 0u);
        m_modRenderSnapshot = {};
        m_presentedModFramebuffer.clear();
        refreshPpuViewerSnapshot();
        refreshPpuEventViewerSnapshot();
        return;
    }
    std::memcpy(
        m_presentedFramebuffer.data(),
        m_emu.getFramebuffer(),
        m_presentedFramebuffer.size() * sizeof(uint32_t)
    );

    refreshPpuViewerSnapshot();
    refreshPpuEventViewerSnapshot();

    m_modRenderSnapshot = {};
    m_presentedModFramebuffer.clear();
    if(m_modFrameCaptureHook) {
        m_modFrameCaptureHook(m_emu, m_modRenderSnapshot, m_presentedModFramebuffer);
    }
}

void SingleThreadEmulationHost::refreshPresentedFramebuffer()
{
    if(m_holdPresentedFramebufferUntilFrameReady) return;
    if(!m_emu.valid()) {
        std::fill(m_presentedFramebuffer.begin(), m_presentedFramebuffer.end(), 0u);
    }
}

void SingleThreadEmulationHost::refreshPpuViewerSnapshot()
{
    if(!m_ppuViewerCaptureEnabled) {
        return;
    }

    const uint32_t frameCount = m_emu.frameCount();
    const PPU& ppu = m_emu.getConsole().ppu();
    const int ppuScanline = ppu.scanline();
    const int ppuCycle = ppu.cycle();
    if(m_ppuViewerSnapshot.valid &&
       m_ppuViewerSnapshot.frameCount == frameCount &&
       (!m_ppuViewerScanlineCaptureEnabled ||
        (m_ppuViewerSnapshot.ppuScanline == ppuScanline &&
         m_ppuViewerSnapshot.ppuCycle == ppuCycle))) {
        return;
    }

    m_ppuViewerSnapshot = {};
    m_ppuViewerSnapshot.valid = m_emu.valid();
    m_ppuViewerSnapshot.frameCount = frameCount;
    m_ppuViewerSnapshot.ppuScanline = ppuScanline;
    m_ppuViewerSnapshot.ppuCycle = ppuCycle;
    if(!m_ppuViewerSnapshot.valid) {
        return;
    }

    m_ppuViewerSnapshot.rgbPalette = ppu.colorPalette();
    m_ppuViewerSnapshot.scrollX = ppu.getCursorX();
    m_ppuViewerSnapshot.scrollY = ppu.getCursorY();
    m_ppuViewerSnapshot.backgroundPatternTableAddress = ppu.debugBackgroundPatternTableAddress();
    for(size_t i = 0; i < m_ppuViewerSnapshot.chrData.size(); ++i) {
        m_ppuViewerSnapshot.chrData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(i));
    }
    for(size_t i = 0; i < m_ppuViewerSnapshot.nametableData.size(); ++i) {
        m_ppuViewerSnapshot.nametableData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x2000 + i));
    }
    for(size_t i = 0; i < m_ppuViewerSnapshot.paletteData.size(); ++i) {
        m_ppuViewerSnapshot.paletteData[i] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x3F00 + i));
    }
}

void SingleThreadEmulationHost::refreshPpuEventViewerSnapshot()
{
    if(m_emu.ppuEventTraceEnabled() != m_ppuEventViewerCaptureEnabled) {
        m_emu.enablePpuEventTrace(m_ppuEventViewerCaptureEnabled);
    }

    if(!m_ppuEventViewerCaptureEnabled) {
        m_ppuEventViewerSnapshot = {};
        return;
    }

    const uint32_t frameCount = m_emu.frameCount();
    if(m_ppuEventViewerSnapshot.valid &&
       m_ppuEventViewerSnapshot.traceEnabled &&
       m_ppuEventViewerSnapshot.frameCount == frameCount) {
        return;
    }

    m_ppuEventViewerSnapshot = {};
    m_ppuEventViewerSnapshot.valid = m_emu.valid();
    m_ppuEventViewerSnapshot.traceEnabled = true;
    m_ppuEventViewerSnapshot.frameCount = frameCount;
    m_ppuEventViewerSnapshot.events = m_emu.ppuRegisterAccessEvents();
    if(m_ppuEventViewerSnapshot.valid) {
        m_ppuEventViewerSnapshot.framebuffer.assign(
            m_presentedFramebuffer.begin(),
            m_presentedFramebuffer.end()
        );
    }
}

SingleThreadEmulationHost::SingleThreadEmulationHost(IAudioOutput& audioOutput)
    : m_emu(audioOutput)
    , m_audioOutput(audioOutput)
{
    m_emu.signalResetExecuted.bind(&SingleThreadEmulationHost::onResetExecutedLocked, this);
    m_emu.signalLoadExecuted.bind(&SingleThreadEmulationHost::onLoadExecutedLocked, this);
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

void SingleThreadEmulationHost::setQueuedInputObserver(QueuedInputObserver observer)
{
    m_queuedInputObserver = std::move(observer);
}

void SingleThreadEmulationHost::setSelectedInputObserver(SelectedInputObserver observer)
{
    m_selectedInputObserver = std::move(observer);
}

void SingleThreadEmulationHost::queueInputForFrame(uint32_t frameNumber, const InputState& input)
{
    postCommand([this, frameNumber, input](GeraNESEmu& emu) {
        m_pendingInputFrames.set(buildInputFrameForEmu(emu, frameNumber, input));
    });
}

void SingleThreadEmulationHost::queueInputFrames(const std::vector<std::pair<uint32_t, InputState>>& inputs)
{
    if(inputs.empty()) return;
    postCommand([this, inputs](GeraNESEmu& emu) {
        for(const auto& [frameNumber, input] : inputs) {
            m_pendingInputFrames.set(buildInputFrameForEmu(emu, frameNumber, input));
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

void SingleThreadEmulationHost::setModFrameCaptureHook(ModFrameCaptureHook hook)
{
    m_modFrameCaptureHook = std::move(hook);
    if(!m_modFrameCaptureHook) {
        m_modRenderSnapshot = {};
        m_presentedModFramebuffer.clear();
    }
}

void SingleThreadEmulationHost::setDebugTraceSink(std::function<void(const std::string&)> sink)
{
    (void)sink;
}

void SingleThreadEmulationHost::setPpuViewerCaptureEnabled(bool enabled, bool scanlineTrace)
{
    if(m_ppuViewerCaptureEnabled == enabled && m_ppuViewerScanlineCaptureEnabled == scanlineTrace) {
        return;
    }
    m_ppuViewerCaptureEnabled = enabled;
    m_ppuViewerScanlineCaptureEnabled = enabled && scanlineTrace;
    if(!enabled) {
        m_ppuViewerSnapshot = {};
    } else {
        serviceBackgroundWork();
        refreshPpuViewerSnapshot();
    }
}

bool SingleThreadEmulationHost::getPpuViewerSnapshot(PpuViewerSnapshot& out) const
{
    out = m_ppuViewerSnapshot;
    return out.valid;
}

void SingleThreadEmulationHost::setPpuEventViewerCaptureEnabled(bool enabled)
{
    if(m_ppuEventViewerCaptureEnabled == enabled) {
        return;
    }
    m_ppuEventViewerCaptureEnabled = enabled;
    serviceBackgroundWork();
    refreshPpuEventViewerSnapshot();
}

bool SingleThreadEmulationHost::getPpuEventViewerSnapshot(PpuEventViewerSnapshot& out) const
{
    out = m_ppuEventViewerSnapshot;
    return out.valid || out.traceEnabled;
}

void SingleThreadEmulationHost::postCommand(std::function<void(GeraNESEmu&)> command)
{
    if(command) {
        m_pendingCommands.push_back(std::move(command));
    }
}

bool SingleThreadEmulationHost::open(const std::string& path, bool autoConfigureInputTopologyOnRomLoad)
{
    resetFreeRunningPacing();
    const bool opened = m_emu.openRom(path, autoConfigureInputTopologyOnRomLoad);
    refreshPresentedFramebuffer();
    return opened;
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

void SingleThreadEmulationHost::copyFramebuffer(std::vector<uint32_t>& out) const
{
    out = m_presentedFramebuffer;
}

bool SingleThreadEmulationHost::getModRenderSnapshot(ModRenderSnapshot& out) const
{
    out = m_modRenderSnapshot;
    return out.valid;
}

bool SingleThreadEmulationHost::getModRenderFrame(ModRenderSnapshot& snapshot, std::vector<uint32_t>& framebuffer) const
{
    snapshot = m_modRenderSnapshot;
    framebuffer = m_presentedModFramebuffer;
    return snapshot.valid && !framebuffer.empty();
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
        refreshPresentedFramebuffer();
        return;
    }
    const uint32_t frameBefore = m_emu.frameCount();
    if(runPreAdvanceHook()) {
        refreshPresentedFramebuffer();
        return;
    }
    if(m_emu.valid() && m_pendingPresenterTicks > 0u) {
        const uint32_t frameBefore = m_emu.frameCount();
        prepareCurrentFrameInput();
        m_emu.updateUntilFrame(m_presenterTickDtMs);
        const uint32_t frameAfter = m_emu.frameCount();
        if(frameAfter != frameBefore) {
            onFrameReady();
        }
        m_pendingPresenterTicks = 0u;
    } else if(m_emu.valid() && m_allowPresenterTimeoutAdvance) {
        const uint32_t frameBefore = m_emu.frameCount();
        prepareCurrentFrameInput();
        m_emu.updateUntilFrame(m_presenterTickDtMs);
        const uint32_t frameAfter = m_emu.frameCount();
        if(frameAfter != frameBefore) {
            onFrameReady();
        }
    }
    refreshPresentedFramebuffer();
    (void)frameBefore;
}

void SingleThreadEmulationHost::configureNetplaySnapshots(size_t snapshotCapacity)
{
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

std::vector<uint8_t> SingleThreadEmulationHost::saveStateToMemory()
{
    const auto saveStart = HostTimingClock::now();
    std::vector<uint8_t> data = m_emu.saveStateToMemory();
    m_netplayDiagnostics.netplayStateSaveTiming.record(elapsedMicrosSince(saveStart));
    m_netplayDiagnostics.netplayStateSerializedBytes.record(data.size());
    return data;
}

bool SingleThreadEmulationHost::loadStateFromMemory(const std::vector<uint8_t>& data)
{
    return loadStateFromMemoryOnCleanBoot(data);
}

bool SingleThreadEmulationHost::loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& data)
{
    if(data.empty()) return false;
    m_holdPresentedFramebufferUntilFrameReady = true;
    const bool loaded = m_emu.loadStateFromMemoryOnCleanBoot(data);
    if(loaded) {
        resetFreeRunningPacing();
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
        onLoadExecutedLocked(m_emu.frameCount());
        refreshPresentedFramebuffer();
    }
    return loaded;
}

uint32_t SingleThreadEmulationHost::lastFrameReadyFrame() const
{
    return m_lastFrameReadyFrameValue;
}

uint32_t SingleThreadEmulationHost::lastFrameReadyNetplayCrc32() const
{
    return m_lastFrameReadyNetplayCrc32Value;
}

std::optional<std::shared_ptr<const std::vector<uint8_t>>> SingleThreadEmulationHost::lastFrameReadyStateSnapshot() const
{
    if(!m_lastFrameReadyStateSnapshot) {
        return std::nullopt;
    }
    return m_lastFrameReadyStateSnapshot;
}

void SingleThreadEmulationHost::setAuthoritativeFrameReadyState(uint32_t frame, uint32_t canonicalCrc32)
{
    m_lastFrameReadyFrameValue = frame;
    m_lastFrameReadyNetplayCrc32Value = canonicalCrc32;
}

std::optional<std::shared_ptr<const std::vector<uint8_t>>> SingleThreadEmulationHost::netplaySnapshotForFrame(uint32_t frame) const
{
    const std::optional<size_t> index = snapshotIndexForFrame(frame);
    if(!index.has_value()) {
        return std::nullopt;
    }
    const std::shared_ptr<const std::vector<uint8_t>> data = m_netplaySnapshots[*index].data;
    m_netplayDiagnostics.snapshotLookupCopyBytes.record(0);
    return data;
}

std::optional<uint32_t> SingleThreadEmulationHost::netplaySnapshotCrc32ForFrame(uint32_t frame) const
{
    const std::optional<size_t> index = snapshotIndexForFrame(frame);
    if(!index.has_value()) {
        return std::nullopt;
    }
    return m_netplaySnapshots[*index].crc32;
}

bool SingleThreadEmulationHost::updateNetplaySnapshotCrc32ForFrame(uint32_t frame, uint32_t canonicalCrc32)
{
    const std::optional<size_t> index = snapshotIndexForFrame(frame);
    if(!index.has_value()) {
        return false;
    }
    m_netplaySnapshots[*index].crc32 = canonicalCrc32;
    m_netplayDiagnostics.latestSnapshotCrc32 = canonicalCrc32;
    m_netplayDiagnostics.currentFrame = frame;
    return true;
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
    if(const std::optional<size_t> index = snapshotIndexForFrame(frame); index.has_value()) {
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

SingleThreadEmulationHost::NetplayDiagnosticsSnapshot SingleThreadEmulationHost::getNetplayDiagnostics() const
{
    return m_netplayDiagnostics;
}
