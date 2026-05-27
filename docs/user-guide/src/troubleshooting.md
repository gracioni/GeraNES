# Troubleshooting

## The ROM Does Not Open

Check the following:

1. The file format is supported.
2. The ROM is not corrupted.
3. If using a patch, the patch actually matches the ROM.
4. If using a `.zip`, the archive contains the expected game file.

## Controls Do Not Respond

Check:

1. The correct controller type is assigned to the correct port.
2. The device was configured after being assigned.
3. Netplay is not restricting the slot you are trying to control.
4. The game does not require specialty hardware.

## The Picture Looks Wrong

Try these fixes:

- change `Options > Video > Filter`
- disable the active shader
- choose another palette
- adjust overscan
- choose a different scale mode

## Audio Crackles Or Uses The Wrong Device

Try:

1. selecting the intended device in `Options > Audio > Device`
2. resetting sample settings to a simpler default
3. closing other software that may be aggressively switching audio devices

## FDS Game Does Not Work

Verify the required FDS BIOS is present for your runtime setup. Without it, FDS software may fail to boot or behave incorrectly.

## Netplay Actions Are Disabled

That is often expected. Many actions depend on:

- whether you are the room owner
- whether the session is paused or running
- whether your local machine owns the relevant input slot

## Still Stuck

When reporting a problem, include:

- the game format
- whether it is single-player or netplay
- the device type in use
- the exact menu action that failed
- whether shaders, palettes, or specialty hardware were involved
