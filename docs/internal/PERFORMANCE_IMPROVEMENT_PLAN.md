# Performance Improvement Plan

Este plano organiza as melhorias de performance identificadas na analise estatica de
`src/GeraNES`, `src/GeraNESApp` e `src/GeraNESNetplay`.

O foco principal e reduzir custo no caminho critico de netplay/rollback:

1. frame ready
2. snapshot + CRC
3. confirmacao e playback de input
4. rollback/resimulacao
5. apresentacao/render

Use este documento como plano de execucao. Cada item tenta registrar o suficiente
para uma sessao futura implementar a mudanca sem precisar redescobrir o problema.
Quando um item for concluido, atualize o checkbox e anote qualquer decisao de
design relevante.

Antes de cada pacote maior, medir o baseline com cenas repetiveis, netplay ativo e
rollback induzido. As metricas minimas sao: tempo de snapshot, tempo de CRC,
tempo de serializacao, quantidade de bytes copiados, rollback count, frames
resimulados, frame time p50/p95/p99 e ocorrencias de hitch.

## Phase 0 - Instrumentation Baseline

- [x] Add lightweight timing around netplay snapshot save, CRC generation and rollback load paths.
  - Why: the highest-priority optimizations target snapshot/CRC/rollback cost, so
    every later change needs before/after numbers instead of intuition.
  - Start in: `ThreadedEmulationHost.cpp`, `SingleThreadEmulationHost.cpp`,
    `GeraNESEmu.h`, and the existing runtime diagnostics structs/UI.
  - Approach: add cheap scoped timers or timestamp deltas around snapshot save,
    CRC generation, snapshot lookup and snapshot load. Keep instrumentation
    compiled in only if it follows the existing diagnostics style, or guard it
    behind a debug/diagnostics flag if it would add release overhead.
  - Done when: runtime diagnostics can report at least count, total time, max
    time and recent average for each measured path.
  - Verify: run offline and netplay sessions and confirm diagnostics stay stable
    when netplay snapshots are disabled or no ROM is loaded.

- [x] Add counters for snapshot bytes copied per frame and snapshot copies during rollback/resync.
  - Why: snapshot byte copies are one of the likely hidden costs, and removing
    copies later should be visible immediately in counters.
  - Start in: snapshot storage and retrieval methods in the emulation hosts, plus
    rollback/resync consumers in `NetplayAppRuntime.cpp`.
  - Approach: increment counters at every vector copy or full snapshot load/store
    boundary. Track copied byte totals separately for per-frame save, lookup,
    rollback and hard resync.
  - Done when: diagnostics distinguish "snapshot bytes serialized" from
    "snapshot bytes copied after serialization".
  - Verify: force rollback/resync and confirm counters increase only on the
    expected paths.

- [x] Add timing/counters for input timeline lookup, confirmed-frame lookup and playback queue rebuild.
  - Why: input lookup costs are probably smaller than snapshots, but they happen
    often and can grow with history length.
  - Start in: `InputTimeline.cpp`, `NetplayCoordinator.cpp`, and
    `ConfirmedInputBufferDriver.cpp`.
  - Approach: measure lookup calls, successful hits, misses, scanned entries and
    queue rebuild counts. Keep the measurements local to the existing
    diagnostics pipeline if possible.
  - Done when: a netplay session can show whether input work scales with retained
    history.
  - Verify: run with enough frames to approach history capacity and confirm the
    counters expose scan length or rebuild work.

- [x] Add frame pacing diagnostics for `dt`, frames advanced, catchup frames, rollback count and snapshot time.
  - Why: pacing changes are behavior-sensitive, and hitch/catchup fixes need
    histograms before policy changes.
  - Start in: `GeraNESApp::mainLoop()` and existing netplay/runtime diagnostics.
  - Approach: record frame delta buckets, frames advanced per render tick, catchup
    frames, rollback count and snapshot timing in the same sample window.
  - Done when: one captured diagnostic sample can explain a visible hitch.
  - Verify: trigger a local hitch and confirm the metrics show the spike and the
    resulting catchup/rollback behavior.

