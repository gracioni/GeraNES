#!/usr/bin/env python3
import argparse
import datetime as dt
import html
import json
import os
import subprocess
import sys
from typing import List, Dict


ROM_EXTENSIONS = {".nes", ".fds", ".unf", ".unif", ".nsf"}


def resolve_binary(bin_folder: str) -> str:
    candidates = [
        os.path.join(bin_folder, "GeraNES"),
        os.path.join(bin_folder, "GeraNES.exe"),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    raise FileNotFoundError(
        "GeraNES binary not found in bin-folder. Expected GeraNES or GeraNES.exe."
    )


def run_command(args: List[str]) -> subprocess.CompletedProcess:
    return subprocess.run(
        args,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def discover_roms(roms_folder: str) -> List[str]:
    roms: List[str] = []
    for root, _, files in os.walk(roms_folder):
        for name in files:
            ext = os.path.splitext(name)[1].lower()
            if ext in ROM_EXTENSIONS:
                roms.append(os.path.join(root, name))
    roms.sort()
    return roms


def to_report_path(rom_path: str, roms_folder: str) -> str:
    rel_path = os.path.relpath(rom_path, roms_folder)
    return rel_path.replace("\\", "/")


def combine_output(stdout: str, stderr: str) -> str:
    if stdout and stderr:
        sep = "" if stdout.endswith("\n") else "\n"
        return f"{stdout}{sep}{stderr}"
    return stdout or stderr or ""


def normalize_return_code(code: int) -> int:
    if code == 4294967295:
        return -1
    return code


def load_expect_config(path: str) -> Dict[str, List[str]]:
    if not path:
        return {}

    with open(path, "r", encoding="utf-8") as f:
        raw = json.load(f)

    mapping: Dict[str, List[str]] = {}

    if isinstance(raw, dict):
        # Preferred shape:
        # {"passTextByRom":{"rom.nes":"TEXT or [TEXT,...]"}}
        source = raw.get("passTextByRom", raw)
        if isinstance(source, dict):
            for key, value in source.items():
                if not isinstance(key, str):
                    continue
                rom_key = key.strip().lower()
                if not rom_key:
                    continue

                if isinstance(value, str):
                    text = value.strip()
                    if text:
                        mapping[rom_key] = [text]
                elif isinstance(value, list):
                    texts = [str(v).strip() for v in value if str(v).strip()]
                    if texts:
                        mapping[rom_key] = texts

    return mapping


def run_tests(binary: str, roms_folder: str, expect_map: Dict[str, List[str]]) -> Dict[str, object]:
    version_proc = run_command([binary, "--version"])
    version_text = (version_proc.stdout or "").strip()
    if version_proc.returncode != 0:
        version_text = version_text or "unknown"

    tests: List[Dict[str, str]] = []
    roms = discover_roms(roms_folder)
    total = len(roms)

    print(f"Emulator version: {version_text}", flush=True)
    print(f"ROMs found: {total}", flush=True)
    if expect_map:
        print(f"Custom pass-text entries: {len(expect_map)}", flush=True)

    for index, rom_path in enumerate(roms, start=1):
        proc = run_command([binary, "--test", rom_path])
        return_code = normalize_return_code(proc.returncode)
        output = combine_output(proc.stdout, proc.stderr).strip()
        rom_name = os.path.basename(rom_path)
        report_path = to_report_path(rom_path, roms_folder)
        if return_code == 0:
            result = "passed"
        elif return_code == -1:
            result = "failed"
        else:
            result = "error"

        print(f"[{index}/{total}] {report_path} -> {result}", flush=True)

        tests.append(
            {
                "fileName": report_path,
                "result": result,
                "returnCode": return_code,
                "output": output,
            }
        )

    passed_count = sum(1 for test in tests if test.get("result") == "passed")
    total_count = len(tests)

    return {
        "emulatorVersion": version_text,
        "generatedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "summary": {
            "passed": passed_count,
            "total": total_count,
            "label": f"Passed ({passed_count}/{total_count})",
        },
        "tests": tests,
    }


def write_json(report: Dict[str, object], out_path: str) -> None:
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)


