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
