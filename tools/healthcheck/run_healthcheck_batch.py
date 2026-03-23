#!/usr/bin/env python3
# Runs GeraNES healthcheck for all ROMs in a folder, analyzes each result,
# and writes consolidated summary/manual-review reports under the output root.
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

import analyze_healthcheck as analyzer


ROM_EXTENSIONS = {".nes", ".fds", ".unf", ".unif", ".nsf"}
SUMMARY_FILE_NAME = "healthcheck_summary.json"
MANUAL_REVIEW_FILE_NAME = "healthcheck_manual_review.json"
STATE_FILE_NAME = "healthcheck_batch_state.json"
TERMINAL_STATUSES = {
    "ok",
    "ok_with_healthcheck_error",
    "healthcheck_failed",
    "healthcheck_missing_content",
    "analysis_failed",
    "skipped_duplicate_stem",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run GeraNES healthcheck for all ROMs in a folder and consolidate the results."
    )
    parser.add_argument("bin_folder", help="Folder containing GeraNES or GeraNES.exe")
    parser.add_argument("roms_folder", help="Folder containing ROMs recursively")
    parser.add_argument("output_root", help="Parent folder where healthcheck outputs will be written")
    parser.add_argument("--seed", type=int, default=0xC0FFEE, help="Deterministic input seed. Default: 12648430")
    parser.add_argument("--sim-seconds", type=int, default=120, help="Emulated duration in seconds. Default: 120")
    parser.add_argument("--shot-interval", type=int, default=10, help="Screenshot interval in emulated seconds. Default: 10")
    return parser.parse_args()


def resolve_binary(bin_folder: Path) -> Path:
    candidates = [
        bin_folder / "GeraNES",
        bin_folder / "GeraNES.exe",
    ]
    for path in candidates:
        if path.is_file():
            return path
    raise FileNotFoundError("GeraNES binary not found in bin_folder. Expected GeraNES or GeraNES.exe.")


def discover_roms(roms_folder: Path) -> list[Path]:
    roms: list[Path] = []
    for root, _, files in os.walk(roms_folder):
        for name in files:
            if Path(name).suffix.lower() in ROM_EXTENSIONS:
                roms.append(Path(root) / name)
    roms.sort()
    return roms


def run_command(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def normalize_process_output(proc: subprocess.CompletedProcess[str]) -> str:
    stdout = proc.stdout or ""
    stderr = proc.stderr or ""
    if stdout and stderr and not stdout.endswith("\n"):
        return f"{stdout}\n{stderr}"
    return stdout + stderr


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=2)


def utc_now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def build_run_config(binary: Path, roms_folder: Path, output_root: Path, args: argparse.Namespace) -> dict[str, Any]:
    return {
        "binaryPath": str(binary),
        "romsFolder": str(roms_folder),
        "outputRoot": str(output_root),
        "seed": args.seed,
        "simSeconds": args.sim_seconds,
        "screenshotIntervalSeconds": args.shot_interval,
    }


def load_state(path: Path, run_config: dict[str, Any]) -> dict[str, Any]:
    if not path.is_file():
        return {
            "schemaVersion": 1,
            "createdAtUtc": utc_now_iso(),
            "updatedAtUtc": utc_now_iso(),
            "runConfig": run_config,
            "roms": {},
        }

    with path.open("r", encoding="utf-8") as f:
        state = json.load(f)

    if not isinstance(state, dict):
        raise ValueError("Invalid batch state file.")

    existing_config = state.get("runConfig", {})
    if existing_config != run_config:
        return {
            "schemaVersion": 1,
            "createdAtUtc": utc_now_iso(),
            "updatedAtUtc": utc_now_iso(),
            "runConfig": run_config,
            "roms": {},
            "replacedPreviousState": True,
        }

    if not isinstance(state.get("roms"), dict):
        state["roms"] = {}
    state["updatedAtUtc"] = utc_now_iso()
    return state


def save_state(path: Path, state: dict[str, Any]) -> None:
    state["updatedAtUtc"] = utc_now_iso()
    write_json(path, state)


