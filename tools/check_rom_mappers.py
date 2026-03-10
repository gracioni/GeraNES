#!/usr/bin/env python3
"""
Scan a ROM folder and report .nes files that use mappers not implemented in GeraNES.

Usage:
  python tools/check_rom_mappers.py <rom_folder>
  python tools/check_rom_mappers.py <rom_folder> --mappers-dir src/GeraNES/Mappers
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable, Tuple


MAPPER_FILE_RE = re.compile(r"^Mapper(\d+)(?:_.*)?\.h$", re.IGNORECASE)


def discover_implemented_mappers(mappers_dir: Path) -> set[int]:
    if not mappers_dir.is_dir():
        raise FileNotFoundError(f"Mappers directory not found: {mappers_dir}")

    mappers: set[int] = set()
    for entry in mappers_dir.iterdir():
        if not entry.is_file():
            continue
        match = MAPPER_FILE_RE.match(entry.name)
        if match:
            mappers.add(int(match.group(1)))
    return mappers


def parse_nes_mapper(path: Path) -> Tuple[int, int]:
    with path.open("rb") as f:
        header = f.read(16)

    if len(header) < 16 or header[:4] != b"NES\x1a":
        raise ValueError("Not a valid iNES/NES 2.0 file")

    flags6 = header[6]
    flags7 = header[7]
    mapper = (flags6 >> 4) | (flags7 & 0xF0)

    # NES 2.0 identification: bits 2-3 of flags7 == 2
    nes2 = (flags7 & 0x0C) == 0x08
    submapper = 0
    if nes2:
        mapper |= (header[8] & 0x0F) << 8
        submapper = (header[8] >> 4) & 0x0F

    return mapper, submapper


def iter_nes_files(root: Path) -> Iterable[Path]:
    for p in root.rglob("*"):
        if p.is_file() and p.suffix.lower() == ".nes":
            yield p


def main() -> int:
    parser = argparse.ArgumentParser(description="Check unsupported mappers in a ROM folder")
    parser.add_argument("rom_folder", type=Path, help="Folder containing .nes files (recursive)")
    parser.add_argument(
        "--mappers-dir",
        type=Path,
        default=Path("src/GeraNES/Mappers"),
        help="Directory used to discover implemented mappers (default: src/GeraNES/Mappers)",
    )
    parser.add_argument(
        "--show-supported",
        action="store_true",
        help="Print the discovered mapper IDs implemented by the emulator",
    )
    args = parser.parse_args()

    if not args.rom_folder.is_dir():
        print(f"Error: ROM folder not found: {args.rom_folder}", file=sys.stderr)
        return 2

    try:
        implemented = discover_implemented_mappers(args.mappers_dir)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2

    if args.show_supported:
        print("Implemented mapper IDs:", ", ".join(str(m) for m in sorted(implemented)))

    total = 0
    invalid = 0
    unsupported: list[tuple[Path, int, int]] = []
    by_mapper: defaultdict[int, int] = defaultdict(int)

    for rom in iter_nes_files(args.rom_folder):
        total += 1
        try:
            mapper, submapper = parse_nes_mapper(rom)
        except Exception:
            invalid += 1
            continue

        if mapper not in implemented:
            unsupported.append((rom, mapper, submapper))
            by_mapper[mapper] += 1

    print(f"ROMs scanned: {total}")
    print(f"Invalid/non-iNES skipped: {invalid}")
    print(f"Implemented mapper IDs found: {len(implemented)}")
    print(f"Unsupported ROMs: {len(unsupported)}")

    if unsupported:
        print("\nUnsupported mapper summary:")
        for mapper_id in sorted(by_mapper):
            print(f"  Mapper {mapper_id}: {by_mapper[mapper_id]} ROM(s)")

        print("\nUnsupported ROM list:")
        for rom, mapper, submapper in unsupported:
            rel = rom.as_posix()
            if submapper:
                print(f"  Mapper {mapper} (submapper {submapper}): {rel}")
            else:
                print(f"  Mapper {mapper}: {rel}")
        return 1

    print("All scanned ROMs use implemented mappers.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
