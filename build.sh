#!/bin/sh

set -eu

usage() {
    cat <<EOF
Usage: $0 <linux|mingw|emscripten|android> [target] [build_type] [jobs] [build_dir] [deploy_dir]

Examples:
  $0 linux
  $0 mingw GeraNES
  $0 emscripten GeraNES Release 16
  $0 android
  $0 android Release 8
  $0 android main Release 8 build-android deploy-android
  $0 mingw GeraNES Debug 8 out/build-mingw out/deploy-mingw

Android config:
  Copy android/build-config.example.jsonc to android/build-config.jsonc
  Fill in SDK/NDK/JDK/package/signing fields there before building.
EOF
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

find_python_command() {
    if command -v python3 >/dev/null 2>&1; then
        command -v python3
        return 0
    fi
    if command -v python >/dev/null 2>&1; then
        command -v python
        return 0
    fi
    return 1
}

find_ninja_command() {
    if [ -n "${GERANES_ANDROID_CMAKE_DIR:-}" ]; then
        if [ -x "${GERANES_ANDROID_CMAKE_DIR}/bin/ninja" ]; then
            printf '%s\n' "${GERANES_ANDROID_CMAKE_DIR}/bin/ninja"
            return 0
        fi
        if [ -x "${GERANES_ANDROID_CMAKE_DIR}/bin/ninja.exe" ]; then
            printf '%s\n' "${GERANES_ANDROID_CMAKE_DIR}/bin/ninja.exe"
            return 0
        fi
    fi
    if command -v ninja >/dev/null 2>&1; then
        command -v ninja
        return 0
    fi
    return 1
}

copy_if_exists() {
    src="$1"
    dst="$2"
    if [ -e "$src" ]; then
        cp "$src" "$dst/"
    fi
}

normalize_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s\n' "$1" | sed 's#\\#/#g'
    fi
}

write_file() {
    file_path="$1"
    shift
    mkdir -p "$(dirname "$file_path")"
    printf '%s\n' "$@" > "$file_path"
}

load_android_config_file() {
    config_file="$1"
    python_bin=$(find_python_command || true)
    if [ -z "${python_bin:-}" ]; then
        printf 'Missing required command: python3 or python (needed for Android JSONC config parsing)\n' >&2
        exit 1
    fi

    eval "$("$python_bin" - "$config_file" <<'PY'
import json
import pathlib
import shlex
import sys


def strip_jsonc(source: str) -> str:
    out = []
    in_string = False
    string_delim = ""
    escape = False
    in_line_comment = False
    in_block_comment = False
    i = 0

    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""

        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
                out.append(ch)
            i += 1
            continue

        if in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
                i += 2
            else:
                i += 1
            continue

        if in_string:
            out.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == string_delim:
                in_string = False
            i += 1
            continue

        if ch in ('"', "'"):
            in_string = True
            string_delim = ch
            out.append(ch)
            i += 1
            continue

        if ch == "/" and nxt == "/":
            in_line_comment = True
            i += 2
            continue

        if ch == "/" and nxt == "*":
            in_block_comment = True
            i += 2
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def get_nested(data, path):
    current = data
    for key in path.split("."):
        if not isinstance(current, dict) or key not in current:
            return None
        current = current[key]
    return current


def to_shell_path(config_dir: pathlib.Path, value):
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return ""
    candidate = pathlib.Path(text).expanduser()
    if not candidate.is_absolute():
        candidate = (config_dir / candidate).resolve()
    return str(candidate)


config_path = pathlib.Path(sys.argv[1]).resolve()
raw_text = config_path.read_text(encoding="utf-8")
data = json.loads(strip_jsonc(raw_text))
if not isinstance(data, dict):
    raise SystemExit("Android config file must contain a JSON object at the root.")

config_dir = config_path.parent
mapping = {
    "sdkRoot": ("GERANES_ANDROID_SDK_ROOT", "path"),
    "ndkRoot": ("GERANES_ANDROID_NDK_ROOT", "path"),
    "javaHome": ("GERANES_ANDROID_JAVA_HOME", "path"),
    "cmakeDir": ("GERANES_ANDROID_CMAKE_DIR", "path"),
    "gradleBin": ("GERANES_ANDROID_GRADLE_BIN", "path"),
    "gradleVersion": ("GERANES_ANDROID_GRADLE_VERSION", "value"),
    "packageFormat": ("GERANES_ANDROID_PACKAGE_FORMAT", "value"),
    "api": ("GERANES_ANDROID_API", "value"),
    "stl": ("GERANES_ANDROID_STL", "value"),
    "appName": ("GERANES_ANDROID_APP_NAME", "value"),
    "iconPath": ("GERANES_ANDROID_ICON_PATH", "path"),
    "applicationId": ("GERANES_ANDROID_APPLICATION_ID", "value"),
    "namespace": ("GERANES_ANDROID_NAMESPACE", "value"),
    "versionCode": ("GERANES_ANDROID_VERSION_CODE", "value"),
    "versionName": ("GERANES_ANDROID_VERSION_NAME", "value"),
    "compileSdk": ("GERANES_ANDROID_COMPILE_SDK", "value"),
    "targetSdk": ("GERANES_ANDROID_TARGET_SDK", "value"),
    "signing.keystorePath": ("GERANES_ANDROID_KEYSTORE_PATH", "path"),
    "signing.keystorePassword": ("GERANES_ANDROID_KEYSTORE_PASSWORD", "value"),
    "signing.keyAlias": ("GERANES_ANDROID_KEY_ALIAS", "value"),
    "signing.keyPassword": ("GERANES_ANDROID_KEY_PASSWORD", "value"),
}

lines = []
for source_key, (env_name, value_kind) in mapping.items():
    value = get_nested(data, source_key)
    if value is None:
        continue
    if value_kind == "path":
        rendered = to_shell_path(config_dir, value)
    else:
        rendered = str(value)
    lines.append(f"{env_name}={shlex.quote(rendered)}")
    lines.append(f"export {env_name}")

abis_value = data.get("abis")
if abis_value is not None:
    if isinstance(abis_value, list):
        rendered_abis = ",".join(str(item).strip() for item in abis_value if str(item).strip())
    else:
        rendered_abis = str(abis_value).strip()
    lines.append(f"GERANES_ANDROID_ABI={shlex.quote(rendered_abis)}")
    lines.append("export GERANES_ANDROID_ABI")

print("\n".join(lines))
PY
)"
}