def make_state_key(rom_path: Path) -> str:
    return str(rom_path)


def can_resume_entry(entry: dict[str, Any]) -> bool:
    status = str(entry.get("status", ""))
    if status not in TERMINAL_STATUSES:
        return False

    report_path = entry.get("reportPath")
    if status in {"ok", "ok_with_healthcheck_error"}:
        return isinstance(report_path, str) and Path(report_path).is_file()
    return True


def has_minimum_generated_content(output_dir: Path) -> tuple[bool, str]:
    required_files = [
        output_dir / "run.json",
        output_dir / "metrics.csv",
        output_dir / "events.jsonl",
        output_dir / "log.txt",
    ]
    missing = [path.name for path in required_files if not path.is_file()]
    if missing:
        return False, f"Missing required healthcheck artifact(s): {', '.join(missing)}"

    frames_dir = output_dir / "frames"
    if not frames_dir.is_dir():
        return False, "Missing frames directory."

    png_count = sum(1 for _ in frames_dir.glob("*.png"))
    if png_count == 0:
        return False, "No screenshot files were generated."

    return True, ""


def override_verdict_for_missing_visual_content(report: dict[str, Any]) -> tuple[str, str]:
    analysis = report.get("analysis", {})
    screens = analysis.get("screens", {})
    boot = analysis.get("boot", {})

    shot_count = int(screens.get("shotCount", 0) or 0)
    missing_files = screens.get("missingFiles", [])
    average_changed_pixels = float(screens.get("averageChangedPixels", 0.0) or 0.0)
    average_unique_colors = float(screens.get("averageUniqueColors", 0.0) or 0.0)
    near_blank_shot_count = int(screens.get("nearBlankShotCount", 0) or 0)
    first_meaningful_frame_seconds = boot.get("firstMeaningfulFrameSeconds")

    if shot_count <= 0:
        return "bad", "Healthcheck produced no screenshots."
    if isinstance(missing_files, list) and missing_files:
        return "bad", "Healthcheck is missing one or more screenshot files referenced in run.json."
    if first_meaningful_frame_seconds is None and average_changed_pixels <= 0.0 and average_unique_colors <= 2.0:
        return "bad", "Healthcheck did not produce any meaningful visual content."
    if near_blank_shot_count >= shot_count and average_unique_colors <= 2.0:
        return "bad", "All screenshots were blank or nearly blank."

    return str(report.get("finalVerdict", "bad")), ""


def build_duplicate_stem_set(roms: list[Path]) -> set[str]:
    counts: dict[str, int] = {}
    for rom in roms:
        stem = rom.stem
        counts[stem] = counts.get(stem, 0) + 1
    return {stem for stem, count in counts.items() if count > 1}


