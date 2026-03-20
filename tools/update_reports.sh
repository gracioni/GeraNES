#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
GENERATED_DIR="$SCRIPT_DIR/generated"

mkdir -p "$GENERATED_DIR"

python "$SCRIPT_DIR/generate_mapper_coverage.py" \
  --out "$GENERATED_DIR/mapper_coverage.html"

python "$SCRIPT_DIR/run_geranes_tests.py" \
  --html \
  --expect-config "$SCRIPT_DIR/test_pass_expectations.json" \
  --out "$GENERATED_DIR/geranes_test_report.html" \
  "$SCRIPT_DIR/../build" \
  "$SCRIPT_DIR/../tests/roms"