resolve_android_ndk_root() {
    if [ -n "${GERANES_ANDROID_NDK_ROOT:-}" ]; then
        printf '%s\n' "$GERANES_ANDROID_NDK_ROOT"
        return 0
    fi
    sdk_root="${GERANES_ANDROID_SDK_ROOT:-}"
    if [ -n "${sdk_root:-}" ] && [ -d "$sdk_root/ndk" ]; then
        ndk_dir=$(find "$sdk_root/ndk" -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1)
        if [ -n "${ndk_dir:-}" ]; then
            printf '%s\n' "$ndk_dir"
            return 0
        fi
    fi
    return 1
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

deploy_android() {
    mkdir -p "$DEPLOY_DIR"
    if [ -n "${ANDROID_PACKAGE_OUTPUT:-}" ] && [ -e "$ANDROID_PACKAGE_OUTPUT" ]; then
        cp "$ANDROID_PACKAGE_OUTPUT" "$DEPLOY_DIR/"
    fi
    if [ -n "${ANDROID_NATIVE_LIB_OUTPUT:-}" ] && [ -e "$ANDROID_NATIVE_LIB_OUTPUT" ]; then
        cp "$ANDROID_NATIVE_LIB_OUTPUT" "$DEPLOY_DIR/"
    fi
}

resolve_android_package_output() {
    package_dir="$1"
    package_extension="$2"
    preferred_path="$3"

    if [ -e "$preferred_path" ]; then
        printf '%s\n' "$preferred_path"
        return 0
    fi

    found_path=$(find "$package_dir" -maxdepth 1 -type f -name "*.${package_extension}" | sort | head -n 1)
    if [ -n "${found_path:-}" ]; then
        printf '%s\n' "$found_path"
        return 0
    fi

    return 1
}

download_file() {
    url="$1"
    out="$2"
    if command -v curl >/dev/null 2>&1; then
        curl -L --fail --output "$out" "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$out" "$url"
    else
        printf 'Missing required command: curl or wget\n' >&2
        exit 1
    fi
}

ensure_gradle() {
    if [ -n "${GERANES_ANDROID_GRADLE_BIN:-}" ]; then
        printf '%s\n' "$(normalize_path "$GERANES_ANDROID_GRADLE_BIN")"
        return 0
    fi
    if command -v gradle >/dev/null 2>&1; then
        command -v gradle
        return 0
    fi

    gradle_version="${GERANES_ANDROID_GRADLE_VERSION:-8.0.2}"
    gradle_cache_dir="$BUILD_DIR/.gradle-dist"
    gradle_home_dir="$gradle_cache_dir/gradle-$gradle_version"
    gradle_bin="$gradle_home_dir/bin/gradle"

    if [ ! -x "$gradle_bin" ]; then
        mkdir -p "$gradle_cache_dir"
        gradle_zip="$gradle_cache_dir/gradle-$gradle_version-bin.zip"
        download_file "https://services.gradle.org/distributions/gradle-$gradle_version-bin.zip" "$gradle_zip"
        rm -rf "$gradle_home_dir"
        if command -v unzip >/dev/null 2>&1; then
            unzip -q "$gradle_zip" -d "$gradle_cache_dir"
        else
            powershell -NoProfile -Command "Expand-Archive -LiteralPath '$gradle_zip' -DestinationPath '$gradle_cache_dir' -Force"
        fi
    fi

    if [ ! -x "$gradle_bin" ] && [ -f "$gradle_bin" ]; then
        chmod +x "$gradle_bin"
    fi

    printf '%s\n' "$(normalize_path "$ROOT_DIR/$gradle_bin")"
}

prepare_android_assets() {
    android_assets_dir="$1"
    runtime_data_stage_dir="$2"
    rm -rf "$android_assets_dir"
    mkdir -p "$android_assets_dir"

    if [ -d "$runtime_data_stage_dir" ]; then
        cp -R "$runtime_data_stage_dir/." "$android_assets_dir/"
        rm -f "$android_assets_dir/docs/sitemap.xml.gz"
        return 0
    fi

    cp -R "$ROOT_DIR/data/." "$android_assets_dir/"
    if [ -d "$ROOT_DIR/docs/user-guide/site" ]; then
        mkdir -p "$android_assets_dir/docs"
        cp -R "$ROOT_DIR/docs/user-guide/site/." "$android_assets_dir/docs/"
        rm -f "$android_assets_dir/docs/sitemap.xml.gz"
    fi
}

prepare_android_project() {
    android_project_dir="$1"
    android_assets_dir="$2"
    sdl_android_project_dir="$3"
    sdl_java_src_dir="$sdl_android_project_dir/app/src/main/java"
    sdl_res_dir="$sdl_android_project_dir/app/src/main/res"
    android_app_java_dir="$android_project_dir/app/src/main/java"
    android_app_res_dir="$android_project_dir/app/src/main/res"

    mkdir -p "$android_project_dir"
    cp -R "$ROOT_DIR/android/." "$android_project_dir/"
    mkdir -p "$android_app_java_dir" "$android_app_res_dir"
    cp -R "$sdl_java_src_dir/." "$android_app_java_dir/"
    cp -R "$sdl_res_dir/." "$android_app_res_dir/"

    android_project_dir_normalized=$(normalize_path "$android_project_dir")
    android_assets_dir_normalized=$(normalize_path "$ROOT_DIR/$android_assets_dir")
    repo_root_normalized=$(normalize_path "$ROOT_DIR")
    app_icon_resource="@android:drawable/sym_def_app_icon"
    keystore_path_normalized=""
    if [ -n "${GERANES_ANDROID_KEYSTORE_PATH:-}" ]; then
        keystore_path_normalized=$(normalize_path "$GERANES_ANDROID_KEYSTORE_PATH")
    fi
    if [ -n "${GERANES_ANDROID_ICON_PATH:-}" ]; then
        icon_source_path="${GERANES_ANDROID_ICON_PATH}"
        if [ ! -f "$icon_source_path" ]; then
            printf 'Configured Android icon file not found: %s\n' "$icon_source_path" >&2
            exit 1
        fi
        icon_extension=$(printf '%s' "${icon_source_path##*.}" | tr '[:upper:]' '[:lower:]')
        case "$icon_extension" in
            png|webp|jpg|jpeg)
                ;;
            *)
                printf 'Unsupported Android icon file extension: %s (expected png, webp, jpg, or jpeg)\n' "$icon_extension" >&2
                exit 1
                ;;
        esac
        icon_res_dir="$android_project_dir/app/src/main/res/drawable-nodpi"
        mkdir -p "$icon_res_dir"
        cp "$icon_source_path" "$icon_res_dir/geranes_app_icon.$icon_extension"
        app_icon_resource="@drawable/geranes_app_icon"
    fi

    write_file "$android_project_dir/local.properties" \
        "sdk.dir=$(normalize_path "$ANDROID_SDK_ROOT_RESOLVED")"

    if [ -n "${GERANES_ANDROID_CMAKE_DIR:-}" ]; then
        printf 'cmake.dir=%s\n' "$(normalize_path "$GERANES_ANDROID_CMAKE_DIR")" >> "$android_project_dir/local.properties"
    fi

    write_file "$android_project_dir/gradle.properties" \
        "org.gradle.jvmargs=-Xmx4096m -Dfile.encoding=UTF-8" \
        "android.useAndroidX=true" \
        "GERANES_REPO_ROOT=$repo_root_normalized" \
        "GERANES_ANDROID_ASSETS_DIR=$android_assets_dir_normalized" \
        "GERANES_ANDROID_NDK_PATH=$(normalize_path "$ANDROID_NDK_ROOT_RESOLVED")" \
        "GERANES_ANDROID_NAMESPACE=${GERANES_ANDROID_NAMESPACE:-com.racionisoft.geranes}" \
        "GERANES_ANDROID_APPLICATION_ID=${GERANES_ANDROID_APPLICATION_ID:-com.racionisoft.geranes}" \
        "GERANES_ANDROID_APP_NAME=${GERANES_ANDROID_APP_NAME:-GeraNES}" \
        "GERANES_ANDROID_APP_ICON=$app_icon_resource" \
        "GERANES_ANDROID_COMPILE_SDK=${GERANES_ANDROID_COMPILE_SDK:-34}" \
        "GERANES_ANDROID_TARGET_SDK=${GERANES_ANDROID_TARGET_SDK:-34}" \
        "GERANES_ANDROID_API=$ANDROID_API" \
        "GERANES_ANDROID_ABIS=$ANDROID_ABI" \
        "GERANES_ANDROID_STL=$ANDROID_STL" \
        "GERANES_ANDROID_VERSION_CODE=${GERANES_ANDROID_VERSION_CODE:-1}" \
        "GERANES_ANDROID_VERSION_NAME=${GERANES_ANDROID_VERSION_NAME:-1.0}" \
        "GERANES_ANDROID_KEYSTORE_PATH=$keystore_path_normalized" \
        "GERANES_ANDROID_KEYSTORE_PASSWORD=${GERANES_ANDROID_KEYSTORE_PASSWORD:-}" \
        "GERANES_ANDROID_KEY_ALIAS=${GERANES_ANDROID_KEY_ALIAS:-}" \
        "GERANES_ANDROID_KEY_PASSWORD=${GERANES_ANDROID_KEY_PASSWORD:-}"
}

