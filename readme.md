# GeraNES

GeraNES is a cycle-accurate NES emulator built with a strong focus on hardware-faithful timing, edge-case correctness, and real-world game compatibility.

The emulator currently passes the full set of blargg and AccuracyCoin test ROMs used by the project, covering demanding CPU, PPU, APU, DMA, interrupt, sprite, mapper, and stress-test scenarios.

If your goal is accuracy first, GeraNES is designed around that philosophy.

**Highlights**

- Cycle-accurate CPU, PPU, APU, DMA, and mapper timing
- Full pass rate on the project's automated test ROM suite
- Strong compatibility with tricky hardware behaviors and commercial games
- NSF player with support for expansion audio
- Built-in generated reports for mapper coverage and test results
- Web version available for quick testing and sharing

Try the web version [HERE](https://racionisoft.com/geranes/GeraNES.html)!

## Reports

Generated reports are available here:

- [Mapper coverage](https://gracioni.github.io/GeraNES/tools/generated/mapper_coverage.html)
- [Test report](https://gracioni.github.io/GeraNES/tools/generated/geranes_test_report.html)

You can refresh both reports with `tools/update_reports.sh`.
