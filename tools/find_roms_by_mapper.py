#!/usr/bin/env python3
"""
List all .nes ROMs in a folder (recursive) that use a specific mapper.

Usage:
  python tools/find_roms_by_mapper.py <rom_folder> <mapper_id>
  python tools/find_roms_by_mapper.py <rom_folder> <mapper_id> --show-submapper
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Iterable, Tuple


def parse_nes_mapper(path: Path) -> Tuple[int, int]:
    with path.open("rb") as f:
        header = f.read(16)

    if len(header) < 16 or header[:4] != b"NES\x1A":
        raise ValueError("Not a valid iNES/NES 2.0 file")

    flags6 = header[6]
    flags7 = header[7]
    mapper = (flags6 >> 4) | (flags7 & 0xF0)

    # NES 2.0: bits 2-3 of flags7 are 0b10
    submapper = 0
    if (flags7 & 0x0C) == 0x08:
        mapper |= (header[8] & 0x0F) << 8
        submapper = (header[8] >> 4) & 0x0F

    return mapper, submapper


def iter_nes_files(root: Path) -> Iterable[Path]:
    for p in root.rglob("*"):
        if p.is_file() and p.suffix.lower() == ".nes":
            yield p


def main() -> int:
    parser = argparse.ArgumentParser(description="Find ROMs by mapper ID")
    parser.add_argument("rom_folder", type=Path, help="Folder with .nes ROMs (recursive)")
    parser.add_argument("mapper_id", type=int, help="Mapper ID to search for")
    parser.add_argument(
        "--show-submapper",
        action="store_true",
        help="Show submapper (when NES 2.0 header provides it)",
    )
    args = parser.parse_args()

    if not args.rom_folder.is_dir():
        print(f"Error: ROM folder not found: {args.rom_folder}", file=sys.stderr)
        return 2

    scanned = 0
    invalid = 0
    matches: list[tuple[Path, int]] = []

    for rom in iter_nes_files(args.rom_folder):
        scanned += 1
        try:
            mapper, submapper = parse_nes_mapper(rom)
        except Exception:
            invalid += 1
            continue

        if mapper == args.mapper_id:
            matches.append((rom, submapper))

    print(f"Mapper searched: {args.mapper_id}")
    print(f"ROMs scanned: {scanned}")
    print(f"Invalid/non-iNES skipped: {invalid}")
    print(f"Matches: {len(matches)}")

    if matches:
        print("\nROM list:")
        for rom, submapper in matches:
            if args.show_submapper:
                print(f"  {rom.as_posix()} (submapper {submapper})")
            else:
                print(f"  {rom.as_posix()}")
    else:
        print("No ROMs found for this mapper.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
