#!/usr/bin/env python3
# Compatibility wrapper. The consolidated healthcheck scripts now live under
# tools/healthcheck/, but this entrypoint is kept so older commands still work.
from __future__ import annotations

import runpy
from pathlib import Path


if __name__ == "__main__":
    script_path = Path(__file__).resolve().parent / "healthcheck" / "analyze_healthcheck.py"
    runpy.run_path(str(script_path), run_name="__main__")