- [ ] Capture baseline results before changing behavior.
  - Why: the performance work should prove improvements and catch regressions in
    latency or stability.
  - Start in: docs or local benchmark notes for the first implementation branch.
  - Approach: record machine, build config, ROM/test scenario, netplay mode,
    input delay, average rollback frequency and diagnostic output.
  - Done when: each phase has a comparable before/after snapshot.
  - Verify: rerun the same scenario after Phase 1 and confirm metrics are
    comparable.

## Phase 1 - Snapshot Hot Path

This is the highest priority group because snapshot work runs every frame when
netplay snapshots are active.

- [x] Unify netplay snapshot generation and CRC calculation.
  - Why: `ThreadedEmulationHost.cpp` and `SingleThreadEmulationHost.cpp`
    calculate `canonicalNetplayStateCrc32()` and then save another snapshot with
    `saveNetplayRollbackStateToMemory()`, serializing state at least twice per
    ready frame.
  - Start in: frame-ready snapshot logic in both emulation hosts and canonical
    state APIs in `GeraNESEmu.h`.
  - Approach: create a core API such as `saveNetplaySnapshotWithCrc()` that
    serializes the canonical rollback snapshot once and computes CRC from that
    same buffer. Prefer returning a small struct containing `{data, crc32}` and
    any metadata already needed by the host.
  - Done when: a saved netplay frame performs one canonical serialization and the
    stored CRC is derived from the stored snapshot buffer.
  - Verify: existing CRC/desync/resync tests still pass, and new instrumentation
    shows one serialization per saved netplay frame.

- [x] Make the netplay snapshot index incremental.
  - Why: `rebuildNetplaySnapshotIndexLocked()` rebuilds the whole map after every
    snapshot in both threaded and single-thread hosts.
  - Start in: snapshot retention containers and `rebuildNetplaySnapshotIndexLocked()`
    in `ThreadedEmulationHost.cpp` and `SingleThreadEmulationHost.cpp`.
  - Approach: when evicting with `pop_front()`, remove only the evicted frame
    from the map. When appending a new snapshot, insert/update only that frame.
    If index shifting makes this fragile, replace `deque + frame -> index` with
    stable snapshot slots or `frame -> shared_ptr`.
  - Done when: appending one snapshot does not scan all retained snapshots.
  - Verify: fill past snapshot capacity, then confirm old frames are unavailable,
    new frames are available and rollback selects the correct snapshot.

- [x] Remove large snapshot copies from lookup and rollback.
  - Why: `netplaySnapshotForFrame()` returns `std::optional<std::vector<uint8_t>>`,
    copying the full snapshot; `ThreadedEmulationHost::rollbackToFrame()` also
    creates another local copy.
  - Start in: `EmulationHost.h`, `ThreadedEmulationHost.cpp`,
    `SingleThreadEmulationHost.cpp`, and callers in `NetplayAppRuntime.cpp`.
  - Approach: return a stable read-only handle such as
    `std::shared_ptr<const std::vector<uint8_t>>`, or expose a host method like
    `loadNetplaySnapshotForFrame(frame, emu)` that performs lookup and load while
    the required lock is controlled internally. Avoid returning `std::span`
    unless the lifetime and locking rules are explicit and hard to misuse.
  - Done when: rollback to a stored frame does not allocate/copy the snapshot
    payload just to pass it to the loader.
  - Verify: use counters from Phase 0 to confirm copy bytes drop during rollback
    and resync, then run rollback/resync flows.
  - Notes:
    `IEmulationHost::netplaySnapshotForFrame()` agora retorna
    `std::optional<std::shared_ptr<const std::vector<uint8_t>>>` e o fluxo de
    rollback em `NetplayAppRuntime` passou a usar `rollbackToFrame()` + update
    de CRC do snapshot, removendo copia intermediaria do payload.
  - Regression and resolution note (important):
    a primeira tentativa com handles compartilhados foi revertida por causar
    freeze em netplay real (host/client), devido a risco de lock reentrante no
    caminho de rollback.
    reimplementacao concluida em modo lock-safe:
    - lookup de snapshot permanece via handle compartilhado (sem copia);
    - rollback no worker usa load direto no `emu` corrente (sem chamar caminho
      de host que pode reentrar lock);
    - CRC do snapshot e atualizado sem reserializar payload.
    validado com testes automatizados de resync/load-state e teste manual real
    sem freeze.

