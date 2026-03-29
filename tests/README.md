# Tests

This project now uses a dedicated Catch2/CTest target for most emulator and netplay validation:

- `GeraNESTests`

The desktop app executable `GeraNES` keeps only the end-to-end entry points:

- `GeraNES --test <rom_path>`
- `GeraNES --healthcheck ...`

## Running the unit/integration test target

From the repo root:

```powershell
cmake --build build --target GeraNESTests -j 4
ctest --test-dir build --output-on-failure
```

You can also run the test binary directly:

```powershell
.\build\GeraNESTests.exe
```

Or filter by tag/name:

```powershell
.\build\GeraNESTests.exe [netplay]
.\build\GeraNESTests.exe "State replay remains deterministic from saved snapshots"
```

## ROM fixture selection

Tests use a ROM fixture path in this order:

1. `GERANES_TEST_ROM` environment variable
2. the CMake-configured default fixture

Current default fixture:

- `tests/roms/ppu_vbl_nmi/ppu_vbl_nmi.nes`

To override it for the current shell:

```powershell
$env:GERANES_TEST_ROM="C:\path\to\your\fixture.nes"
ctest --test-dir build --output-on-failure
```

This is especially useful for gameplay-heavy netplay scenarios.

## Why one netplay test is skipped by default

The Catch2 test:

- `Netplay robust matrix stays green`

is intentionally skipped when the default `ppu_vbl_nmi.nes` fixture is active. That ROM is great for deterministic smoke coverage, but it does not reliably produce the gameplay/prediction activity required by the full robust netplay matrix.

To enable that test, point `GERANES_TEST_ROM` at a gameplay-oriented fixture ROM.

## VS Code usage

The project is configured so VS Code can discover tests through CMake/CTest.

Recommended extensions:

- `CMake Tools`
- `Test Explorer UI`
- `C++ TestMate`

Typical flow:

1. Configure the project with CMake Tools
2. Build `GeraNESTests`
3. Open the Testing panel
4. Run or debug individual tests

Project settings already enable CMake test explorer integration in `.vscode/settings.json`.

## Test layout

Main files:

- [`tests/StateReplayTests.cpp`](/c:/Users/geral/Desktop/pacman/GeraNES/tests/StateReplayTests.cpp)
- [`tests/ResimulationTests.cpp`](/c:/Users/geral/Desktop/pacman/GeraNES/tests/ResimulationTests.cpp)
- [`tests/NetplayTests.cpp`](/c:/Users/geral/Desktop/pacman/GeraNES/tests/NetplayTests.cpp)
- [`tests/TestSupport.h`](/c:/Users/geral/Desktop/pacman/GeraNES/tests/TestSupport.h)
- [`tests/TestConfig.h.in`](/c:/Users/geral/Desktop/pacman/GeraNES/tests/TestConfig.h.in)

## Notes

- The Catch2 target is the primary place for replay, resimulation, and netplay logic validation.
- The app executable remains useful for end-to-end smoke coverage and real desktop/runtime verification.
