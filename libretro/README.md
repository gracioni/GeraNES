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

Output artifact:

- `geranes_libretro.so` (Linux)
- `geranes_libretro.dll` (Windows/MinGW)
- `geranes_libretro.dylib` (macOS)

## Notes

- Libretro builds define `GERANES_DISABLE_PATCHES`.
  - `.ips/.ups/.bps` runtime patching is disabled in this core build.
- Standard ROM formats (`.nes`, `.zip`) are supported.