PLATFORM="${1:-}"
TARGET="${2:-}"
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
        DEFAULT_TARGET="GeraNES"
        CONFIGURE_CMD="cmake"
        ;;
    mingw)
        GENERATOR_ARGS='-G "MinGW Makefiles"'
        PLATFORM_BUILD_DIR="build-mingw"
        PLATFORM_DEPLOY_DIR="deploy-mingw"
        DEFAULT_TARGET="GeraNES"
        CONFIGURE_CMD="cmake"
        ;;
    emscripten)
        GENERATOR_ARGS=""
        PLATFORM_BUILD_DIR="build-emscripten"
        PLATFORM_DEPLOY_DIR="deploy-emscripten"
        DEFAULT_TARGET="GeraNES"
        CONFIGURE_CMD="emcmake cmake"
        ;;
    android)
        GENERATOR_ARGS=""
        PLATFORM_BUILD_DIR="build-android"
        PLATFORM_DEPLOY_DIR="deploy-android"
        DEFAULT_TARGET="main"
        CONFIGURE_CMD="cmake"
        ;;
    *)
        usage
        exit 1
        ;;
esac

is_build_type() {
    case "$1" in
        Debug|Release|RelWithDebInfo|MinSizeRel)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

TARGET="${TARGET:-$DEFAULT_TARGET}"
if [ "$PLATFORM" = "android" ] && { [ -z "${2:-}" ] || is_build_type "${2:-}"; }; then
    TARGET="$DEFAULT_TARGET"
    BUILD_TYPE="${2:-Release}"
    JOBS="${3:-16}"
    BUILD_DIR="${4:-$PLATFORM_BUILD_DIR}"
    DEPLOY_DIR="${5:-$PLATFORM_DEPLOY_DIR}"
