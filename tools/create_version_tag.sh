#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
DEFINES_H="$ROOT_DIR/src/GeraNES/defines.h"

if [ ! -f "$DEFINES_H" ]; then
    echo "error: version file not found: $DEFINES_H" >&2
    exit 1
fi

VERSION=$(sed -n 's/.*GERANES_VERSION = "\([^"]*\)".*/\1/p' "$DEFINES_H" | head -n 1)

if [ -z "$VERSION" ]; then
    echo "error: could not extract GERANES_VERSION from $DEFINES_H" >&2
    exit 1
fi

TAG="v$VERSION"

if git -C "$ROOT_DIR" rev-parse "$TAG" >/dev/null 2>&1; then
    echo "error: tag already exists: $TAG" >&2
    exit 1
fi

echo "creating annotated tag: $TAG"
git -C "$ROOT_DIR" tag -a "$TAG" -m "GeraNES $TAG"
echo "tag created: $TAG"
echo "push with: git push origin $TAG"