def main() -> int:
    args = parse_args()
    bin_folder = Path(args.bin_folder).resolve()
    roms_folder = Path(args.roms_folder).resolve()
    output_root = Path(args.output_root).resolve()

    if not bin_folder.is_dir():
        print("bin_folder is not a directory.", file=sys.stderr)
        return 2
    if not roms_folder.is_dir():
        print("roms_folder is not a directory.", file=sys.stderr)
        return 2
    if args.sim_seconds <= 0 or args.shot_interval <= 0:
        print("--sim-seconds and --shot-interval must be greater than zero.", file=sys.stderr)
        return 2

    try:
        binary = resolve_binary(bin_folder)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    roms = discover_roms(roms_folder)
    if not roms:
        print("No ROMs found.", file=sys.stderr)
        return 2

    output_root.mkdir(parents=True, exist_ok=True)
    state_path = output_root / STATE_FILE_NAME
    duplicate_stems = build_duplicate_stem_set(roms)
    run_config = build_run_config(binary, roms_folder, output_root, args)
    state = load_state(state_path, run_config)
    if state.get("replacedPreviousState"):
        print("Existing batch state did not match the current parameters. Starting a new resumable session.", flush=True)
        save_state(state_path, state)

    summary_entries: list[dict[str, Any]] = []
    manual_review_entries: list[dict[str, Any]] = []
    counts = {
        "good": 0,
        "suspicious": 0,
        "bad": 0,
        "healthcheckFailed": 0,
        "analysisFailed": 0,
        "duplicateStemSkipped": 0,
    }

    total = len(roms)
    for index, rom_path in enumerate(roms, start=1):
        rel_rom_path = rom_path.relative_to(roms_folder).as_posix()
        output_dir = output_root / rom_path.stem
        print(f"[{index}/{total}] {rel_rom_path}", flush=True)
        state_key = make_state_key(rom_path)
        previous_entry = state["roms"].get(state_key)

        if isinstance(previous_entry, dict) and can_resume_entry(previous_entry):
            summary_entry = dict(previous_entry.get("summaryEntry", {}))
            if summary_entry:
                summary_entries.append(summary_entry)
                if summary_entry.get("needsManualReview"):
                    manual_review_entries.append(summary_entry)
                verdict = str(summary_entry.get("finalVerdict", "bad"))
                counts[verdict] = counts.get(verdict, 0) + 1
                if summary_entry.get("status") == "healthcheck_failed":
                    counts["healthcheckFailed"] += 1
                elif summary_entry.get("status") == "analysis_failed":
                    counts["analysisFailed"] += 1
                elif summary_entry.get("status") == "skipped_duplicate_stem":
                    counts["duplicateStemSkipped"] += 1
                elif summary_entry.get("status") == "healthcheck_missing_content":
                    counts["healthcheckFailed"] += 1
                print(f"  resumed: {verdict}", flush=True)
                continue

        if rom_path.stem in duplicate_stems:
            counts["duplicateStemSkipped"] += 1
            entry = {
                "romPath": str(rom_path),
                "relativeRomPath": rel_rom_path,
                "outputDir": str(output_dir),
                "status": "skipped_duplicate_stem",
                "finalVerdict": "bad",
                "reason": "ROM stem collides with another ROM name. Healthcheck output folder would conflict.",
                "needsManualReview": True,
            }
            summary_entries.append(entry)
            manual_review_entries.append(entry)
            state["roms"][state_key] = {
                "status": entry["status"],
                "updatedAtUtc": utc_now_iso(),
                "summaryEntry": entry,
            }
            save_state(state_path, state)
            print("  skipped: duplicate ROM stem would collide in output folder", flush=True)
            continue

        proc = run_command([
            str(binary),
            "--healthcheck",
            str(rom_path),
            str(output_root),
            "--seed",
            str(args.seed),
            "--sim-seconds",
            str(args.sim_seconds),
            "--shot-interval",
            str(args.shot_interval),
        ])

        has_content, content_reason = has_minimum_generated_content(output_dir)
        if proc.returncode != 0 and not has_content:
            counts["healthcheckFailed"] += 1
            entry = {
                "romPath": str(rom_path),
                "relativeRomPath": rel_rom_path,
                "outputDir": str(output_dir),
                "status": "healthcheck_failed",
                "finalVerdict": "bad",
                "returnCode": proc.returncode,
                "reason": content_reason or "Healthcheck process failed before generating artifacts.",
                "processOutput": normalize_process_output(proc).strip(),
                "needsManualReview": True,
            }
            summary_entries.append(entry)
            manual_review_entries.append(entry)
            state["roms"][state_key] = {
                "status": entry["status"],
                "updatedAtUtc": utc_now_iso(),
                "summaryEntry": entry,
            }
            save_state(state_path, state)
            print(f"  healthcheck failed (code {proc.returncode})", flush=True)
            continue
        if not has_content:
            counts["healthcheckFailed"] += 1
            entry = {
                "romPath": str(rom_path),
                "relativeRomPath": rel_rom_path,
                "outputDir": str(output_dir),
                "status": "healthcheck_missing_content",
                "finalVerdict": "bad",
                "reason": content_reason,
                "needsManualReview": True,
            }
            summary_entries.append(entry)
            manual_review_entries.append(entry)
            state["roms"][state_key] = {
                "status": entry["status"],
                "updatedAtUtc": utc_now_iso(),
                "summaryEntry": entry,
            }
            save_state(state_path, state)
            print(f"  bad: {content_reason}", flush=True)
            continue

        try:
            report = analyzer.analyze_healthcheck(output_dir)
            report_path = output_dir / analyzer.REPORT_FILE_NAME
            write_json(report_path, report)
        except Exception as exc:
            counts["analysisFailed"] += 1
            entry = {
                "romPath": str(rom_path),
                "relativeRomPath": rel_rom_path,
                "outputDir": str(output_dir),
                "status": "analysis_failed",
                "finalVerdict": "bad",
                "reason": str(exc),
                "needsManualReview": True,
            }
            summary_entries.append(entry)
            manual_review_entries.append(entry)
            state["roms"][state_key] = {
                "status": entry["status"],
                "updatedAtUtc": utc_now_iso(),
                "summaryEntry": entry,
            }
            save_state(state_path, state)
            print(f"  analysis failed: {exc}", flush=True)
            continue

        final_verdict, override_reason = override_verdict_for_missing_visual_content(report)
        if override_reason:
            report["finalVerdict"] = final_verdict
            report["batchOverride"] = {
                "applied": True,
                "reason": override_reason,
            }
            report_path = output_dir / analyzer.REPORT_FILE_NAME
            write_json(report_path, report)

        counts[final_verdict] = counts.get(final_verdict, 0) + 1
        decision = report.get("decision", {})
        entry = {
            "romPath": str(rom_path),
            "relativeRomPath": rel_rom_path,
            "outputDir": str(output_dir),
            "status": "ok" if proc.returncode == 0 else "ok_with_healthcheck_error",
            "finalVerdict": final_verdict,
            "needsManualReview": final_verdict != "good",
            "hardChecksPassed": bool(decision.get("hardChecksPassed", False)),
            "hardCheckFailures": decision.get("hardCheckFailures", []),
            "softCheckFailures": decision.get("softCheckFailures", []),
            "reportPath": str(output_dir / analyzer.REPORT_FILE_NAME),
        }
        if proc.returncode != 0:
            entry["returnCode"] = proc.returncode
            entry["processOutput"] = normalize_process_output(proc).strip()
        if override_reason:
            entry["batchOverrideReason"] = override_reason
        summary_entries.append(entry)
        if entry["needsManualReview"]:
            manual_review_entries.append(entry)
        state["roms"][state_key] = {
            "status": entry["status"],
            "updatedAtUtc": utc_now_iso(),
            "summaryEntry": entry,
        }
        save_state(state_path, state)
        print(f"  verdict: {final_verdict}", flush=True)

    generated_at = utc_now_iso()
    summary = {
        "schemaVersion": 1,
        "generatedAtUtc": generated_at,
        "binaryPath": str(binary),
        "romsFolder": str(roms_folder),
        "outputRoot": str(output_root),
        "seed": args.seed,
        "simSeconds": args.sim_seconds,
        "screenshotIntervalSeconds": args.shot_interval,
        "totalRoms": len(roms),
        "counts": counts,
        "manualReviewCount": len(manual_review_entries),
        "results": summary_entries,
    }
    manual_review = {
        "schemaVersion": 1,
        "generatedAtUtc": generated_at,
        "outputRoot": str(output_root),
        "totalManualReview": len(manual_review_entries),
        "roms": manual_review_entries,
    }

    summary_path = output_root / SUMMARY_FILE_NAME
    manual_review_path = output_root / MANUAL_REVIEW_FILE_NAME
    write_json(summary_path, summary)
    write_json(manual_review_path, manual_review)
    save_state(state_path, state)

    print(summary_path, flush=True)
    print(manual_review_path, flush=True)
    print(state_path, flush=True)
    print(f"Manual review ROMs: {len(manual_review_entries)}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
