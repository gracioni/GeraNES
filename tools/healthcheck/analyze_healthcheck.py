#!/usr/bin/env python3
# Analyzes a GeraNES healthcheck output folder and writes a JSON report beside it.
# For OCR-based text validation, this script expects the `tesseract` executable to
# be installed and available in PATH. Without it, text validation is skipped.
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
import struct
import zlib
from pathlib import Path
from typing import Any


REPORT_FILE_NAME = "healthcheck_report.json"
COMMON_OCR_WORDS = {
    "START", "PRESS", "PUSH", "OPTION", "OPTIONS", "GAME", "PLAYER", "PLAY",
    "MEGA", "MAN", "CAPCOM", "NINTENDO", "LICENSED", "CONTINUE", "STAGE",
    "SCORE", "SELECT", "PASSWORD", "INSERT", "COIN", "ROUND", "WORLD", "LEVEL",
    "PAUSE", "BONUS", "HIGH", "LOAD", "SAVE",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Analyze a GeraNES healthcheck folder and write a JSON report."
    )
    parser.add_argument(
        "healthcheck_dir",
        help="Folder containing run.json, metrics.csv, events.jsonl, log.txt and frames/",
    )
    return parser.parse_args()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line:
                continue
            rows.append(json.loads(line))
    return rows


def load_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def load_log_lines(path: Path) -> list[str]:
    with path.open("r", encoding="utf-8", errors="replace") as f:
        return [line.rstrip("\r\n") for line in f]


