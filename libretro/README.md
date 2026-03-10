# GeraNES Libretro Core

This repository includes a `libretro` build target intended for RetroArch buildbot style builds.

## Build

From repository root:

```bash
make
```

Or directly:

```bash
make -C libretro
```

On Windows/MinGW, the Makefile auto-detects the toolchain and emits `.dll`
even when `platform` is not passed. You can still force it explicitly:

```bash
make -C libretro platform=mingw
```

Build profile:

- Release (default): `make -C libretro`
- Debug: `make -C libretro BUILD=debug`

Output artifact:

- `geranes_libretro.so` (Linux)
- `geranes_libretro.dll` (Windows/MinGW)
- `geranes_libretro.dylib` (macOS)

## Notes

- Runtime patching support is enabled (`.ips`, `.ups`, `.bps`).
- Standard ROM formats (`.nes`, `.zip`) are supported.