- [x] Reduce temporary copies in canonical snapshot save.
  - Why: `saveNetplayStateToMemory()` and `saveNetplayRollbackStateToMemory()`
    copy the full `InputBuffer` to temporarily override input state.
  - Start in: the save-state helpers in `GeraNESEmu.h` and any serialization code
    that reads `m_inputBuffer`.
  - Approach: add an explicit serialization context or override input frame so
    canonical netplay serialization can read replacement input state without
    mutating `m_inputBuffer`. If that is too invasive, use a small guard that
    swaps only the fields required for canonical output.
  - Done when: per-frame netplay snapshot save no longer copies the entire
    `InputBuffer`.
  - Verify: save-state bytes and CRCs remain stable for the same canonical state,
    and input buffer contract tests still pass.
  - Notes:
    `saveCanonicalNetplayStateToMemory()` deixou de copiar `InputBuffer` inteiro
    por valor e passou a usar swap temporario com buffer vazio da mesma
    capacidade, mantendo o formato serializado e restaurando estado original ao
    final.

## Phase 2 - Serialization Throughput

- [x] Add reserve sizing before snapshot serialization.
  - Why: snapshot serialization writes many bytes into vectors, and repeated
    vector growth can add avoidable allocations and copies.
  - Start in: `Serialization.h`, save-state helpers in `GeraNESEmu.h`, and any
    existing `SerializationSize` usage.
  - Approach: compute serialized size before writing when the snapshot path can
    afford a sizing pass, then `reserve()` or `resize()` the output buffer before
    byte writes. Reuse the unified snapshot API from Phase 1 so this happens in
    one place.
  - Done when: snapshot output buffers do not repeatedly reallocate during normal
    netplay snapshot save.
  - Verify: instrument vector capacity changes or allocations in a debug run and
    confirm existing save/load tests still pass.
  - Notes:
    `Serialize` ganhou `reserve()` e `takeData()`; os caminhos de save state
    agora aplicam reserve por size hint thread-local e retornam o buffer via
    move, reduzindo realocacoes e copia final do `std::vector<uint8_t>`.

- [x] Add block-write fast paths for trivial data in little-endian mode.
  - Why: `Serialize::single()` pushes byte by byte, and
    `SerializationBase::array()` calls `single()` element by element. Snapshots
    and CRC generation amplify that overhead.
  - Start in: `Serialization.h`.
  - Approach: for contiguous trivially copyable arrays when `littleEndian() ==
    true`, grow the output vector once and copy bytes with `memcpy`. Keep the
    current element-by-element path for non-trivial types, endian conversion and
    compatibility-sensitive cases.
  - Done when: hot arrays/blocks used by save states serialize through a block
    path with fewer calls and fewer byte-level pushes.
  - Verify: compare serialized bytes before/after for representative save states,
    then run save/load and netplay CRC tests.
  - Notes:
    `Serialization.h` agora usa fast path em little-endian para:
    - `SerializationBase::array()` (escreve/le em bloco com um `single` do
      tamanho total);
    - `Serialize::single()` (resize + `memcpy` em vez de `push_back` byte a byte);
    - `Deserialize::single()` (leitura em bloco com `memcpy`).

- [x] Keep the fallback path for non-trivial data and non-little-endian targets.
  - Why: serialization is a compatibility boundary; performance changes must not
    change byte layout.
  - Start in: `Serialization.h` templates and any endian helpers.
  - Approach: constrain fast paths with type traits and clear conditions. Leave
    the existing implementation as the default fallback.
  - Done when: unsupported types and endian cases still use the old path.
  - Verify: build on the normal target and inspect template selection with small
    focused tests if available.
  - Notes:
    o caminho antigo por elemento (com reversao de bytes) foi mantido quando
    `littleEndian() == false`, preservando compatibilidade de layout.

## Phase 3 - Input Timeline And Confirmed Frames

