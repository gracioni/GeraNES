# GeraNES

GeraNES is a cycle-accurate NES emulator built with a strong focus on hardware-faithful timing, edge-case correctness, and real-world game compatibility.

The emulator currently passes the full set of blargg and AccuracyCoin test ROMs used by the project, covering demanding CPU, PPU, APU, DMA, interrupt, sprite, mapper, and stress-test scenarios.

If your goal is accuracy first, GeraNES is designed around that philosophy.

**Highlights**

- Cycle-accurate CPU, PPU, APU, DMA, and mapper timing
- Full pass rate on the project's automated test ROM suite
- Strong compatibility with tricky hardware behaviors and commercial games
- Player-friendly features including save states, rewind, speed boost, shaders, and touch controls
- Supports NES, Famicom, NSF, and Famicom Disk System emulation (BIOS required for FDS)
- NSF player with support for expansion audio
- Famicom Disk System disk actions including insert, eject, and side switching
- Loads ROMs from .zip archives and supports .ips, .ups, and .bps patch files with automatic same-name ROM patching
- Flexible input support with keyboard and gamepads
- Emulates NES/Famicom peripherals beyond standard controllers, including Zapper and more
- Built with cross-platform libraries and CMake for Windows, Linux, macOS, and web builds (Emscripten)
- Also available as a libretro core
- Web version available for quick testing and sharing

Try the web version [HERE](https://racionisoft.com/geranes/GeraNES.html)!

## Reports

Generated reports are available here:

- [Mapper coverage](https://gracioni.github.io/GeraNES/tools/generated/mapper_coverage.html)
- [Test report](https://gracioni.github.io/GeraNES/tools/generated/geranes_test_report.html)

You can refresh both reports with `tools/update_reports.sh`.
