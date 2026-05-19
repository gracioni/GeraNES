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

`GeraNESTests` requires a ROM fixture path. Tests resolve it in this order:

1. `GERANES_TEST_ROM` environment variable

There is no built-in default ROM in this repo. You must define one explicitly before running the test suite.

To define it for the current shell:

```powershell
$env:GERANES_TEST_ROM="C:\path\to\your\fixture.nes"
ctest --test-dir build --output-on-failure
```

For the robust netplay matrix, prefer a ROM fixture with:

- sustained player-driven gameplay
- frequent input changes
- meaningful remote input/rollback opportunities

## Why one netplay test is skipped by default

The Catch2 test:

- `Netplay robust matrix stays green`

is skipped when the selected ROM does not generate enough gameplay/rollback activity for that matrix. This is not tied to a specific title; it is about the characteristics of the fixture.

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

### Selecting the ROM fixture in VS Code

The simplest approach is to start VS Code from a shell that already has `GERANES_TEST_ROM` defined, or define it in the integrated terminal before running the tests.

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