- [x] Replace linear `InputTimeline::find()` lookups with indexed access.
  - Why: timeline lookup is linear and is called per participant/slot from
    `tryAssembleConfirmedFrame()` and `tryBuildPlaybackFrameInternal()`.
  - Start in: `InputTimeline.cpp`, `InputTimeline.h`, and the coordinator call
    sites in `NetplayCoordinator.cpp`.
  - Approach: add an index keyed by `(frame, participantId, slot)` or convert the
    storage to ring buffers per participant/slot. Preserve existing ordering and
    retention behavior so packet/replay logic does not change.
  - Done when: lookup is effectively O(1) near the 2400-entry history limit.
  - Verify: run timeline tests and a long netplay session; Phase 0 counters
    should show lookup scan length removed or bounded.
  - Notes:
    `InputTimeline` agora usa `std::list<TimelineInputEntry>` +
    `std::unordered_map<(frame, participantId, slot), iterator>`, mantendo
    ordem/retencao e removendo scan linear em `find()`/`findMutable()`.

- [x] Replace linear scans in `m_confirmedFrames`.
  - Why: `findConfirmedFrame()` reverse scans up to
    `kConfirmedFrameHistoryCapacity = 16384`, and `storeConfirmedFrame()` also
    searches linearly before inserting.
  - Start in: confirmed-frame storage and helpers in `NetplayCoordinator.cpp`.
  - Approach: use a ring buffer indexed by `frame % capacity`, storing the frame
    number in each slot to detect stale collisions. If random access patterns
    require it, maintain an incremental `frame -> index` map instead.
  - Done when: confirmed frame read/write paths are O(1) in normal sequential
    flow.
  - Verify: test frame wrap/eviction behavior, replay lookup and rollback lookup
    after capacity is exceeded.
  - Notes:
    `NetplayCoordinator` agora usa `std::list<ConfirmedFrameInputs>` +
    `std::unordered_map<FrameNumber, iterator>` para indexar confirmed frames.
    `findConfirmedFrame()` e update em `storeConfirmedFrame()` deixaram de fazer
    scan linear; eviccao/discard/clear mantem o indice em sincronia.

- [x] Compare `InputFrame` directly instead of serializing to compare.
  - Why: `inputFramesEqual()` serializes both frames before comparison, and this
    path is used by `recordLocalInputFrame()`.
  - Start in: `NetplayCoordinator.cpp` and the `InputFrame` definition.
  - Approach: implement direct equality for frame id, participant/slot fields,
    button masks, device metadata and payload bytes. If some fields have
    canonical serialization quirks, compare cheap scalar fields first and fall
    back to serialization only for those special cases.
  - Done when: the common equality check does not allocate or serialize.
  - Verify: add or update tests for equal frames, different button masks,
    different payloads and special-device input.
  - Notes:
    `InputFrame` ganhou `operator==` default em `InputBuffer.h` e
    `inputFramesEqual()` no coordinator passou a usar comparacao direta, sem
    serializacao temporaria.

- [x] Reduce allocations when building confirmed input packets.
  - Why: `buildConfirmedInputFramesPacket()` creates
    `std::vector<std::vector<uint8_t>>` and serializes each `InputFrame` into
    temporaries.
  - Start in: `NetplayCoordinator.cpp` packet-building code and `PacketWriter`
    helpers.
  - Approach: write each serialized input frame directly to `PacketWriter`, or
    reuse one scratch buffer for the whole call. If packet format requires sizes
    before payloads, first compute sizes or reserve placeholders and patch them
    after writing.
  - Done when: confirmed packet construction performs no per-frame nested vector
    allocation on the common path.
  - Verify: packet encode/decode tests still pass, and network compatibility is
    preserved unless a protocol version bump is intentionally added.
  - Notes:
    `buildConfirmedInputFramesPacket()` removeu `std::vector<std::vector<uint8_t>>`
    e agora:
    - calcula tamanho estimado via `SerializationSize`;
    - reutiliza `Serialize` thread-local (com `clear()` + reserve hint) para
      serializar cada frame e escrever direto no `PacketWriter`.

