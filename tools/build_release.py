#!/usr/bin/env python3
"""
Build and package GeraNES release archives.

Examples:
    py -3 tools/build_release.py --system win --version 1-2-3
    py -3 tools/build_release.py --system web --version 1-2-3 --emscripten-env "C:\\emsdk\\emsdk_env.bat"
    py -3 tools/build_release.py --system linux --version 1-2-3 --toolchain /path/to/linux-toolchain.cmake
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT_DIR = Path(__file__).resolve().parent.parent
DIST_DIR = ROOT_DIR / "dist"
APP_NAME = "GeraNES"


def log(message: str) -> None:
    print(message, flush=True)


def fail(message: str) -> int:
    print(message, file=sys.stderr, flush=True)
    return 1


def detect_version() -> str:
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            cwd=ROOT_DIR,
            capture_output=True,
            text=True,
            check=True,
        )
        tag = result.stdout.strip()
        if tag:
            return tag.lstrip("vV").replace(".", "-")
    except Exception:
        pass

    raise RuntimeError("could not detect version from git tags")


def host_system_name() -> str:
    system = platform.system().lower()
    if "windows" in system:
        return "win"
    if "linux" in system:
        return "linux"
    if "darwin" in system or "mac" in system:
        return "mac"
    return system


def default_build_dir(system_name: str) -> Path:
    return ROOT_DIR / f"build-{system_name}"


def release_name(version: str, system_name: str) -> str:
    return f"{APP_NAME}-v-{version}-{system_name}"


def detect_emscripten_env() -> str | None:
    candidates: list[Path] = []

    env_emsdk = os.environ.get("EMSDK")
    if env_emsdk:
        candidates.append(Path(env_emsdk) / "emsdk_env.bat")

    env_emsdk_path = os.environ.get("EMSDK_PATH")
    if env_emsdk_path:
        candidates.append(Path(env_emsdk_path) / "emsdk_env.bat")

    candidates.extend(
        [
            Path("C:/emsdk/emsdk_env.bat"),
            Path.home() / "emsdk" / "emsdk_env.bat",
        ]
    )

    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return None


def detect_toolchain(system_name: str) -> Path | None:
    env_key = f"GERANES_{system_name.upper()}_TOOLCHAIN"
    from_env = os.environ.get(env_key)
    if from_env:
        candidate = Path(from_env)
        if candidate.exists():
            return candidate.resolve()

    candidates: list[Path] = []
    if system_name == "win":
        candidates.extend(
            [
                ROOT_DIR / "build" / "_deps" / "sdl2-src" / "build-scripts" / "cmake-toolchain-mingw64-x86_64.cmake",
                ROOT_DIR / "build" / "_deps" / "pffft-src" / "mingw-w64-x64_64.cmake",
            ]
        )
    elif system_name == "linux":
        candidates.extend(
            [
                ROOT_DIR / "toolchains" / "linux.cmake",
                ROOT_DIR / "cmake" / "linux-toolchain.cmake",
            ]
        )
    elif system_name == "mac":
        candidates.extend(
            [
                ROOT_DIR / "toolchains" / "mac.cmake",
                ROOT_DIR / "cmake" / "mac-toolchain.cmake",
                ROOT_DIR / "toolchains" / "osxcross.cmake",
            ]
        )

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def run_command(command: list[str], workdir: Path, env: dict[str, str] | None = None) -> None:
    log(f"[cmd] {' '.join(command)}")
    subprocess.run(command, cwd=workdir, env=env, check=True)


def run_shell_command(command: str, workdir: Path, env: dict[str, str] | None = None) -> None:
    log(f"[cmd] {command}")
    subprocess.run(command, cwd=workdir, env=env, shell=True, check=True)


def configure_and_build(
    system_name: str,
    build_dir: Path,
    build_type: str,
    toolchain: Path | None,
    generator: str | None,
    emscripten_env: str | None,
) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)

    if system_name == "web":
        if not emscripten_env:
            emscripten_env = detect_emscripten_env()
        if not emscripten_env:
            raise RuntimeError(
                "web builds require --emscripten-env or a detectable EMSDK installation with emsdk_env.bat"
            )

        configure_cmd = (
            f'call "{emscripten_env}" && '
            f'cmake -S . -B "{build_dir}" -DCMAKE_BUILD_TYPE={build_type}'
        )
        build_cmd = f'call "{emscripten_env}" && cmake --build "{build_dir}" --config {build_type}'
        run_shell_command(configure_cmd, ROOT_DIR)
        run_shell_command(build_cmd, ROOT_DIR)
        return

    configure_cmd = ["cmake", "-S", ".", "-B", str(build_dir), f"-DCMAKE_BUILD_TYPE={build_type}"]

    if generator:
        configure_cmd.extend(["-G", generator])
    elif system_name == "win" and host_system_name() == "win":
        configure_cmd.extend(["-G", "MinGW Makefiles"])

    if toolchain:
        configure_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
    elif system_name in ("linux", "mac") and host_system_name() != system_name:
        auto_toolchain = detect_toolchain(system_name)
        if auto_toolchain:
            toolchain = auto_toolchain
            configure_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
            log(f"[release] auto-detected toolchain: {toolchain}")
        else:
            raise RuntimeError(
                f"{system_name} builds from this host require --toolchain or a detectable toolchain file"
            )

    run_command(configure_cmd, ROOT_DIR)
    run_command(["cmake", "--build", str(build_dir), "--config", build_type], ROOT_DIR)


def copy_file_if_exists(src: Path, dest: Path) -> bool:
    if not src.exists():
        return False
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)
    return True


def copy_tree_if_exists(src: Path, dest: Path) -> bool:
    if not src.exists():
        return False
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(src, dest)
    return True


def collect_runtime_files(system_name: str, build_dir: Path, stage_dir: Path) -> list[str]:
    copied: list[str] = []

    def add_file(path: Path, relative_name: str | None = None) -> None:
        dest = stage_dir / (relative_name or path.name)
        if copy_file_if_exists(path, dest):
            copied.append(str(dest.relative_to(stage_dir)))

    def add_dir(path: Path, relative_name: str | None = None) -> None:
        dest = stage_dir / (relative_name or path.name)
        if copy_tree_if_exists(path, dest):
            copied.append(str(dest.relative_to(stage_dir)) + "/")

    if system_name == "win":
        add_file(build_dir / "GeraNES.exe")
        add_file(build_dir / "db.txt")
        add_dir(build_dir / "shaders")
    elif system_name == "linux":
        add_file(build_dir / "GeraNES")
        add_file(build_dir / "db.txt")
        add_dir(build_dir / "shaders")
    elif system_name == "mac":
        app_bundle = build_dir / "GeraNES.app"
        if app_bundle.exists():
            add_dir(app_bundle)
        else:
            add_file(build_dir / "GeraNES")
            add_file(build_dir / "db.txt")
            add_dir(build_dir / "shaders")
    elif system_name == "web":
        for ext in (".html", ".js", ".wasm", ".data", ".ico"):
            for file in build_dir.glob(f"*{ext}"):
                add_file(file)
        for file in build_dir.glob("*.worker.js"):
            add_file(file)
    else:
        raise RuntimeError(f"unsupported system: {system_name}")

    if not copied:
        raise RuntimeError(f"no release files were found in {build_dir} for system '{system_name}'")

    return copied


def create_zip_from_stage(stage_dir: Path, output_zip: Path) -> None:
    output_zip.parent.mkdir(parents=True, exist_ok=True)
    zip_root = stage_dir.parent
    with ZipFile(output_zip, "w", compression=ZIP_DEFLATED, compresslevel=9) as zip_file:
        for file_path in sorted(stage_dir.rglob("*")):
            if file_path.is_dir():
                continue
            zip_file.write(file_path, file_path.relative_to(zip_root))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Build and package GeraNES release archives.")
    parser.add_argument("--system", choices=("win", "linux", "mac", "web"), required=True)
    parser.add_argument("--build-type", default="Release", help="CMake build type (default: Release)")
    parser.add_argument("--build-dir", type=Path, help="build directory to use")
    parser.add_argument("--toolchain", type=Path, help="optional CMake toolchain file for cross builds")
    parser.add_argument("--generator", help="optional CMake generator override")
    parser.add_argument(
        "--emscripten-env",
        help="path to emsdk_env.bat or equivalent script required for --system web on Windows",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="skip the CMake build step and only package files from the build directory",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()

    version = detect_version()
    build_dir = args.build_dir.resolve() if args.build_dir else default_build_dir(args.system)
    output_zip = DIST_DIR / f"{release_name(version, args.system)}.zip"

    log(f"[release] system={args.system}")
    log(f"[release] version={version}")
    log(f"[release] build_dir={build_dir}")
    log(f"[release] output={output_zip}")

    try:
        if not args.skip_build:
            configure_and_build(
                system_name=args.system,
                build_dir=build_dir,
                build_type=args.build_type,
                toolchain=args.toolchain.resolve() if args.toolchain else detect_toolchain(args.system),
                generator=args.generator,
                emscripten_env=args.emscripten_env or detect_emscripten_env(),
            )

        with tempfile.TemporaryDirectory(prefix="geranes-release-") as temp_dir_str:
            temp_dir = Path(temp_dir_str)
            stage_dir = temp_dir / release_name(version, args.system)
            stage_dir.mkdir(parents=True, exist_ok=True)

            copied = collect_runtime_files(args.system, build_dir, stage_dir)
            for item in copied:
                log(f"[release] packaged {item}")

            create_zip_from_stage(stage_dir, output_zip)

        log(f"[release] created {output_zip}")
        return 0
    except subprocess.CalledProcessError as exc:
        return fail(f"[release] command failed with exit code {exc.returncode}")
    except Exception as exc:
        return fail(f"[release] error: {exc}")


if __name__ == "__main__":
    raise SystemExit(main())
