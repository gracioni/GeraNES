# Replay

## What Replay Does

The replay feature records your play session as timed input data and lets GeraNES play that session back later.

This is useful for:

- saving a full run or practice session
- checking a tricky section again
- continuing a run from an earlier replay position
- sharing a reproducible session with someone who has the same ROM

Replay files use the `.replay` extension.

## Opening the Replay Window

Open the replay controls from `Tools > Replay`.

The Replay window shows:

- the current mode: idle, recording, replay loaded, playback, or seeking
- the ROM name and CRC
- the replay file path
- the current replay time and total time
- the current frame position
- the input topology used by the replay

## Recording a Replay

To create a replay:

1. load the ROM you want to play
2. open `Tools > Replay`
3. press `Record`

When recording starts, GeraNES reloads the current ROM and begins capturing the session from the start with the current input configuration.

When you stop recording, GeraNES asks where to save the replay file.

## Playing a Replay

To play a replay:

1. load the matching ROM first
2. open `Tools > Replay`
3. use `File > Load Replay`

If the replay matches the loaded ROM, GeraNES loads it, seeks to the start, and begins playback.

Playback controls:

- `Play`: start or resume playback
- `Pause`: pause playback at the current position
- `Stop`: return to the start of the replay
- `Position`: move to another point in the replay when playback is paused
- `Speed`: change emulation speed while viewing the replay

## Seeking Through a Replay

When a replay is loaded and paused, you can drag the `Position` slider to jump to another frame.

GeraNES also shows the replay time for the current slider position, which makes it easier to jump to a specific moment in a long session.

## Continue Recording From a Replay

One of the main replay features is the ability to branch from an existing replay.

If a replay is loaded and paused, you can press `Record` again to continue recording from the current replay position. This lets you:

- retry a difficult section from a chosen point
- create an alternate version of a run
- extend a replay beyond its original ending

The original replay is closed and the new recording continues from the current cursor position.

## What Replay Preserves

A replay keeps the session metadata needed for playback, including:

- the ROM identity
- the recorded input timeline
- the input topology used for the session

The input topology includes the configured devices for controller ports and supported expansion or multitap setups. During replay, that topology is locked for the session.

## Restrictions and Expected Behavior

Replay is designed for single-system playback, so some actions are intentionally limited while a replay session is active.

Common restrictions:

- replay is unavailable while netplay is active
- opening or closing ROMs is blocked during replay recording or playback
- some save state actions are blocked during replay sessions
- resetting the emulator is blocked while replay recording is active
- loading, clearing, or changing mods is blocked during replay sessions
- CPU debugger access is blocked during replay sessions
- input topology changes are locked for the full replay session

If a button or menu item is disabled while replay is active, that is expected behavior.

## ROM Compatibility

A replay must match the loaded ROM. If the ROM CRC does not match, GeraNES refuses to open the replay.

If a replay does not load, verify that:

- you loaded the correct ROM before opening the replay
- the ROM is the same revision used when the replay was recorded
- the replay file is valid and was saved completely