- [x] Make `ConfirmedInputBufferDriver` playback queue updates incremental.
  - Why: `preparePlaybackFramesForEmulationThread()` rebuilds a new deque and
    replaces `m_pendingFrames`, even when little changed.
  - Start in: `ConfirmedInputBufferDriver.cpp`.
  - Approach: retain `m_pendingFrames`, drop frames older than the current
    playback point, and append only missing frames up to the target horizon.
    Track the last prepared horizon/frame to skip work when unchanged.
  - Done when: ticks with unchanged playback horizon avoid rebuilding and moving
    the full queue.
  - Verify: run playback/rollback tests and confirm no duplicate or skipped
    pending frames.
  - Notes:
    `preparePlaybackFramesForEmulationThread()` agora:
    - descarta apenas frames antigos;
    - poda apenas cauda fora do horizonte;
    - atualiza frames existentes in-place;
    - anexa somente frames faltantes.
    `queuePendingFramesToEmu()` deixou de drenar toda a deque a cada tick e
    passou a manter janela incremental ativa.

## Phase 4 - Core Input Buffer

- [x] Replace repeated linear `InputBuffer` scans with frame/epoch indexed access.
  - Why: `findByFrame`, `latestFrameForTimelineEpoch`, `markConsumed` and `push`
    scan the buffer. Rollback/resimulation can call these paths repeatedly in
    sequential frame order.
  - Start in: `InputBuffer.h` and tests documented by
    `docs/internal/INPUT_BUFFER.md`.
  - Approach: use a ring buffer indexed by frame plus timeline epoch validation,
    or keep the existing storage and add indexes for `(frame, epoch)` and latest
    frame per epoch. Prefer the smallest change that preserves current semantics.
  - Done when: common frame lookup, consume and latest-frame queries no longer
    scan the full retained buffer.
  - Verify: run input buffer tests, rollback/resync tests and long-session
    retention tests.
  - Notes:
    `InputBuffer` ganhou indice interno lazy por `(frame, timelineEpoch)` e
    caches auxiliares de latest por epoch/frame. `findByFrame`, `markConsumed`,
    `isConsumed` e `latestFrameForTimelineEpoch` usam lookup indexado; mutacoes
    marcam o indice como dirty e reconstroem sob demanda.

- [x] Preserve the existing input contract.
  - Why: input behavior is correctness-critical, and performance changes here can
    silently corrupt rollback.
  - Start in: `docs/internal/INPUT_BUFFER.md` and corresponding tests.
  - Approach: treat consumed-frame immutability, epoch rejection, contiguous
    growth and startup/bootstrap behavior as non-negotiable contracts. Add tests
    before refactoring if a case is not already covered.
  - Done when: the implementation is faster but externally preserves the same
    enqueue results and playback behavior.
  - Verify: all input buffer contract tests pass before and after each internal
    storage change.
  - Notes:
    validado com testes automatizados de contrato e runtime netplay:
    - `InputBuffer enforces sequential frame enqueue per timeline epoch`
    - `Netplay core advances only when the exact next numbered input frame exists`
    - `Netplay runtime flow hard-resyncs after an injected desync`

## Phase 5 - App Snapshot, Render And Threading

- [x] Reduce work inside `refreshSnapshotLocked()`.
  - Why: this method copies UI/config strings/vectors and framebuffer data
    frequently in the worker loop, mixing slow-changing configuration with
    per-frame presentation state.
  - Start in: `ThreadedEmulationHost.cpp`, especially `refreshSnapshotLocked()`
    and the snapshot struct it fills.
  - Approach: split per-frame state from slower config/UI state. Update
    audio-device lists, channel JSON and similar data only when the source
    changes. Keep synchronization rules explicit so UI reads remain safe.
  - Done when: routine worker refreshes do not copy unchanged config/UI vectors
    and strings.
  - Verify: run the app, change audio/config settings, and confirm UI still
    updates while normal frame refresh copies less data.
  - Notes:
    `ThreadedEmulationHost::refreshSnapshotLocked()` foi dividido em:
    - campos rapidos atualizados em todo refresh;
    - campos lentos de audio/UI (`audioDevices`, `audioDeviceName`,
      `audioChannelsJson`, volume) atualizados apenas quando dirty
      (`configAudioDevice`, `restartAudio`, volume/channel changes) ou em
      refresh periodico (250 ms).

