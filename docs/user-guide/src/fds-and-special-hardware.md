# FDS and Special Hardware

## Famicom Disk System

GeraNES supports FDS, but it requires the expected BIOS setup.

If an FDS game does not boot correctly, verify that the BIOS required by your build is present and correctly located for the emulator runtime.

## FDS Runtime Commands

When an FDS title is loaded, GeraNES exposes hardware actions such as:

- `FDS - Switch Disk Side`
- `FDS - Eject Disk`
- `FDS - Insert Next Disk`

These appear in the hardware menu only when relevant.

## When To Use These Commands

Use them when a game asks for a disk swap or behaves like it is waiting on drive state changes.

## VS System Actions

Some VS titles expose coin-related hardware actions. These are only shown when the loaded software supports them.

## Specialty Controllers

Some games need non-standard hardware to play correctly, such as:

- Zapper
- Arkanoid controller
- Power Pad or Family Trainer
- Famicom expansion devices

If gameplay seems wrong, check that the correct device is selected under `Options > Input`.
