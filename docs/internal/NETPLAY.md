```md
# AGENTS.md

Important: After modifications, run cmake and WAIT to complete before run tests

## How to use this file

This file has two jobs:

- describe the product and architecture goals for netplay
- give coding agents a practical playbook for working safely in this repo
- record the current implementation status so future Codex sessions start from the real baseline

If there is tension between an old implementation detail and the current emulator/input-buffer model, favor the current model and update tests accordingly instead of preserving obsolete behavior.

---

## Current project status

The netplay system is no longer at the "starting implementation" stage. Use the sections below as the current baseline when answering questions or planning changes.

### Implemented so far

The following netplay features are already present in the codebase:

- canonical save-state export and canonical CRC generation in the emulator core
- per-frame snapshot storage for rollback
- local rollback and resimulation plumbing in the desktop runtime
- ENet-based host/join/disconnect transport
- basic room/lobby flow with participant list
- host-side controller assignment
- ready/start/pause/resume/end session flow
- ROM selection propagation and ROM compatibility validation
- per-frame input timeline for local and remote participants
- configurable input delay in the running session flow
- last-known-input prediction for missing remote frames
- rollback scheduling on prediction mismatch
- hard resync with authoritative save-state transfer
- periodic CRC exchange and confirmed desync escalation into hard resync
- ping/jitter reporting in the room UI
- reconnect reservations with timeout and host removal
- reconnect by persistent reconnect token, not display name
- auto-resume when all required participants are ready again
- late spectator join with private spectator sync instead of room-wide resync
- observer-to-player slot request flow with host approve/deny
- diagnostics UI for rollback, prediction, confirmed conflicts, resyncs, and per-peer mismatch stats

### Recently completed corrective hardening (current baseline)

These fixes are now part of the active netplay baseline and should be treated as implemented behavior:

- explicit recovery input state machine in room/runtime:
  - `RecoveryInputMode::Normal`
  - `RecoveryInputMode::ResyncLocked`
  - `RecoveryInputMode::PostResyncStabilizing`
- resync input locking semantics:
  - local gameplay input is blocked from timeline submission while locked
  - incoming gameplay input/ack/confirmed-frame updates are ignored for simulation application while locked
- post-resync stabilization gate:
  - unlock requires stabilization frame window + confirmed checkpoint + post-recovery CRC submission
  - stabilization timeout escalates to a single controlled host retry (not uncontrolled resync loops)
- stale-epoch discipline during recovery:
  - stale `InputFrame`, `InputAck`, `FrameStatus`, and `CrcReport` packets remain gated during lock/stabilization
- host-side resync cascade prevention:
  - desync monitor escalation is deferred during explicit stabilization windows
  - redundant actionable resync scheduling is coalesced while recovery is in progress
- stronger recovery diagnostics:
  - `recoveryInputMode`
  - `inputsDroppedDuringRecovery`
  - `stabilizationFramesRemaining`
  - `stabilizationCrcPassCount`
  - `stabilizationRetryCount`
  - explicit transition logs (`lock start`, `stabilization start`, `unlock`, `controlled retry`)
- host force-resync presentation continuity fix:
  - host authoritative local reload now uses presentation hold before clean-boot state load
  - this prevents host-only black-frame flicker during force resync

### Current automated evidence for corrective hardening

Key tests now covering these contracts include:

- `Netplay runtime drops host input spam during resync and avoids resync cascade`
- `Netplay runtime drops host/client input spam during resync and converges`
- `Netplay coordinator keeps stale-epoch packets gated during recovery lock and stabilization`
- `Netplay coordinator schedules a single controlled resync retry when stabilization fails`
- `Netplay runtime manual resync performs immediate post-resync CRC verification`
- `Netplay runtime ignores stale previous-epoch inputs after authoritative recovery`
- `Netplay presentation hold keeps last frame visible across authoritative state load`

### Current UI/runtime support

- `Tools -> Netplay` window exists in the desktop app
- host can:
  - create room
  - assign controllers
  - change role between player and observer
  - kick peers
  - remove reconnect reservations
  - force hard resync
  - approve or deny observer controller requests
- observers can request player slots from the same window
- the room UI shows:
  - session state
  - ROM status
  - input delay
  - ping/jitter
  - reconnect reservation state
  - rollback/prediction/desync counters

### Current validation guidance

When changing netplay now, do not treat lobby, rollback, resync, or CRC monitoring as speculative design work. They already exist and should be debugged, extended, or tightened in place.

Prefer this validation order:

1. `StateReplayTest` for deterministic/core issues
2. `NetplayTest` for protocol/runtime/session regressions
3. desktop `Tools -> Netplay` flow for UI/integration confirmation

### Not done yet

These parts are still incomplete or intentionally deferred:

- production-grade reconnect authentication beyond local reconnect token persistence
- full late-input policy polish under harsher real network conditions
- spectator-to-player join policy beyond host manual approval
- chat UX polish
- host migration
- relay/NAT traversal
- compression/delta optimization for resync payloads
- broad real-world stress testing and playability validation

---

## How to explore the repo

Start from these folders:

- `src/GeraNES`
  - emulator core
  - save states
  - deterministic simulation
  - input buffer
- `src/GeraNESApp`
  - desktop app runtime
  - UI
  - emulation host
  - app-side netplay integration
- `src/GeraNESNetplay`
  - transport/session/runtime/protocol/diagnostics code for netplay
- `src`
  - top-level test harnesses such as `StateReplayTest.h` and `NetplayTest.h`

When investigating a bug, prefer this order:

1. replay determinism in `StateReplayTest`
2. headless or app-style netplay reproduction in `NetplayTest`
3. desktop runtime integration in `GeraNESApp` / `EmulationHost`
4. manual UI behavior last

Useful files to inspect first:

- `src/GeraNES/GeraNESEmu.h`
- `src/GeraNES/InputBuffer.h`
- `src/GeraNESApp/EmulationHost.h`
- `src/GeraNESApp/GeraNESApp.h`
- `src/GeraNESNetplay/NetplayCoordinator.h`
- `src/GeraNESNetplay/NetplayCoordinator.cpp`
- `src/StateReplayTest.h`
- `src/NetplayTest.h`

---

## Build and test commands

Run builds from the repo root.

Primary build command:

```powershell
cmake --build build --target GeraNES -j 4
```

Important:

- always wait for the build to finish before running tests
- if `GeraNES.exe` is still open from a previous run, close it before rebuilding

Useful local commands:

```powershell
Get-Process GeraNES -ErrorAction SilentlyContinue
Get-Process GeraNES -ErrorAction SilentlyContinue | Stop-Process -Force
```

Recommended validation flow after netplay/runtime changes:

1. rebuild
2. run state replay
3. run netplay test
4. only then do manual host/client testing

Replay test example:

```powershell
build\GeraNES.exe --test-state-replay "C:\path\to\rom.zip" --frames 300 --replay-horizon 8 --from-frame 180 --report build\state_replay_report.json
```

Netplay test example:

```powershell
build\GeraNES.exe --test-netplay "C:\path\to\rom.zip" --app-flow --frames 300 --input-delay 10 --robust --report build\netplay_report.json
```

Warm-start netplay bootstrap example:

```powershell
build\GeraNES.exe --test-netplay "C:\path\to\rom.zip" --app-flow --frames 300 --input-delay 10 --pre-session-warmup 120 --report build\netplay_warmup_report.json
```

If a report file does not appear where expected, inspect the latest JSONs in `build` and do not assume the named file was written.

---

## Safe editing rules

- Prefer incremental, testable changes.
- Do not preserve dead netplay paths just because they existed before.
- Keep transport concerns out of emulator-core logic.
- Keep deterministic-core fixes separate from UI/runtime conveniences when possible.
- When refactoring, update the reproducer tests in the same change if behavior changes.
- If a bug can be reproduced in a test, fix the test path first and only then trust manual testing.
- Do not silently widen canonical CRC/save-state scope with transient runtime state.
- Treat frame numbering as a contract. Be explicit about whether code means:
  - current emulated frame
  - next frame to simulate
  - last confirmed frame
  - last queued frame

For the current codebase, the most important runtime rule is:

- the emulator advances only when it has the exact `InputFrame` needed for the next frame
- if resync/bootstrap changes the current frame, every producer/driver that feeds the `InputBuffer` must be reanchored to that loaded frame

---

## Agent workflow

When working on netplay or determinism:

1. reproduce with `StateReplayTest` or `NetplayTest`
2. make the smallest fix that restores the contract
3. rebuild
4. rerun the relevant automated tests
5. only then ask for or rely on manual verification

When modifying tests:

- prefer replacing obsolete harness behavior over layering more compatibility code on top
- keep tests aligned with the current emulator contract, especially around `InputBuffer` and frame numbering
- make reports explain why a scenario failed, not only that it failed

When modifying runtime netplay:

- prefer putting dedicated netplay logic in `src/GeraNESNetplay`
- keep `GeraNESApp` focused on orchestration/UI
- keep `EmulationHost` focused on running the emulator safely on the emulation thread

## Objective

Implement a robust and pleasant-to-use netplay system for the NES emulator based on:

- host-authoritative session management
- input-only networking
- deterministic frame simulation
- configurable input delay
- remote input prediction using "last known input"
- rollback and resimulation on prediction mismatch
- per-frame save states for rollback
- periodic desync detection using canonical save-state CRC
- hard resynchronization by transferring the host save state
- a lobby UX with roles, ROM validation, controller assignment, and chat

This document is intended to guide coding agents working on the project. Favor correctness, determinism, debuggability, and incremental delivery over premature optimization.

---

## Core design principles

1. **Determinism first**
   - Netplay is impossible to trust if emulation is not deterministic.
   - Any source of non-determinism must be identified and removed or isolated.

2. **Authoritative host**
   - The host is the source of truth for room state, player assignments, official input timeline, desync decisions, and hard resync snapshots.

3. **Input-only transport**
   - Do not stream video/audio.
   - Every participant runs the emulation locally.
   - Even "spectators" are synchronized participants without a controller assignment.

4. **Fast local feel**
   - Use configurable input delay plus remote input prediction.
   - Avoid pure blocking lockstep that freezes too often under jitter.

5. **Rollback as normal operation**
   - Prediction mistakes are expected and should be handled routinely.
   - Rollback/resimulation must be cheap and reliable.

6. **Hard resync as recovery path**
   - If clients truly diverge, the host sends a canonical save state and pauses progression until the client acknowledges synchronization.

7. **Debuggability is a feature**
   - Build internal tooling for frame traces, input history, snapshot CRCs, desync reports, and rollback statistics.

---

## High-level architecture

The system should be split into clearly separated modules:

- `NetTransport`
  - ENet-based transport
  - channels, packet serialization, connection/session state

- `NetSession`
  - lobby state
  - participants
  - roles
  - ROM validation
  - controller assignment
  - ready state
  - start/pause/resync flow

- `NetplayRuntime`
  - frame timeline
  - input collection
  - prediction
  - rollback
  - resimulation
  - frame confirmation

- `SnapshotSystem`
  - per-frame save states
  - ring buffer management
  - canonical serialization
  - CRC/hash generation

- `DesyncMonitor`
  - periodic CRC exchange/validation
  - mismatch detection
  - desync severity classification
  - escalation to hard resync

- `UI/Lobby`
  - create/join room
  - participant list
  - ROM compatibility state
  - controller assignment
  - ready state
  - chat
  - start session
  - resync/connection status

- `Diagnostics`
  - logs
  - frame timeline dump
  - per-player input history
  - rollback count
  - latency/jitter stats
  - desync reports

Keep these areas decoupled. Do not bury protocol rules directly inside rendering or emulation code.

Create a folder specific for netplay stuff.

---

## Recommended transport choice

Use **ENet** for transport.

Reasons:

- UDP-based
- supports reliable and unreliable packets
- supports channels
- good fit for low-latency netplay
- simpler and more appropriate than raw TCP for this use case

Suggested channel layout:

- **Channel 0**: reliable control/lobby/session messages
  - room state
  - chat
  - ROM validation
  - ready state
  - controller assignment
  - start/pause/resync commands
  - save-state transfer metadata

- **Channel 1**: gameplay input stream
  - player input packets by frame
  - input acknowledgements if needed

- **Channel 2**: diagnostics/health
  - ping
  - jitter
  - frame counters
  - CRC reports
  - warnings/telemetry

Do not overcomplicate the transport layer. Higher-level correctness belongs in the session/runtime layers.

---

## Session model

### Roles

Each participant is one of:

- `Host`
- `Player`
- `Observer`

Notes:

- Observers still need the exact same ROM and run the emulator locally.
- An observer is simply a synchronized participant with no active controller assignment.

### Controller assignments

Support explicit assignment of:

All periferals can be assigned. The client will bind the keys as he likes.

### Room lifecycle

The room should move through these states:

1. `Lobby`
2. `ValidatingRom`
3. `ReadyCheck`
4. `Starting`
5. `Running`
6. `Resyncing`
7. `Paused`
8. `Ended`

The state machine must be explicit and testable.

---

## Lobby UX requirements

The user experience should support:

- host creates room
- clients join room
- host selects ROM
- all participants load and validate same ROM
- host assigns controllers
- participants can be marked as players or observers
- all participants mark themselves ready
- host starts the session

Recommended participant row fields:

- display name
- role
- controller assignment
- ROM loaded / not loaded
- ROM compatible / incompatible
- ready / not ready
- ping
- connection quality

Recommended room-level fields:

- selected game name
- ROM CRC/hash
- mapper
- file size
- session status
- whether the room is locked for start

Recommended host actions:

- choose ROM
- assign controllers
- switch role player/observer
- kick participant
- start session
- pause session
- force resync
- end session

---

## ROM validation requirements

All participants must have the exact compatible game content before start.

Validation should include at minimum:

- ROM CRC32 or stronger hash
- mapper ID
- PRG/CHR sizes
- relevant cartridge/peripheral metadata if applicable

Display both machine-readable and human-readable status.

Examples:

- `ROM OK`
- `ROM not loaded`
- `ROM incompatible with host`
- `Mapper mismatch`
- `Patch mismatch`

If patches are supported, validation must be based on the **effective content actually being executed**, not only the original file.

---

## Runtime model

### Frame timeline

All gameplay communication is organized by simulation frame.

Each input event must include:

- frame number
- participant/player ID
- button bitmask
- optional sequence number

### Input delay

Use configurable input delay.

Interpretation:

- a locally captured input is applied to a future simulation frame
- this creates time for remote inputs to arrive before they are needed

Goal:

- higher delay = fewer rollbacks, more latency
- lower delay = more responsive, more rollbacks

The implementation should support room-configurable defaults and ideally future adaptive tuning.

### Remote prediction

For missing remote input on a needed frame:

- predict by reusing the participant's **last known input**

Do not invent clever heuristics initially.

Prediction metadata should be recorded so the runtime knows which frames contain predicted inputs.

### Rollback

If a later-arriving actual input differs from the predicted input used for a past frame:

1. restore snapshot for the earliest affected frame
2. apply the corrected official inputs
3. resimulate up to the current frame
4. replace current runtime state with the recomputed state

Rollback window must be bounded.

If an input arrives too late to fit within the rollback window, escalate to hard resync or other defined recovery logic.

---

## Snapshot system requirements

### Save state every frame

For the initial implementation:

- create a save state every frame
- store snapshots in a ring buffer
- keep enough history to support rollback window + safety margin

Why:

- easiest and most reliable rollback implementation
- excellent for debugging
- reasonable for NES-scale state sizes

### Snapshot content rules

The serialized state used for rollback and CRC must be **canonical**.

Must not include:

- real-time clocks
- raw pointer addresses
- uninitialized memory
- irrelevant caches unless deterministic and necessary
- platform-dependent layout artifacts

Must include all state that affects emulation outcome, such as:

- CPU state
- PPU state
- APU state
- RAM/VRAM/OAM/palette
- mapper state
- controller/peripheral state
- timing counters
- any open-bus state if it affects determinism
- any IRQ/NMI pending state
- any cartridge RAM/state

### Save-state format rules

The format must be:

- versioned
- deterministic
- portable
- independent of compiler struct padding
- suitable for network transfer

Recommended structure:

- header
- save-state version
- emulator core version
- frame number
- serialized blocks in fixed order

---

## Desync detection strategy

### Canonical CRC/hash

Generate a CRC/hash from the canonical save state.

Use it to detect whether peers at the same confirmed frame have the same emulation state.

### Frequency

Do **not** necessarily send CRC every frame over the network.

Recommended:

- snapshot every frame locally
- exchange CRC periodically, such as every N frames
- choose N conservatively at first, e.g. 30 or 60 frames

### Desync severity classification

Define explicit categories:

- `NoIssue`
- `PredictionMismatchOnly`
- `RollbackCorrected`
- `ConfirmedDesync`
- `HardResyncRequired`

A hard resync should be triggered when one or more of the following happens:

- CRC mismatch on a confirmed frame
- mismatch persists after rollback correction
- client falls too far behind
- late input exceeds rollback window
- repeated CRC mismatch over several checkpoints
- host explicitly forces resync

---

## Hard resync strategy

When desync is severe:

1. host pauses progression
2. host creates authoritative canonical save state
3. host sends resync header:
   - target frame
   - save-state size
   - save-state CRC/hash
   - session/resync ID
4. host transfers the save-state payload
5. client receives and validates payload
6. client discards incompatible local rollback history
7. client loads the new state
8. client resets input/snapshot tracking appropriately
9. client sends ACK with loaded frame and CRC/hash
10. host confirms and resumes the session

Session state must clearly enter `Resyncing`.

During resync:

- simulation must not continue normally
- audio should be muted or reset
- UI should indicate resynchronization in progress
- timeouts/failure handling must exist

---

## Diagnostics and observability requirements

Agents should implement diagnostics early, not only after bugs appear.

At minimum provide:

- current room/session state
- current simulation frame
- last confirmed frame
- rollback count
- max rollback distance
- prediction hit/miss counts
- per-peer ping/jitter
- last received input frame per peer
- CRC history
- resync count
- disconnect reason log

Useful debug dumps:

- per-frame input timeline
- which player input was predicted on which frame
- snapshot CRC table
- desync report with earliest mismatching frame
- save-state compare tools if possible

Strongly consider a debug overlay or log view.

---

## Implementation roadmap

---

## Phase 0 - Preparation and constraints

### Goals

- define scope
- confirm architectural boundaries
- prepare the codebase for deterministic netplay features

### Tasks

- identify where emulator state is fully serialized today
- audit current save-state system
- audit all stateful subsystems included in emulation
- identify places where wall-clock time or host timing leaks into core behavior
- define coding conventions for netplay modules
- define packet serialization format
- define internal frame numbering conventions

### Deliverables

- technical design notes
- list of determinism risks
- list of required emulator state components
- initial protocol sketch

---

## Phase 1 - Determinism audit

### Goals

- ensure the emulator behaves identically on multiple machines when given identical inputs

### Tasks

- test repeated runs with identical input playback
- verify save/load/save cycles preserve exact state
- verify loaded state produces same future CRC as original
- remove pointer/layout dependence from serialization
- ensure controller polling is frame-accurate and deterministic
- ensure audio generation does not affect emulation timing
- verify mapper/peripheral state is fully serialized
- build a local replay runner that can feed recorded inputs and compare hashes

### Deliverables

- deterministic replay test harness
- canonical save-state serializer
- determinism test suite

### Exit criteria

- identical replay inputs reproduce identical CRCs across repeated runs

---

## Phase 2 - Snapshot and rollback foundation

### Goals

- make per-frame snapshotting and rollback fast and trustworthy

### Tasks

- implement per-frame save-state ring buffer
- store frame number per snapshot
- implement restore-by-frame
- implement resimulation from restored frame to target frame
- track predicted vs confirmed inputs per frame
- expose rollback stats in diagnostics

### Deliverables

- `SnapshotSystem`
- rollback/resimulation primitive
- test that restoring frame F and simulating forward reproduces original CRC at frame G

### Exit criteria

- local rollback without networking works reliably

---

## Phase 3 - Transport layer with ENet

### Goals

- establish stable peer connectivity and message routing

### Tasks

- integrate ENet
- define channel usage
- implement packet encode/decode
- implement connect/disconnect lifecycle
- implement host/client roles at network level
- add ping/latency measurement
- add sequence/session IDs where needed

### Deliverables

- `NetTransport`
- reliable packet handling for control messages
- gameplay input channel
- diagnostics channel

### Exit criteria

- host and client can connect, exchange control messages, and remain stable

---

## Phase 4 - Lobby and room UX

### Goals

- create the full pre-game room workflow

### Tasks

- host creates room
- clients join room
- implement participant list
- implement chat
- implement role display: host/player/observer
- implement controller assignment UI
- implement ready status
- implement ROM selection by host
- implement ROM validation and compatibility display
- prevent start if validation/ready rules fail

### Deliverables

- room UI
- room state synchronization protocol
- host admin actions

### Exit criteria

- a room can be created, populated, configured, and started cleanly

---

## Phase 5 - Start session and confirmed lockstep baseline

### Goals

- first working online play, even before prediction/rollback polish

### Tasks

- define official start frame
- synchronize all peers to common start state
- begin input-by-frame exchange
- implement a conservative baseline that can run deterministically online
- confirm that all peers remain aligned for simple stable tests

### Deliverables

- minimal working netplay session
- frame-based input protocol
- confirmed synchronized game start

### Exit criteria

- two peers can play a simple test session without diverging under good network conditions

---

## Phase 6 - Input delay and remote input prediction

### Goals

- improve feel and reduce visible stalling

### Tasks

- implement configurable input delay
- apply local input to future target frames
- implement remote prediction using last-known input
- annotate predicted inputs in frame history
- visualize prediction hits/misses in diagnostics

### Deliverables

- smooth non-blocking runtime under moderate latency/jitter
- input delay setting in room or config UI

### Exit criteria

- play remains smooth when remote packets arrive slightly late

---

## Phase 7 - Rollback and resimulation in live network sessions

### Goals

- correct prediction mistakes transparently

### Tasks

- detect mismatch between predicted and actual remote input
- compute earliest corrected frame
- rollback to snapshot
- resimulate to present
- ensure current state, render state, and audio state remain coherent
- cap rollback window
- define fallback when correction is too late

### Deliverables

- live rollback system
- rollback statistics
- stable state after correction

### Exit criteria

- visible gameplay remains coherent after packet jitter and prediction errors

---

## Phase 8 - CRC monitoring and desync detection

### Goals

- detect true state divergence early and reliably

### Tasks

- compute canonical save-state CRC periodically
- exchange CRC reports between peers and host
- compare CRC only at agreed frames/checkpoints
- classify mismatch severity
- log detailed desync reports
- distinguish ordinary rollback corrections from true desyncs

### Deliverables

- `DesyncMonitor`
- periodic CRC protocol
- desync classification and logging

### Exit criteria

- host can reliably identify confirmed desync situations

---

## Phase 9 - Hard resync with authoritative save-state transfer

### Goals

- recover from real divergence instead of just disconnecting

### Tasks

- define resync session/message types
- pause session on host authority
- serialize authoritative current save state
- transfer state to client
- validate payload integrity
- load state on client
- reset local rollback history
- clear or rebuild local frame windows
- ACK synchronization back to host
- resume session cleanly

### Deliverables

- working hard resync flow
- UI indication for resync progress
- timeout/retry/error handling

### Exit criteria

- client can recover from forced desync and continue play

---

## Phase 10 - UX polish and resilience

### Goals

- make the system pleasant and understandable for real users

### Tasks

- improve room status messages
- show why start is blocked
- show ROM mismatch reasons clearly
- show network quality indicators
- add reconnect handling policy
- define behavior for host disconnect
- define behavior for player disconnect
- define pause/resume rules
- add host kick/removal handling
- make chat usable but not intrusive
- expose rollback/input delay stats in optional debug UI only

### Deliverables

- understandable UX for normal users
- robust session recovery behavior
- clear admin controls

### Exit criteria

- non-developer users can create/join/play without guessing what is wrong

---

## Phase 11 - Stress testing and validation

### Goals

- validate under imperfect network conditions

### Tasks

- simulate latency, jitter, packet loss, and burst loss
- test with repeated taps, quick direction changes, and menu-heavy games
- test long sessions
- test disconnect/reconnect
- test frequent rollback cases
- test forced desync injection
- test large save-state transfer and resume
- record metrics on rollback frequency and perceived playability

### Deliverables

- test matrix
- benchmark results
- list of remaining edge cases

### Exit criteria

- system is stable and understandable under realistic consumer network conditions

---

## Phase 12 - Future enhancements

These are not required for first release but should be designed for:

- adaptive input delay
- spectator join-in-progress
- rollback optimization using delta snapshots
- save-state compression for resync
- host migration
- relay server / NAT traversal solutions
- more peripherals
- four-player support
- patch-aware session validation
- replay export from netplay sessions
- richer desync diff tooling

---

## Protocol guidance

Define message types explicitly. Suggested families:

### Control / lobby
- `CreateRoom`
- `JoinRoom`
- `ParticipantJoined`
- `ParticipantLeft`
- `ChatMessage`
- `SetRole`
- `AssignController`
- `SetReady`
- `SelectRom`
- `RomValidationResult`
- `StartSession`
- `PauseSession`
- `ResumeSession`
- `EndSession`

### Runtime
- `InputFrame`
- `InputAck` if needed
- `FrameStatus`
- `PredictionCorrectionNotice` if useful
- `PeerHealth`
- `CrcReport`

### Resync
- `ResyncBegin`
- `ResyncChunk`
- `ResyncComplete`
- `ResyncAck`
- `ResyncAbort`

Keep payloads versioned where appropriate.

---

## Rules for coding agents

1. Never mix transport concerns directly into emulator core logic.
2. Never assume save-state binary layout is portable unless explicitly defined.
3. Never include nondeterministic or host-only ephemeral data in canonical CRC generation.
4. Prefer explicit state machines over boolean flag soup.
5. Add diagnostics whenever adding a new rollback, prediction, or resync path.
6. Test every new feature first in local deterministic replay if possible.
7. Do not optimize away correctness checks early.
8. Keep host-authoritative decisions centralized.
9. Write code so forced desyncs can be injected intentionally for testing.
10. Preserve clear separation between:
   - session state
   - transport state
   - runtime simulation state
   - UI state

---

## Immediate next steps

If continuing implementation now, prioritize this order:

1. tighten automated coverage around the current replay, rollback, CRC, reconnect, and resync contracts
2. improve late-input handling and harsher-network behavior in `NetplayTest`
3. polish room/chat UX and clearer start-blocked or mismatch messaging
4. harden reconnect/auth flows beyond local reconnect token persistence
5. expand stress testing, metrics, and real-session validation
6. only then pursue larger follow-on features such as host migration, relay/NAT traversal, or resync compression

---

## Definition of success

The netplay system is considered successful when:

- two or more participants can join a room and validate identical ROM content
- host can assign controllers and start the match
- gameplay remains smooth under moderate latency due to input delay + prediction
- prediction errors are corrected by rollback without breaking state
- true desyncs are detected by periodic CRC
- the host can recover a desynced client by sending an authoritative save state
- users understand what is happening from the UI without needing debugger logs

---
```

---


