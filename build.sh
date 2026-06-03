#!/bin/sh

set -eu

usage() {
    cat <<EOF
Usage: $0 <linux|mingw|emscripten> [target] [build_type] [jobs] [build_dir] [deploy_dir]

Examples:
  $0 linux
  $0 mingw GeraNES
  $0 emscripten GeraNES Release 16
  $0 mingw GeraNES Debug 8 out/build-mingw out/deploy-mingw
EOF
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

copy_if_exists() {
    src="$1"
    dst="$2"
    if [ -e "$src" ]; then
        cp "$src" "$dst/"
    fi
}

deploy_desktop() {
    mkdir -p "$DEPLOY_DIR"

    if [ -d "$BUILD_DIR/generated/runtime_data" ]; then
        cp -R "$BUILD_DIR/generated/runtime_data/." "$DEPLOY_DIR/"
    fi

    copy_if_exists "$BUILD_DIR/$OUTPUT_NAME" "$DEPLOY_DIR"

    find "$BUILD_DIR" -maxdepth 1 -type f \( -name '*.dll' -o -name '*.so' -o -name '*.dylib' \) \
        -exec cp {} "$DEPLOY_DIR/" \;
}

deploy_emscripten() {
    mkdir -p "$DEPLOY_DIR"

    copy_if_exists "$BUILD_DIR/${TARGET}.html" "$DEPLOY_DIR"
    copy_if_exists "$BUILD_DIR/${TARGET}.js" "$DEPLOY_DIR"
    copy_if_exists "$BUILD_DIR/${TARGET}.wasm" "$DEPLOY_DIR"
    copy_if_exists "$BUILD_DIR/${TARGET}.data" "$DEPLOY_DIR"

    if [ -d "$BUILD_DIR/docs" ]; then
        cp -R "$BUILD_DIR/docs" "$DEPLOY_DIR/"
    fi
}

PLATFORM="${1:-}"
TARGET="${2:-GeraNES}"
BUILD_TYPE="${3:-Release}"
JOBS="${4:-16}"
ROOT_DIR=$PWD

if [ -z "$PLATFORM" ]; then
    usage
    exit 1
fi

case "$PLATFORM" in
    linux)
        GENERATOR_ARGS=""
        PLATFORM_BUILD_DIR="build-linux"
        PLATFORM_DEPLOY_DIR="deploy-linux"
        OUTPUT_NAME="$TARGET"
        CONFIGURE_CMD="cmake"
        ;;
    mingw)
        GENERATOR_ARGS='-G "MinGW Makefiles"'
        PLATFORM_BUILD_DIR="build-mingw"
        PLATFORM_DEPLOY_DIR="deploy-mingw"
        OUTPUT_NAME="${TARGET}.exe"
        CONFIGURE_CMD="cmake"
        ;;
    emscripten)
        GENERATOR_ARGS=""
        PLATFORM_BUILD_DIR="build-emscripten"
        PLATFORM_DEPLOY_DIR="deploy-emscripten"
        OUTPUT_NAME="${TARGET}.html"
        CONFIGURE_CMD="emcmake cmake"
        ;;
    *)
        usage
        exit 1
        ;;
esac

BUILD_DIR="${5:-$PLATFORM_BUILD_DIR}"
DEPLOY_DIR="${6:-$PLATFORM_DEPLOY_DIR}"

require_command cmake
if [ "$PLATFORM" = "emscripten" ]; then
    require_command emcmake
fi

rm -rf "$DEPLOY_DIR"
mkdir -p "$BUILD_DIR"

cd "$ROOT_DIR"
eval "$CONFIGURE_CMD -S . -B \"$BUILD_DIR\" $GENERATOR_ARGS -DCMAKE_BUILD_TYPE=\"$BUILD_TYPE\""
cmake --build "$BUILD_DIR" --target "$TARGET" -j "$JOBS"

rm -rf "$DEPLOY_DIR"

case "$PLATFORM" in
    linux|mingw)
        deploy_desktop
        ;;
    emscripten)
        deploy_emscripten
        ;;
esac

printf 'Build complete for %s\n' "$PLATFORM"
printf 'Build dir: %s/%s\n' "$ROOT_DIR" "$BUILD_DIR"
printf 'Deploy dir: %s/%s\n' "$ROOT_DIR" "$DEPLOY_DIR"
