# Loading Games

## Supported Formats

GeraNES supports:

- `.nes`
- `.fds`
- `.nsf`
- `.zip` archives containing supported content

It also supports ROM patch workflows using `.ips`, `.ups`, and `.bps` files when the patch matches the ROM name.

## Open A Game

1. Open `File > Open ROM`.
2. Choose your file.
3. Wait for the game or NSF player to load.

You can also return to a recently opened title with `File > Recent Files`.

## Close The Current Game

Use `File > Close ROM`.

This is useful when you want to switch games without restarting the app.

## NSF Files

If you open an `.nsf` file, GeraNES loads its NSF playback interface instead of normal game emulation. This is meant for music playback and soundtrack exploration.

## ZIP Archives

If your ROM is stored in a `.zip`, GeraNES can load it directly. This avoids the need to extract every ROM manually.

## Patch Files

GeraNES supports automatic patch workflows for common ROM patch formats:

- `.ips`
- `.ups`
- `.bps`

If the patch uses the same base filename as the ROM, the emulator can apply it automatically.

## Region

Some games behave better with the expected region. If needed, change it at:

- `Emulator > Region > NTSC`
- `Emulator > Region > PAL`
