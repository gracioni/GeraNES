# Netplay

## What Netplay Changes

Netplay adds session ownership, player-slot assignment, and synchronization rules. Because of that, some actions that are normally available in single-player may be restricted.

## Common Restrictions

Depending on your role in the room:

- only the owner may be able to pause the session
- ROM changes may be blocked for clients
- save states may be blocked for clients
- rewind may be disabled during netplay
- CPU debugger features may be blocked while netplay is active

If an action is disabled, check whether you are the session owner or a client.

## Input Assignment

Controller slots can be assigned to different participants. During a session:

- only your assigned local slots may be configurable
- some port and expansion options may become read-only

## Stability Advice

For the best netplay experience:

- make sure every participant is using the correct ROM
- avoid changing core gameplay-affecting settings mid-session
- avoid relying on features that are intentionally restricted by the session owner

## If Netplay Feels Broken

Before assuming a bug, verify:

1. you joined the expected room
2. your local input slot assignment is correct
3. the action you want is not owner-only
4. the current game is actually loaded and running