else
    BUILD_DIR="${5:-$PLATFORM_BUILD_DIR}"
    DEPLOY_DIR="${6:-$PLATFORM_DEPLOY_DIR}"
fi

case "$PLATFORM" in
    linux)
        OUTPUT_NAME="$TARGET"
        ;;
    mingw)
        OUTPUT_NAME="${TARGET}.exe"
        ;;
    emscripten)
        OUTPUT_NAME="${TARGET}.html"
        ;;
    android)
        OUTPUT_NAME="lib${TARGET}.so"
        ;;
esac

require_command cmake
if [ "$PLATFORM" = "emscripten" ]; then
    require_command emcmake
fi
if [ "$PLATFORM" = "android" ]; then
    ANDROID_CONFIG_FILE="$ROOT_DIR/android/build-config.jsonc"
    if [ ! -f "$ANDROID_CONFIG_FILE" ]; then
        printf 'Missing Android config file: %s\n' "$ANDROID_CONFIG_FILE" >&2
        printf 'Create it from android/build-config.example.jsonc before building for Android.\n' >&2
        exit 1
    fi
    printf 'Using Android config file: %s\n' "$ANDROID_CONFIG_FILE"
    load_android_config_file "$ANDROID_CONFIG_FILE"

    ANDROID_SDK_ROOT_RESOLVED="${GERANES_ANDROID_SDK_ROOT:-}"
    ANDROID_NDK_ROOT_RESOLVED=$(resolve_android_ndk_root || true)
    ANDROID_JAVA_HOME_RESOLVED="${GERANES_ANDROID_JAVA_HOME:-}"
    ANDROID_ABI="${GERANES_ANDROID_ABI:-arm64-v8a}"
    ANDROID_API="${GERANES_ANDROID_API:-24}"
    ANDROID_STL="${GERANES_ANDROID_STL:-c++_shared}"
    ANDROID_PACKAGE_FORMAT="${GERANES_ANDROID_PACKAGE_FORMAT:-apk}"
    ANDROID_PACKAGE_VARIANT=$(printf '%s' "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')
    ANDROID_PROJECT_DIR="$BUILD_DIR/android-project"
    ANDROID_ASSETS_DIR="$BUILD_DIR/android-assets"

    if [ -z "${ANDROID_NDK_ROOT_RESOLVED:-}" ]; then
        printf 'Missing Android NDK in android/build-config.jsonc. Fill "ndkRoot" or ensure "sdkRoot/ndk/*" exists.\n' >&2
        exit 1
    fi
    if [ -z "${ANDROID_SDK_ROOT_RESOLVED:-}" ]; then
        printf 'Missing Android SDK in android/build-config.jsonc. Fill "sdkRoot".\n' >&2
        exit 1
    fi
    case "$ANDROID_PACKAGE_FORMAT" in
        apk|aab)
            ;;
        *)
            printf 'Invalid GERANES_ANDROID_PACKAGE_FORMAT: %s (expected apk or aab)\n' "$ANDROID_PACKAGE_FORMAT" >&2
            exit 1
            ;;
    esac

    if [ -n "${ANDROID_JAVA_HOME_RESOLVED:-}" ]; then
        export JAVA_HOME="$ANDROID_JAVA_HOME_RESOLVED"
    fi
    if [ -n "${ANDROID_SDK_ROOT_RESOLVED:-}" ]; then
        export ANDROID_SDK_ROOT="$ANDROID_SDK_ROOT_RESOLVED"
        export ANDROID_HOME="$ANDROID_SDK_ROOT_RESOLVED"
    fi
    export ANDROID_NDK_ROOT="$ANDROID_NDK_ROOT_RESOLVED"
    export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT_RESOLVED"
    export GIT_CONFIG_COUNT=1
    export GIT_CONFIG_KEY_0=core.longpaths
    export GIT_CONFIG_VALUE_0=true

    ANDROID_NINJA_BIN=$(find_ninja_command || true)
    if [ -z "${ANDROID_NINJA_BIN:-}" ]; then
        printf 'Missing Ninja for Android builds. Install ninja or set "cmakeDir" to an SDK CMake package that includes ninja.\n' >&2
        exit 1
    fi
    GENERATOR_ARGS="-G Ninja -DCMAKE_MAKE_PROGRAM=\"$(normalize_path "$ANDROID_NINJA_BIN")\""

    EXTRA_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=\"$(normalize_path "$ANDROID_NDK_ROOT_RESOLVED")/build/cmake/android.toolchain.cmake\" -DANDROID_ABI=\"$ANDROID_ABI\" -DANDROID_PLATFORM=\"android-$ANDROID_API\" -DANDROID_STL=\"$ANDROID_STL\" -DGERANES_ANDROID_MIN_SDK=\"$ANDROID_API\""