def write_md(report: Dict[str, object], out_path: str) -> None:
    version = str(report.get("emulatorVersion", "unknown"))
    generated_at = str(report.get("generatedAtUtc", ""))
    summary = report.get("summary", {})
    summary_label = str(summary.get("label", "Passed (0/0)")) if isinstance(summary, dict) else "Passed (0/0)"
    tests = report.get("tests", [])
    if not isinstance(tests, list):
        tests = []

    lines: List[str] = []
    lines.append("# GeraNES Test Report")
    lines.append("")
    lines.append(f"## {summary_label}")
    lines.append("")
    lines.append(f"- Emulator version: `{version}`")
    lines.append(f"- Generated at (UTC): `{generated_at}`")
    lines.append("")
    for test in tests:
        if not isinstance(test, dict):
            continue
        file_name = html.escape(str(test.get("fileName", "")))
        result_raw = str(test.get("result", "")).strip().lower()
        if result_raw == "passed":
            result = '<span style="color: green;"><strong>passed</strong></span>'
        elif result_raw == "error":
            result = '<span style="color: orange;"><strong>error</strong></span>'
        else:
            result = '<span style="color: red;"><strong>failed</strong></span>'
        lines.append(f"- **{file_name}**: {result}")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def write_html(report: Dict[str, object], out_path: str) -> None:
    version = html.escape(str(report.get("emulatorVersion", "unknown")))
    generated_at = html.escape(str(report.get("generatedAtUtc", "")))
    summary = report.get("summary", {})
    summary_label = html.escape(str(summary.get("label", "Passed (0/0)"))) if isinstance(summary, dict) else "Passed (0/0)"
    tests = report.get("tests", [])
    if not isinstance(tests, list):
        tests = []

    parts: List[str] = []
    parts.append("<!doctype html>")
    parts.append("<html lang=\"en\">")
    parts.append("<head>")
    parts.append("<meta charset=\"utf-8\">")
    parts.append("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">")
    parts.append("<title>GeraNES Test Report</title>")
    parts.append("<style>")
    parts.append("body{font-family:Consolas,Menlo,monospace;margin:24px;line-height:1.35;background:#fafafa;color:#111;}")
    parts.append("h1{margin:0 0 12px 0;font-size:24px;}")
    parts.append(".meta{margin:0 0 16px 0;color:#444;}")
    parts.append(".test{background:#fff;border:1px solid #ddd;border-radius:8px;padding:12px 14px;margin:0 0 10px 0;}")
    parts.append(".head{margin:0 0 6px 0;}")
    parts.append(".name{font-weight:700;}")
    parts.append(".status{font-weight:700;}")
    parts.append(".passed{color:#198754;}")
    parts.append(".failed{color:#c1121f;}")
    parts.append(".error{color:#d97706;}")
    parts.append("details{margin-top:6px;}")
    parts.append("summary{cursor:pointer;user-select:none;}")
    parts.append("pre{margin:8px 0 0 0;padding:10px;border:1px solid #e5e7eb;border-radius:6px;background:#f8fafc;overflow:auto;white-space:pre;}")
    parts.append("</style>")
    parts.append("</head>")
    parts.append("<body>")
    parts.append("<h1>GeraNES Test Report</h1>")
    parts.append(f"<h2>{summary_label}</h2>")
    parts.append(f"<p class=\"meta\">Emulator version: <strong>{version}</strong><br>Generated at (UTC): <strong>{generated_at}</strong></p>")

    for test in tests:
        if not isinstance(test, dict):
            continue
        file_name = html.escape(str(test.get("fileName", "")))
        result_raw = str(test.get("result", "")).strip().lower()
        if result_raw == "passed":
            status_class = "passed"
        elif result_raw == "error":
            status_class = "error"
        else:
            status_class = "failed"
        return_code = html.escape(str(test.get("returnCode", "")))
        output_raw = str(test.get("output", ""))
        output_norm = output_raw.replace("\r\n", "\n").replace("\r", "\n")
        output_html = html.escape(output_norm)

        parts.append("<section class=\"test\">")
        code_suffix = ""
        if result_raw != "passed":
            code_suffix = f" (code: {return_code})"
        parts.append(
            f"<p class=\"head\"><span class=\"name\">{file_name}</span>: "
            f"<span class=\"status {status_class}\">{html.escape(result_raw)}</span>"
            f"{code_suffix}</p>"
        )
        parts.append("<details>")
        parts.append("<summary>show output</summary>")
        parts.append(f"<pre>{output_html}</pre>")
        parts.append("</details>")
        parts.append("</section>")

    parts.append("</body>")
    parts.append("</html>")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(parts))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run GeraNES test ROMs recursively and generate a report."
    )
    parser.add_argument("bin_folder", help="Folder containing GeraNES binary")
    parser.add_argument("roms_folder", help="Folder with ROMs (recursive)")
    parser.add_argument(
        "--json",
        action="store_true",
        default=False,
        help="Generate JSON report (default)",
    )
    parser.add_argument(
        "--md",
        action="store_true",
        default=False,
        help="Generate Markdown report",
    )
    parser.add_argument(
        "--html",
        action="store_true",
        default=False,
        help="Generate HTML report",
    )
    parser.add_argument(
        "--out",
        default="",
        help="Output file path (optional). If omitted, uses default name by format.",
    )
    parser.add_argument(
        "--expect-config",
        default="",
        help=(
            "Optional JSON file mapping ROM basename to pass marker text. "
            "Example: {\"passTextByRom\":{\"dma_2007_read.nes\":\"96E2976E\"}}"
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    formats: List[str] = []
    if args.json:
        formats.append("json")
    if args.md:
        formats.append("md")
    if args.html:
        formats.append("html")
    if not formats:
        formats = ["json"]

    try:
        binary = resolve_binary(args.bin_folder)
    except FileNotFoundError as e:
        print(str(e), file=sys.stderr)
        return 2

    if not os.path.isdir(args.roms_folder):
        print("roms-folder is not a directory.", file=sys.stderr)
        return 2

    expect_map: Dict[str, List[str]] = {}
    if args.expect_config:
        if not os.path.isfile(args.expect_config):
            print("expect-config file not found.", file=sys.stderr)
            return 2
        try:
            expect_map = load_expect_config(args.expect_config)
        except Exception as e:
            print(f"failed to parse expect-config: {e}", file=sys.stderr)
            return 2

    report = run_tests(binary, args.roms_folder, expect_map)

    default_out = {
        "json": "geranes_test_report.json",
        "md": "geranes_test_report.md",
        "html": "geranes_test_report.html",
    }

    generated_files: List[str] = []
    for out_format in formats:
        if args.out:
            if len(formats) == 1:
                out_file = args.out
            else:
                base, _ = os.path.splitext(args.out)
                out_file = f"{base}.{out_format}"
        else:
            out_file = default_out[out_format]

        if out_format == "md":
            write_md(report, out_file)
        elif out_format == "html":
            write_html(report, out_file)
        else:
            write_json(report, out_file)

        generated_files.append(out_file)

    for out_file in generated_files:
        print(out_file, flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
