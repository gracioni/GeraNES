# GeraNES

GeraNES is a cycle-accurate NES emulator built with a strong focus on hardware-faithful timing, edge-case correctness, and real-world game compatibility.

The emulator currently passes the full set of blargg and AccuracyCoin test ROMs used by the project, covering demanding CPU, PPU, APU, DMA, interrupt, sprite, mapper, and stress-test scenarios.

If your goal is accuracy first, GeraNES is designed around that philosophy.

**Highlights**

- Cycle-accurate CPU, PPU, APU, DMA, and mapper timing
- Full pass rate on the blargg and AccuracyCoin automated test suites
- Strong compatibility with tricky hardware behaviors and commercial games
- Player-friendly features including save states, rewind, speed boost, touch controls, palettes, and stackable shader passes with presets
- Netplay support with room-based multiplayer
- Visual mod loading from ZIP files or folders, with compatibility for supported Mesen-style HD packs built around `hires.txt`
- Supports NES, Famicom, NSF, and Famicom Disk System emulation (BIOS required for FDS)
- NSF player with support for expansion audio
- Loads ROMs from .zip archives and supports .ips, .ups, and .bps patch files with automatic same-name ROM patching
- Flexible input support with keyboard, gamepads, and touch-friendly layouts on supported builds
- Emulates NES/Famicom peripherals beyond standard controllers, including Zapper, Power Pad, mice, multitaps, and more
- Built with cross-platform libraries and CMake for Windows, Linux, macOS, web builds (Emscripten), and Android native builds
- Also available as a libretro core
- Web version available for quick testing and sharing

Try the web version [HERE](https://racionisoft.com/geranes/GeraNES.html?settings=ewogICJuZXRwbGF5IjogewogICAgInNpZ25hbGluZ1VybCI6ICJ3c3M6Ly9zaWduYWwucmFjaW9uaXNvZnQuY29tL3dzLyIKICB9Cn0=)!

## Reports

Generated reports are available here:

- [Mapper coverage](https://gracioni.github.io/GeraNES/tools/generated/mapper_coverage.html)
- [Test report](https://gracioni.github.io/GeraNES/tools/generated/geranes_test_report.html)

You can refresh both reports with `tools/update_reports.sh`.

## Android builds

`build.sh android` now drives the full Android packaging flow:

- requires Android settings in `android/build-config.jsonc`
- generates an Android Gradle project from the repo template
- builds the native SDL `main` shared library through Gradle/CMake
- packages either an `.apk` or an `.aab`
- applies release signing when a keystore is provided

Required config file flow:

- copy `android/build-config.example.jsonc` to `android/build-config.jsonc`
- fill in the SDK, NDK, JDK, package, and optional signing fields
- run `./build.sh android ...`

Resolution rules:

- `android/build-config.jsonc`
- platform defaults inside the script/Gradle template

Important JSONC fields:

- `sdkRoot`
- `ndkRoot`
- `javaHome`
- `packageFormat`
- `abis`
- `api`
- `stl`
- `appName`
- `applicationId`
- `namespace`
- `versionCode`
- `versionName`
- `compileSdk`
- `targetSdk`
- `signing.keystorePath`
- `signing.keystorePassword`
- `signing.keyAlias`
- `signing.keyPassword`

Example signed AAB build with a project config file:

```sh
cp android/build-config.example.jsonc android/build-config.jsonc
# edit android/build-config.jsonc
./build.sh android Release 8
```

More Android-specific details are documented in [android/README.md](android/README.md).
