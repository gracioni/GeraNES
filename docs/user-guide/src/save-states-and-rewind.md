# Save States and Rewind

## Save States

GeraNES supports save states through the `Emulator` menu.

Main actions:

- `Emulator > Save State`
- `Emulator > Load State`
- `Emulator > Slot`

Shortcuts:

- `Alt+S`: Save State
- `Alt+L`: Load State

## Save State Slots

The current slot is selected in `Emulator > Slot`. GeraNES exposes slots `0` through `9`.

If you load the wrong state, check the current slot before trying again.

## Rewind

Rewind is part of the emulator's player-friendly improvements.

If rewind is enabled in your configuration, holding the mapped rewind control moves gameplay backward through recent history. The exact amount of history depends on the configured rewind buffer.

## Speed Boost

GeraNES also supports a speed boost input for fast traversal through slow sections such as menus, intros, or grinding.

## Netplay Restrictions

Some state-related features are restricted in netplay:

- clients may not be allowed to save or load states
- rewind may be suppressed during netplay to keep synchronization stable

If the UI blocks a state action during netplay, that is expected behavior.

## Recommended Use

- Use save states for practice and experimentation
- Use rewind for short corrections
- Do not depend on save states as a replacement for in-game saves when you want maximum compatibility across emulator versions