def to_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def to_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def paeth_predictor(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png_rgb_data(path: Path) -> tuple[int, int, bytes]:
    raw = path.read_bytes()
    signature = b"\x89PNG\r\n\x1a\n"
    if not raw.startswith(signature):
        raise ValueError(f"Unsupported PNG signature in {path}")

    width = 0
    height = 0
    bit_depth = 0
    color_type = 0
    compressed_chunks = bytearray()
    pos = len(signature)

    while pos + 8 <= len(raw):
        length = struct.unpack(">I", raw[pos:pos + 4])[0]
        chunk_type = raw[pos + 4:pos + 8]
        chunk_data_start = pos + 8
        chunk_data_end = chunk_data_start + length
        chunk_crc_end = chunk_data_end + 4
        if chunk_crc_end > len(raw):
            raise ValueError(f"Truncated PNG chunk in {path}")
        chunk_data = raw[chunk_data_start:chunk_data_end]

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", chunk_data)
            if bit_depth != 8 or color_type != 2:
                raise ValueError(f"Unsupported PNG format in {path}: bit_depth={bit_depth}, color_type={color_type}")
            if compression != 0 or filter_method != 0 or interlace != 0:
                raise ValueError(f"Unsupported PNG compression/filter/interlace settings in {path}")
        elif chunk_type == b"IDAT":
            compressed_chunks.extend(chunk_data)
        elif chunk_type == b"IEND":
            break

        pos = chunk_crc_end

    if width <= 0 or height <= 0:
        raise ValueError(f"PNG missing valid IHDR in {path}")

    decompressed = zlib.decompress(bytes(compressed_chunks))
    bpp = 3
    stride = width * bpp
    expected_size = (stride + 1) * height
    if len(decompressed) != expected_size:
        raise ValueError(f"Unexpected PNG payload size in {path}")

    output = bytearray(width * height * bpp)
    src_pos = 0
    dst_pos = 0
    prev_row = bytearray(stride)

    for _ in range(height):
        filter_type = decompressed[src_pos]
        src_pos += 1
        row = bytearray(decompressed[src_pos:src_pos + stride])
        src_pos += stride

        if filter_type == 0:
            pass
        elif filter_type == 1:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + left) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                row[i] = (row[i] + prev_row[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                up = prev_row[i]
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                up = prev_row[i]
                up_left = prev_row[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + paeth_predictor(left, up, up_left)) & 0xFF
        else:
            raise ValueError(f"Unsupported PNG filter type {filter_type} in {path}")

        output[dst_pos:dst_pos + stride] = row
        dst_pos += stride
        prev_row = row

    return width, height, bytes(output)


def read_png_stats(path: Path) -> dict[str, Any]:
    width, height, data = read_png_rgb_data(path)

    total_pixels = width * height
    brightness_sum = 0
    dark_pixels = 0
    bright_pixels = 0
    solid_runs = 0

    previous_pixel = None
    for offset in range(0, len(data), 3):
        pixel = data[offset:offset + 3]
        r = pixel[0]
        g = pixel[1]
        b = pixel[2]
        brightness = (r + g + b) / 3.0
        brightness_sum += brightness

        if brightness <= 12:
            dark_pixels += 1
        if brightness >= 243:
            bright_pixels += 1
        if previous_pixel == pixel:
            solid_runs += 1
        previous_pixel = pixel

    return {
        "width": width,
        "height": height,
        "avgBrightness": round(brightness_sum / total_pixels, 4),
        "darkPixelRatio": round(dark_pixels / total_pixels, 6),
        "brightPixelRatio": round(bright_pixels / total_pixels, 6),
        "adjacentRepeatRatio": round(solid_runs / max(1, total_pixels - 1), 6),
    }


def classify_log_lines(lines: list[str]) -> dict[str, Any]:
    error_lines: list[str] = []
    warning_lines: list[str] = []

    for line in lines:
        lowered = line.lower()
        if any(token in lowered for token in ("error", "fatal", "exception", "assert", "fail")):
            error_lines.append(line)
        elif any(token in lowered for token in ("warn", "unknown", "dead-end", "stub")):
            warning_lines.append(line)

    return {
        "lineCount": len(lines),
        "errorCount": len(error_lines),
        "warningCount": len(warning_lines),
        "sampleErrors": error_lines[:10],
        "sampleWarnings": warning_lines[:10],
    }


def analyze_inputs(events: list[dict[str, Any]]) -> dict[str, Any]:
    invalid_startup_events: list[dict[str, Any]] = []
    startup_allowed = {"A", "B", "T"}

    for event in events:
        emu_seconds = to_float(event.get("emuSeconds"))
        buttons = str(event.get("buttons", ""))
        if emu_seconds > 30.0:
            continue
        buttons_set = {ch for ch in buttons if ch != "-"}
        if not buttons_set.issubset(startup_allowed):
            invalid_startup_events.append({
                "frame": to_int(event.get("frame")),
                "emuSeconds": emu_seconds,
                "buttons": buttons,
            })

    return {
        "eventCount": len(events),
        "startupConstraintPassed": not invalid_startup_events,
        "invalidStartupSamples": invalid_startup_events[:10],
    }


def analyze_shots(healthcheck_dir: Path, shots: list[dict[str, Any]]) -> dict[str, Any]:
    duplicate_run = 1
    max_duplicate_run = 1
    duplicate_pairs = 0
    previous_hash = None

    unique_colors_values: list[int] = []
    changed_pixels_values: list[int] = []
    brightness_values: list[float] = []
    dark_shots = 0
    near_blank_shots = 0
    missing_files: list[str] = []
    first_meaningful_frame_seconds = None
    first_meaningful_frame_file = ""
    early_meaningful_shots = 0
    ocr_candidates: list[str] = []

    for shot in shots:
        shot_file = Path(str(shot.get("file", "")))
        shot_path = healthcheck_dir / shot_file
        exists = shot_path.is_file()
        unique_colors = to_int(shot.get("uniqueColors"))
        changed_pixels = to_int(shot.get("changedPixelsSinceLastShot"))
        emu_seconds = to_float(shot.get("emuSeconds"))
        current_hash = str(shot.get("hashFnv64", ""))

        unique_colors_values.append(unique_colors)
        changed_pixels_values.append(changed_pixels)

        if previous_hash is not None and current_hash == previous_hash:
            duplicate_run += 1
            duplicate_pairs += 1
        else:
            duplicate_run = 1
        max_duplicate_run = max(max_duplicate_run, duplicate_run)
        previous_hash = current_hash

        if not exists:
            missing_files.append(shot_file.as_posix())
            continue

        image_stats = read_png_stats(shot_path)
        brightness_values.append(to_float(image_stats.get("avgBrightness")))

        is_dark = image_stats["darkPixelRatio"] >= 0.98
        is_blank = unique_colors <= 2 or (
            image_stats["adjacentRepeatRatio"] >= 0.995 and unique_colors <= 4
        )

        if is_dark:
            dark_shots += 1
        if is_blank:
            near_blank_shots += 1

        is_meaningful = not is_dark and not is_blank and unique_colors >= 6
        if is_meaningful:
            if first_meaningful_frame_seconds is None:
                first_meaningful_frame_seconds = emu_seconds
                first_meaningful_frame_file = shot_file.as_posix()
            if emu_seconds <= 30.0:
                early_meaningful_shots += 1
            if len(ocr_candidates) < 4:
                ocr_candidates.append(shot_file.as_posix())

    avg_unique_colors = sum(unique_colors_values) / len(unique_colors_values) if unique_colors_values else 0.0
    avg_changed_pixels = sum(changed_pixels_values) / len(changed_pixels_values) if changed_pixels_values else 0.0
    avg_brightness = sum(brightness_values) / len(brightness_values) if brightness_values else 0.0
    last_changed_pixels = changed_pixels_values[-1] if changed_pixels_values else 0

    return {
        "shotCount": len(shots),
        "missingFiles": missing_files,
        "maxDuplicateRun": max_duplicate_run,
        "duplicatePairCount": duplicate_pairs,
        "averageUniqueColors": round(avg_unique_colors, 4),
        "averageChangedPixels": round(avg_changed_pixels, 4),
        "averageBrightness": round(avg_brightness, 4),
        "darkShotCount": dark_shots,
        "nearBlankShotCount": near_blank_shots,
        "firstMeaningfulFrameSeconds": first_meaningful_frame_seconds,
        "firstMeaningfulFrameFile": first_meaningful_frame_file,
        "earlyMeaningfulShotCount": early_meaningful_shots,
        "lastChangedPixels": last_changed_pixels,
        "ocrCandidates": ocr_candidates,
    }


def run_tesseract_ocr(image_path: Path) -> str:
    with tempfile.TemporaryDirectory(prefix="geranes_ocr_") as temp_dir:
        output_base = Path(temp_dir) / "ocr_result"
        completed = subprocess.run(
            ["tesseract", str(image_path), str(output_base), "--psm", "6"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if completed.returncode != 0:
            stderr = (completed.stderr or "").strip()
            raise RuntimeError(stderr or "tesseract failed")
        txt_path = output_base.with_suffix(".txt")
        if not txt_path.is_file():
            return ""
        return txt_path.read_text(encoding="utf-8", errors="replace")


def analyze_text_presence(healthcheck_dir: Path, shot_analysis: dict[str, Any]) -> dict[str, Any]:
    candidates = shot_analysis.get("ocrCandidates", [])
    if not isinstance(candidates, list):
        candidates = []

    tesseract_path = shutil.which("tesseract")
    if not tesseract_path:
        return {
            "available": False,
            "passed": True,
            "skipped": True,
            "message": "OCR engine not available. Text validation was skipped.",
            "matchedWords": [],
            "rawTokenCount": 0,
            "analyzedFiles": [],
        }

    matched_words: set[str] = set()
    analyzed_files: list[str] = []
    raw_token_count = 0

    for rel_path in candidates:
        image_path = healthcheck_dir / rel_path
        if not image_path.is_file():
            continue

        analyzed_files.append(rel_path)
        try:
            ocr_text = run_tesseract_ocr(image_path)
        except Exception:
            continue

        tokens = re.findall(r"[A-Z0-9]{3,}", ocr_text.upper())
        raw_token_count += len(tokens)
        for token in tokens:
            if token in COMMON_OCR_WORDS or re.fullmatch(r"[A-Z]{4,}", token):
                matched_words.add(token)

    matched_words_list = sorted(matched_words)
    passed = len(matched_words_list) > 0 or raw_token_count >= 3
    if passed:
        message = "Detected plausible OCR text in gameplay or menu screenshots."
    else:
        message = "Did not detect plausible OCR text in the sampled screenshots."

    return {
        "available": True,
        "passed": passed,
        "skipped": False,
        "message": message,
        "matchedWords": matched_words_list[:20],
        "rawTokenCount": raw_token_count,
        "analyzedFiles": analyzed_files,
    }


def make_check(
    check_id: str,
    label: str,
    passed: bool,
    message: str,
    severity: str = "error",
    details: dict[str, Any] | None = None,
) -> dict[str, Any]:
    return {
        "id": check_id,
        "label": label,
        "passed": passed,
        "severity": severity,
        "message": message,
        "details": details or {},
    }


def build_checks(
    run_data: dict[str, Any],
    input_analysis: dict[str, Any],
    shot_analysis: dict[str, Any],
    log_analysis: dict[str, Any],
    text_analysis: dict[str, Any],
) -> tuple[list[dict[str, Any]], str]:
    checks: list[dict[str, Any]] = []
    shot_count = to_int(shot_analysis.get("shotCount"))
    max_duplicate_run = to_int(shot_analysis.get("maxDuplicateRun"))
    average_changed_pixels = to_float(shot_analysis.get("averageChangedPixels"))
    average_unique_colors = to_float(shot_analysis.get("averageUniqueColors"))
    dark_shot_count = to_int(shot_analysis.get("darkShotCount"))
    near_blank_shot_count = to_int(shot_analysis.get("nearBlankShotCount"))
    early_meaningful_shots = to_int(shot_analysis.get("earlyMeaningfulShotCount"))
    first_meaningful_frame_seconds = shot_analysis.get("firstMeaningfulFrameSeconds")
    last_changed_pixels = to_int(shot_analysis.get("lastChangedPixels"))
    missing_files = shot_analysis.get("missingFiles", [])
    if not isinstance(missing_files, list):
        missing_files = []

    boot_ok = (
        shot_count > 0
        and first_meaningful_frame_seconds is not None
        and first_meaningful_frame_seconds <= 30.0
    )
    checks.append(make_check(
        "boot_ok",
        "Boot succeeded",
        boot_ok,
        "The ROM left the blank or dark startup state and showed plausible visual content within the first 30 seconds."
        if boot_ok else
        "The ROM did not show clearly useful visual content within the first 30 seconds.",
        "error",
        {
            "firstMeaningfulFrameSeconds": first_meaningful_frame_seconds,
            "earlyMeaningfulShotCount": early_meaningful_shots,
        },
    ))

    startup_inputs_ok = bool(input_analysis.get("startupConstraintPassed", False))
    checks.append(make_check(
        "startup_input_constraint",
        "Startup input constraint",
        startup_inputs_ok,
        "The first 30 seconds used only Start, A and B."
        if startup_inputs_ok else
        "Buttons outside Start/A/B were detected during the first 30 seconds.",
        "error",
        {
            "invalidStartupSamples": input_analysis.get("invalidStartupSamples", []),
        },
    ))

    screenshots_present = not missing_files
    checks.append(make_check(
        "screenshot_files_present",
        "Screenshot files present",
        screenshots_present,
        "All referenced screenshot files exist."
        if screenshots_present else
        "Some screenshots referenced by the healthcheck are missing on disk.",
        "error",
        {
            "missingFiles": missing_files,
        },
    ))

    screen_consistency_ok = shot_count == 0 or max_duplicate_run < 4
    checks.append(make_check(
        "screen_consistency",
        "Screen consistency",
        screen_consistency_ok,
        "There was no excessively long run of identical screenshots."
        if screen_consistency_ok else
        "Many consecutive screenshots were identical, suggesting a freeze or visual loop.",
        "warning",
        {
            "maxDuplicateRun": max_duplicate_run,
        },
    ))

    dark_frames_ok = shot_count == 0 or dark_shot_count < max(2, math.ceil(shot_count * 0.6))
    checks.append(make_check(
        "mostly_dark_frames",
        "Mostly dark frames",
        dark_frames_ok,
        "Most screenshots are not excessively dark."
        if dark_frames_ok else
        "Too many screenshots are excessively dark.",
        "warning",
        {
            "darkShotCount": dark_shot_count,
            "shotCount": shot_count,
        },
    ))

    blank_frames_ok = shot_count == 0 or near_blank_shot_count < max(2, math.ceil(shot_count * 0.6))
    checks.append(make_check(
        "near_blank_frames",
        "Near blank frames",
        blank_frames_ok,
        "Most screenshots contain enough visual information."
        if blank_frames_ok else
        "Too many screenshots look almost blank or extremely low-detail.",
        "warning",
        {
            "nearBlankShotCount": near_blank_shot_count,
            "shotCount": shot_count,
        },
    ))

    visual_activity_ok = shot_count <= 1 or average_changed_pixels >= 128.0
    checks.append(make_check(
        "visual_activity",
        "Visual activity",
        visual_activity_ok,
        "The run showed enough visual variation between screenshots."
        if visual_activity_ok else
        "Visual variation between screenshots was too low.",
        "warning",
        {
            "averageChangedPixels": average_changed_pixels,
            "lastChangedPixels": last_changed_pixels,
        },
    ))

    color_variety_ok = shot_count == 0 or average_unique_colors > 4.0
    checks.append(make_check(
        "color_variety",
        "Color variety",
        color_variety_ok,
        "The run showed reasonable color variety."
        if color_variety_ok else
        "Average color variety was too low.",
        "warning",
        {
            "averageUniqueColors": average_unique_colors,
        },
    ))

    text_present_ok = bool(text_analysis.get("passed", False))
    text_severity = "warning"
    checks.append(make_check(
        "valid_text_detected",
        "Valid text detected",
        text_present_ok,
        str(text_analysis.get("message", "Text detection was not executed.")),
        text_severity,
        {
            "available": text_analysis.get("available", False),
            "skipped": text_analysis.get("skipped", False),
            "matchedWords": text_analysis.get("matchedWords", []),
            "rawTokenCount": text_analysis.get("rawTokenCount", 0),
            "analyzedFiles": text_analysis.get("analyzedFiles", []),
        },
    ))

    log_errors_ok = to_int(log_analysis.get("errorCount")) == 0
    checks.append(make_check(
        "log_errors",
        "Error logs",
        log_errors_ok,
        "No errors were detected in the log."
        if log_errors_ok else
        "Error lines were detected in the log.",
        "error",
        {
            "errorCount": log_analysis.get("errorCount", 0),
            "sampleErrors": log_analysis.get("sampleErrors", []),
        },
    ))

    log_warnings_ok = to_int(log_analysis.get("warningCount")) == 0
    checks.append(make_check(
        "log_warnings",
        "Warning logs",
        log_warnings_ok,
        "No warnings were detected in the log."
        if log_warnings_ok else
        "Warning lines were detected in the log.",
        "warning",
        {
            "warningCount": log_analysis.get("warningCount", 0),
            "sampleWarnings": log_analysis.get("sampleWarnings", []),
        },
    ))

    total_frames_ok = to_int(run_data.get("totalFrames")) > 0
    checks.append(make_check(
        "total_frames_valid",
        "Valid total frames",
        total_frames_ok,
        "The healthcheck reports a valid total frame count."
        if total_frames_ok else
        "The healthcheck reports zero frames.",
        "error",
        {
            "totalFrames": run_data.get("totalFrames", 0),
        },
    ))

    failed_error_checks = sum(1 for check in checks if not check["passed"] and check["severity"] == "error")
    failed_warning_checks = sum(1 for check in checks if not check["passed"] and check["severity"] == "warning")

    if failed_error_checks > 0:
        verdict = "bad"
    elif failed_warning_checks > 0:
        verdict = "suspicious"
    else:
        verdict = "good"

    return checks, verdict


def analyze_healthcheck(healthcheck_dir: Path) -> dict[str, Any]:
    run_path = healthcheck_dir / "run.json"
    metrics_path = healthcheck_dir / "metrics.csv"
    events_path = healthcheck_dir / "events.jsonl"
    log_path = healthcheck_dir / "log.txt"

    missing_required = [
        path.name
        for path in (run_path, metrics_path, events_path, log_path)
        if not path.is_file()
    ]
    if missing_required:
        raise FileNotFoundError(
            f"Missing required healthcheck file(s): {', '.join(missing_required)}"
        )

    run_data = load_json(run_path)
    metrics_rows = load_csv(metrics_path)
    events = load_jsonl(events_path)
    log_lines = load_log_lines(log_path)

    shots = run_data.get("shots", [])
    if not isinstance(shots, list):
        raise ValueError("run.json has an invalid 'shots' field.")

    input_analysis = analyze_inputs(events)
    shot_analysis = analyze_shots(healthcheck_dir, shots)
    log_analysis = classify_log_lines(log_lines)
    text_analysis = analyze_text_presence(healthcheck_dir, shot_analysis)
    checks, verdict = build_checks(run_data, input_analysis, shot_analysis, log_analysis, text_analysis)
    hard_failures = [check["id"] for check in checks if not check["passed"] and check["severity"] == "error"]
    soft_failures = [check["id"] for check in checks if not check["passed"] and check["severity"] == "warning"]

    return {
        "schemaVersion": 1,
        "generatedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "healthcheckDir": str(healthcheck_dir.resolve()),
        "romPath": str(run_data.get("romPath", "")),
        "emulatorVersion": str(run_data.get("emulatorVersion", "unknown")),
        "finalVerdict": verdict,
        "decision": {
            "hardChecksPassed": len(hard_failures) == 0,
            "hardCheckFailureCount": len(hard_failures),
            "softCheckFailureCount": len(soft_failures),
            "hardCheckFailures": hard_failures,
            "softCheckFailures": soft_failures,
        },
        "summary": {
            "simSeconds": to_int(run_data.get("simSeconds")),
            "fps": to_int(run_data.get("fps")),
            "totalFrames": to_int(run_data.get("totalFrames")),
            "screenshotIntervalSeconds": to_int(run_data.get("screenshotIntervalSeconds")),
            "seed": to_int(run_data.get("seed")),
            "shotCount": to_int(shot_analysis.get("shotCount")),
            "metricRowCount": len(metrics_rows),
            "eventCount": to_int(input_analysis.get("eventCount")),
            "logLineCount": to_int(log_analysis.get("lineCount")),
        },
        "analysis": {
            "boot": {
                "firstMeaningfulFrameSeconds": shot_analysis.get("firstMeaningfulFrameSeconds"),
                "earlyMeaningfulShotCount": shot_analysis.get("earlyMeaningfulShotCount"),
            },
            "screens": {
                "shotCount": to_int(shot_analysis.get("shotCount")),
                "maxDuplicateRun": to_int(shot_analysis.get("maxDuplicateRun")),
                "averageChangedPixels": to_float(shot_analysis.get("averageChangedPixels")),
                "averageUniqueColors": to_float(shot_analysis.get("averageUniqueColors")),
                "averageBrightness": to_float(shot_analysis.get("averageBrightness")),
                "darkShotCount": to_int(shot_analysis.get("darkShotCount")),
                "nearBlankShotCount": to_int(shot_analysis.get("nearBlankShotCount")),
                "missingFiles": shot_analysis.get("missingFiles", []),
            },
            "inputs": input_analysis,
            "logs": log_analysis,
            "text": text_analysis,
        },
        "validations": checks,
    }


def main() -> int:
    args = parse_args()
    healthcheck_dir = Path(args.healthcheck_dir).resolve()
    if not healthcheck_dir.is_dir():
        print("healthcheck_dir is not a directory.", file=sys.stderr)
        return 2

    try:
        report = analyze_healthcheck(healthcheck_dir)
    except Exception as exc:
        print(f"failed to analyze healthcheck: {exc}", file=sys.stderr)
        return 2

    report_path = healthcheck_dir / REPORT_FILE_NAME
    with report_path.open("w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)

    print(report_path)
    print(f"Verdict: {report['finalVerdict']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
