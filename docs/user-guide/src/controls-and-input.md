# Controls and Input

## Where Input Is Configured

Open `Options > Input`.

This area controls player devices, expansion devices, and controller-specific configuration windows.

## Standard Use

For normal NES play, the common setup is:

- `Port 1 > Standard Controller`
- `Port 2 > Standard Controller`

After assigning a device to a port, use the relevant configuration entry to map buttons.

## Supported Device Types

GeraNES supports more than basic controllers. Depending on the port, you may see options such as:

- Standard Controller
- Famicom Controller
- Zapper
- Power Pad
- SNES Mouse
- Subor Mouse
- SNES Controller
- Virtual Boy Controller
- Arkanoid Controller
- Expansion devices for Famicom-style setups
- Multitap options such as Four Score and Hori Adapter

## Keyboard and Gamepads

You can use keyboard controls, gamepads, or both depending on your configuration. If a button does not respond, re-open the relevant input configuration window and remap it.

## Touch Controls

On builds that expose touch controls, GeraNES can show an on-screen input layout. This is most useful on web or touch-focused deployments.

## Netplay Input Notes

During netplay, some input assignments can be managed by the session. In that case:

- some port changes may be locked
- some local configuration actions may only be available for the slots assigned to you

If a setting looks disabled during netplay, it is usually a session ownership or assignment rule rather than a bug.

## If Input Does Not Work

Check these points:

1. Make sure the correct device type is assigned to the correct port.
2. Re-open the configuration dialog and confirm the binding was saved.
3. If using netplay, verify that the slot is assigned to your local player.
4. If using a specialty controller, make sure the game actually supports it.
