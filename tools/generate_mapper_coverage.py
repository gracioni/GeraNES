from __future__ import annotations

import argparse
import html
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAPPERS_DIR = ROOT / "src" / "GeraNES" / "Mappers"
DEFINES_PATH = ROOT / "src" / "GeraNES" / "defines.h"
OUTPUT_HTML_PATH = Path(__file__).resolve().parent / "mapper_coverage.html"
NESDEV_MAPPER_URL = "https://www.nesdev.org/wiki/Mapper"


def find_implemented_mappers() -> set[int]:
    implemented: set[int] = set()
    pattern = re.compile(r"^Mapper(\d{3})\.h$")

    for path in MAPPERS_DIR.glob("Mapper*.h"):
        match = pattern.match(path.name)
        if match is None:
            continue
        implemented.add(int(match.group(1)))

    return implemented


def read_geranes_version() -> str:
    text = DEFINES_PATH.read_text(encoding="utf-8")
    match = re.search(r'GERANES_VERSION\s*=\s*"([^"]+)"', text)
    if match is None:
        return "unknown"
    return match.group(1)


def build_html(implemented: set[int], version: str) -> str:
    cells: list[str] = []
    for mapper_id in range(256):
        is_implemented = mapper_id in implemented
        cell_class = "implemented" if is_implemented else "missing"
        title = f"Mapper {mapper_id:03d} - {'implemented' if is_implemented else 'not implemented'}"
        mapper_url = f"https://www.nesdev.org/wiki/INES_Mapper_{mapper_id:03d}"
        cells.append(
            f'<a class="cell {cell_class}" href="{mapper_url}" title="{html.escape(title)}">{mapper_id:03d}</a>'
        )

    implemented_list = ", ".join(f"{mapper_id:03d}" for mapper_id in sorted(implemented))

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>GeraNES v{html.escape(version)} mapper coverage</title>
  <style>
    :root {{
      --bg: #f6f4ee;
      --panel: #fffdf8;
      --text: #1f2937;
      --muted: #6b7280;
      --grid: #d6d3d1;
      --implemented-bg: #1f9d55;
      --implemented-border: #167c43;
      --missing-bg: #f3f4f6;
      --missing-border: #d1d5db;
      --missing-text: #6b7280;
    }}

    * {{
      box-sizing: border-box;
    }}

    body {{
      margin: 0;
      font-family: "Segoe UI", "Helvetica Neue", Helvetica, Arial, sans-serif;
      background:
        radial-gradient(circle at top left, #ffffff 0%, transparent 32%),
        linear-gradient(180deg, #f8fafc 0%, var(--bg) 100%);
      color: var(--text);
    }}

    .page {{
      max-width: 900px;
      margin: 0 auto;
      padding: 20px 14px 30px;
    }}

    .hero {{
      background: var(--panel);
      border: 1px solid #e7e5e4;
      border-radius: 16px;
      padding: 18px;
      box-shadow: 0 10px 35px rgba(15, 23, 42, 0.06);
    }}

    h1 {{
      margin: 0 0 8px;
      font-size: clamp(1.45rem, 2.5vw, 2.1rem);
      line-height: 1.05;
    }}

    p {{
      margin: 0;
      color: var(--muted);
      line-height: 1.5;
    }}

    .meta {{
      margin-top: 12px;
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
    }}

    .pill {{
      padding: 6px 10px;
      border-radius: 999px;
      background: #f3f4f6;
      color: var(--text);
      font-size: 0.85rem;
      border: 1px solid #e5e7eb;
    }}

    .legend {{
      margin-top: 16px;
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      align-items: center;
    }}

    .legend-item {{
      display: inline-flex;
      align-items: center;
      gap: 10px;
      color: var(--muted);
      font-size: 0.85rem;
    }}

    .legend-swatch {{
      width: 14px;
      height: 14px;
      border-radius: 4px;
      border: 1px solid transparent;
    }}

    .legend-swatch.implemented {{
      background: var(--implemented-bg);
      border-color: var(--implemented-border);
    }}

    .legend-swatch.missing {{
      background: var(--missing-bg);
      border-color: var(--missing-border);
    }}

    .grid {{
      margin-top: 16px;
      display: grid;
      grid-template-columns: repeat(16, minmax(0, 1fr));
      gap: 3px;
    }}

    .cell {{
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 25px;
      border-radius: 7px;
      font-weight: 700;
      letter-spacing: 0.01em;
      border: 1px solid var(--grid);
      user-select: none;
      font-size: clamp(0.54rem, 0.82vw, 0.7rem);
      text-decoration: none;
      line-height: 1;
      padding: 3px 0;
      transition: transform 0.08s ease, box-shadow 0.12s ease, border-color 0.12s ease;
    }}

    .cell:hover {{
      transform: translateY(-1px);
      box-shadow: 0 6px 16px rgba(15, 23, 42, 0.12);
    }}

    .cell.implemented {{
      background: var(--implemented-bg);
      border-color: var(--implemented-border);
      color: white;
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.15);
    }}

    .cell.missing {{
      background: var(--missing-bg);
      border-color: var(--missing-border);
      color: var(--missing-text);
    }}

    .footer {{
      margin-top: 18px;
      padding: 14px 16px;
      background: var(--panel);
      border: 1px solid #e7e5e4;
      border-radius: 14px;
      color: var(--muted);
      line-height: 1.5;
      font-size: 0.9rem;
      box-shadow: 0 10px 35px rgba(15, 23, 42, 0.05);
    }}

    code {{
      font-family: Consolas, "Courier New", monospace;
      background: #f3f4f6;
      padding: 0.15em 0.35em;
      border-radius: 6px;
      color: #111827;
    }}

    a {{
      color: #0f766e;
    }}

    @media (max-width: 720px) {{
      .page {{
        padding: 14px 8px 20px;
      }}

      .grid {{
        gap: 2px;
      }}

      .cell {{
        min-height: 21px;
        border-radius: 5px;
        font-size: 0.48rem;
      }}
    }}
  </style>
</head>
<body>
  <div class="page">
    <section class="hero">
      <h1>GeraNES v{html.escape(version)} mapper coverage</h1>
      <p>Visual coverage grid inspired by the NESdev iNES plane 0 mapper table.</p>
      <div class="meta">
        <div class="pill">Implemented: <strong>{len(implemented)}</strong></div>
        <div class="pill">Range: <strong>000-255</strong></div>
        <div class="pill"><a href="{NESDEV_MAPPER_URL}">NESdev reference</a></div>
      </div>
      <div class="legend">
        <div class="legend-item"><span class="legend-swatch implemented"></span> Implemented</div>
        <div class="legend-item"><span class="legend-swatch missing"></span> Not implemented</div>
      </div>
    </section>

    <section class="grid">
      {''.join(cells)}
    </section>

    <section class="footer">
      <div><strong>Implemented mapper IDs:</strong></div>
      <div>{html.escape(implemented_list)}</div>
      <div style="margin-top: 10px;">Generated from files matching <code>MapperNNN.h</code> in <code>src/GeraNES/Mappers</code>.</div>
    </section>
  </div>
</body>
</html>
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate the GeraNES mapper coverage HTML report."
    )
    parser.add_argument(
        "--out",
        default=str(OUTPUT_HTML_PATH),
        help="Output HTML file path. Defaults to tools/mapper_coverage.html",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    implemented = find_implemented_mappers()
    version = read_geranes_version()
    out_path.write_text(build_html(implemented, version), encoding="utf-8-sig")
    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