else
    EXTRA_CMAKE_ARGS=""
fi

rm -rf "$DEPLOY_DIR"
mkdir -p "$BUILD_DIR"

cd "$ROOT_DIR"
eval "$CONFIGURE_CMD -S . -B \"$BUILD_DIR\" $GENERATOR_ARGS -DCMAKE_BUILD_TYPE=\"$BUILD_TYPE\" $EXTRA_CMAKE_ARGS"
if [ "$PLATFORM" = "android" ]; then
    cmake --build "$BUILD_DIR" --target copy_runtime_data -j "$JOBS"
    SDL_ANDROID_PROJECT_DIR="$BUILD_DIR/_deps/sdl2-src/android-project"
    if [ ! -d "$SDL_ANDROID_PROJECT_DIR" ]; then
        printf 'Could not locate SDL Android project template at %s\n' "$SDL_ANDROID_PROJECT_DIR" >&2
        exit 1
    fi

    prepare_android_assets "$ANDROID_ASSETS_DIR" "$BUILD_DIR/generated/runtime_data"
    prepare_android_project "$ANDROID_PROJECT_DIR" "$ANDROID_ASSETS_DIR" "$SDL_ANDROID_PROJECT_DIR"

    GRADLE_BIN=$(ensure_gradle)
    ANDROID_GRADLE_TASK="assembleRelease"
    if [ "$ANDROID_PACKAGE_FORMAT" = "aab" ]; then
        ANDROID_GRADLE_TASK="bundleRelease"
    fi
    if [ "$ANDROID_PACKAGE_VARIANT" != "release" ]; then
        if [ "$ANDROID_PACKAGE_FORMAT" = "aab" ]; then
            ANDROID_GRADLE_TASK="bundleDebug"
        else
            ANDROID_GRADLE_TASK="assembleDebug"
        fi
    fi

    if [ "$ANDROID_PACKAGE_VARIANT" = "release" ] && [ -z "${GERANES_ANDROID_KEYSTORE_PATH:-}" ]; then
        printf 'Warning: release Android build has no keystore configured in android/build-config.jsonc and may not be publishable.\n' >&2
    fi

    (cd "$ANDROID_PROJECT_DIR" && "$GRADLE_BIN" --no-daemon "$ANDROID_GRADLE_TASK")

    if [ "$ANDROID_PACKAGE_FORMAT" = "aab" ]; then
        ANDROID_PACKAGE_DIR="$ANDROID_PROJECT_DIR/app/build/outputs/bundle/$ANDROID_PACKAGE_VARIANT"
        ANDROID_PACKAGE_OUTPUT=$(resolve_android_package_output \
            "$ANDROID_PACKAGE_DIR" \
            "aab" \
            "$ANDROID_PACKAGE_DIR/app-$ANDROID_PACKAGE_VARIANT.aab" || true)
    else
        ANDROID_PACKAGE_DIR="$ANDROID_PROJECT_DIR/app/build/outputs/apk/$ANDROID_PACKAGE_VARIANT"
        ANDROID_PACKAGE_OUTPUT=$(resolve_android_package_output \
            "$ANDROID_PACKAGE_DIR" \
            "apk" \
            "$ANDROID_PACKAGE_DIR/app-$ANDROID_PACKAGE_VARIANT.apk" || true)
    fi
    if [ -z "${ANDROID_PACKAGE_OUTPUT:-}" ]; then
        printf 'Could not locate generated Android package under %s\n' "$ANDROID_PACKAGE_DIR" >&2
        exit 1
    fi
    ANDROID_NATIVE_LIB_OUTPUT=""
else
    cmake --build "$BUILD_DIR" --target "$TARGET" -j "$JOBS"
fi

rm -rf "$DEPLOY_DIR"

case "$PLATFORM" in
    linux|mingw)
        deploy_desktop
        ;;
    emscripten)
        deploy_emscripten
        ;;
    android)
        deploy_android
        ;;
esac

printf 'Build complete for %s\n' "$PLATFORM"
printf 'Build dir: %s/%s\n' "$ROOT_DIR" "$BUILD_DIR"
printf 'Deploy dir: %s/%s\n' "$ROOT_DIR" "$DEPLOY_DIR"
if [ "$PLATFORM" = "android" ]; then
    printf 'Android package format: %s\n' "$ANDROID_PACKAGE_FORMAT"
    printf 'Android Gradle project: %s\n' "$ANDROID_PROJECT_DIR"
fi
