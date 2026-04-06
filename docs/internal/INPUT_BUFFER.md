# Input Buffer Contract

This document defines the authoritative frame-input behavior used by emulator core, offline runtime, and netplay runtime.

## Core rule

For each `(frame, timelineEpoch)` there is one authoritative input entry.

- while frame is pending: input may be inserted or updated
- once frame is consumed by emulation: input is immutable

The emulator only advances when the exact required playback frame input exists.

## Frame model

- `frameCount()` is the current playback frame expected by emulation.
- Emulation step requires input for `frameCount()` at current `inputTimelineEpoch()`.
- On successful frame execution:
  - input for that frame is marked consumed
  - frame becomes immutable
  - `frameCount()` advances

## Timeline epoch model

- Inputs are partitioned by `timelineEpoch`.
- Epoch mismatch enqueue is rejected.
- Reanchor/resync paths must set epoch explicitly and re-seed input timeline from the loaded frame.
- Stale epoch inputs must not be applied.

## Enqueue semantics

`InputBuffer::EnqueueResult`:

- `Inserted`
- `UpdatedPending`
- `RejectedConsumed`
- `RejectedEpoch`
- `RejectedOutOfSequence`

Rules:

1. Same `(frame, epoch)`:
   - pending -> update allowed (`UpdatedPending`)
   - consumed -> rejected (`RejectedConsumed`)
2. New frame in same epoch:
   - must be sequential (contiguous next frame for that epoch)
   - otherwise rejected (`RejectedOutOfSequence`)
3. Wrong epoch:
   - rejected (`RejectedEpoch`)

## Sequential requirement

InputBuffer enforces contiguous growth per epoch.

Example in one epoch:

- valid: `10 -> 11 -> 12`
- invalid: `10 -> 12` (out of sequence)

If no frame is queued yet in current epoch, emulator-side enqueue accepts only current playback frame bootstrap.

## Startup/bootstrap

Emulator core no longer seeds startup input in `open/init`.

Bootstrap responsibility is in producers:

- offline hosts queue frame `0` (or current loaded frame) explicitly after open/load
- netplay runtime queues first timeline frame explicitly after start/reanchor/resync

## Consumption and locking

During frame execution, emulator locks the resolved input for current playback frame.

- lock prevents same-frame mutation races
- enqueue for locked current frame is rejected as consumed-equivalent

## Rollback/resync implications

- late/corrected input for already consumed frame must be handled by rollback/resync policy
- never mutate consumed frame directly
- after rollback/load, producers must reanchor and queue from restored frame forward

## Diagnostics

InputBuffer tracks enqueue counters:

- `inserted`
- `updatedPending`
- `rejectedConsumed`
- `rejectedEpoch`
- `rejectedOutOfSequence`

These are surfaced in netplay runtime reports (`inputEnqueueCounters`) for observability.