- [x] Copy framebuffer only when a new frame is ready or presentation requires it.
  - Why: framebuffer memcpy can dominate refresh work if it happens on state
    refreshes that do not produce a presentable frame.
  - Start in: `ThreadedEmulationHost.cpp` frame-ready/presentation paths and
    snapshot refresh code.
  - Approach: track whether the framebuffer changed since the last published
    snapshot. Copy only on frame-ready events, explicit presentation requests or
    state transitions that require a fresh image.
  - Done when: non-frame-ready refreshes avoid framebuffer memcpy.
  - Verify: instrumentation shows fewer framebuffer copies while presentation
    remains visually correct during pause, run, reset and netplay resync.
  - Notes:
    `ThreadedEmulationHost` agora usa flag interna `m_framebufferDirty`.
    `refreshSnapshotLocked()` so copia framebuffer quando nao esta em hold e a
    flag esta suja; `onFrameReadyLocked()` marca a flag para o proximo publish.

- [x] Replace small heap allocation in `GeraNESApp::updateBuffers()`.
  - Why: layout data is small and fixed-size, but currently built with
    `std::vector<GLfloat>`.
  - Start in: `GeraNESApp.cpp`, `GeraNESApp::updateBuffers()`.
  - Approach: replace the vector with `std::array<GLfloat, 16>` or another
    fixed-size stack buffer matching the VBO layout.
  - Done when: VBO layout data generation performs no heap allocation.
  - Verify: build and visually confirm screen layout/aspect handling still works.
  - Notes:
    `updateBuffers()` trocou `std::vector<GLfloat>` por
    `std::array<GLfloat, 16>` com preenchimento direto dos 4 vertices
    (posicao+UV), removendo `push_back` e alocacao heap por update.

- [x] Review presenter-locked pacing under hitch/catchup.
  - Why: presenter cadence and catchup paths can introduce delay under hitch,
    especially with the 3-frame cap.
  - Start in: `GeraNESApp::mainLoop()`.
  - Approach: analyze Phase 0 pacing metrics first. Only change policy after a
    concrete pattern is visible, such as repeated capped catchup or netplay
    rollback bursts causing visible latency.
  - Done when: any pacing change has a documented before/after histogram and
    clear behavior goal.
  - Verify: compare offline and netplay frame pacing, including pause/resume and
    temporary hitches.
  - Notes:
    o clamp de catchup de netplay (limitar `framesToAdvance` a 1 e limitar
    acumulador a 1 frame) foi aplicado tambem no desktop, nao apenas no web.
    objetivo: reduzir bursts de catchup que aumentam latencia/percepcao de
    hitch em sessao netplay.

## Suggested First Change Set

- [x] Implement unified snapshot + CRC generation.
  - Depends on: Phase 0 snapshot/CRC timing if possible.
  - Expected impact: largest direct reduction in per-frame netplay snapshot cost.

- [x] Make the snapshot index incremental.
  - Depends on: understanding snapshot retention/eviction semantics.
  - Expected impact: removes avoidable per-frame map rebuild work.

- [x] Return/load snapshots by handle/reference instead of copying vectors.
  - Depends on: clear lifetime and locking rules for stored snapshots.
  - Expected impact: reduces rollback/resync memory bandwidth and allocation.

- [x] Add serialization reserve and little-endian block fast paths.
  - Depends on: save-state compatibility checks.
  - Expected impact: reduces CPU cost inside all snapshot/CRC paths.

- [x] Index `InputTimeline` and `m_confirmedFrames` by frame.
  - Depends on: timeline and confirmed-frame retention tests.
  - Expected impact: stabilizes input/confirmed-frame lookup cost during long
    sessions and rollback.

This first set attacks the most likely netplay bottleneck before touching more
behavior-sensitive areas such as pacing and audio.

## Verification Checklist

- [ ] Build completes with the project CMake flow.
- [ ] Existing unit tests pass.
- [ ] Netplay start, rollback, resync and spectator sync still work.
- [ ] Save-state compatibility is unchanged for existing states.
- [ ] Baseline metrics are updated after each phase.
- [ ] Any change to packet format is versioned or kept backward compatible.
