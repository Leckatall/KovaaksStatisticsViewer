from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from typing import Callable, Mapping


TOOLCHAIN_KEYS = (
    "KSV_CMAKE_EXE",
    "KSV_NINJA_EXE",
    "KSV_C_COMPILER",
    "KSV_CXX_COMPILER",
    "KSV_VCPKG_TOOLCHAIN",
    "KSV_VCPKG_TRIPLET",
    "KSV_VCPKG_BIN_DIR",
    "KSV_PROTOC_EXE",
    "KSV_QT_BIN_DIR",
    "KSV_QT6_DIR",
)
# Runner settings share the env files but stay out of the toolchain fingerprint,
# so changing them never wipes build-agent/.
RUNNER_KEYS = (
    "KSV_BUILD_JOBS",
    "KSV_BUILD_PRIORITY",
)
KNOWN_KEYS = TOOLCHAIN_KEYS + RUNNER_KEYS
LOCAL_ENV_NAME = "toolchain.local.env"
# Windows priority classes; children of a below-normal or idle process inherit its class,
# so this also covers the compilers and test runners that cmake/ninja/ctest start.
BUILD_PRIORITIES = {
    "normal": 0,
    "below_normal": getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0),
    "idle": getattr(subprocess, "IDLE_PRIORITY_CLASS", 0),
}


class ToolchainError(RuntimeError):
    pass


@dataclass(frozen=True)
class Toolchain:
    cmake_exe: Path
    ninja_exe: Path
    c_compiler: Path
    cxx_compiler: Path
    vcpkg_toolchain: Path
    vcpkg_triplet: str
    vcpkg_bin_dir: Path
    protoc_exe: Path
    qt_bin_dir: Path
    qt6_dir: Path


@dataclass(frozen=True)
class RunnerSettings:
    jobs: int
    priority: str = "normal"

    @property
    def creationflags(self) -> int:
        return BUILD_PRIORITIES[self.priority]


def read_toolchain_env(path: Path, *, optional: bool = False) -> dict[str, str]:
    if not path.is_file():
        if optional:
            return {}
        raise ToolchainError(f"Toolchain environment file not found: {path}")

    values: dict[str, str] = {}
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line or line.index("=") == 0:
            raise ToolchainError(f"Malformed toolchain entry at {path}:{line_number} (expected NAME=value)")
        key, value = (part.strip() for part in line.split("=", 1))
        if key not in KNOWN_KEYS:
            raise ToolchainError(f"Unknown toolchain key '{key}' at {path}:{line_number}")
        if key in values:
            raise ToolchainError(f"Duplicate toolchain key '{key}' at {path}:{line_number}")
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        if not value:
            raise ToolchainError(f"Empty toolchain value for '{key}' at {path}:{line_number}")
        values[key] = value
    return values


def _resolve_path(value: str, what: str, *, directory: bool = False) -> Path:
    path = Path(value)
    if not path.is_absolute():
        raise ToolchainError(f"{what} must be an absolute path: {path}")
    if directory and not path.is_dir():
        raise ToolchainError(f"{what} not found at: {path}")
    if not directory and not path.is_file():
        raise ToolchainError(f"{what} not found at: {path}")
    return path.resolve()


def main_worktree_root(repo_root: Path) -> Path | None:
    """The main checkout that owns repo_root's git directory, or None outside a non-bare repository."""
    try:
        result = subprocess.run(
            ["git", "-C", str(repo_root), "rev-parse", "--git-common-dir"],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return None
    output = result.stdout.strip()
    if result.returncode or not output:
        return None
    common_dir = Path(output)
    if not common_dir.is_absolute():
        common_dir = repo_root / common_dir
    common_dir = common_dir.resolve()
    if common_dir.name != ".git":
        return None
    return common_dir.parent


def find_local_env(
    scripts_dir: Path,
    *,
    main_root_of: Callable[[Path], Path | None] = main_worktree_root,
) -> Path | None:
    """This checkout's local env file, else the main worktree's, so linked worktrees share one copy."""
    own = scripts_dir / LOCAL_ENV_NAME
    if own.is_file():
        return own
    repo_root = scripts_dir.parent
    main_root = main_root_of(repo_root)
    if main_root is None or normalized_path(main_root) == normalized_path(repo_root):
        return None
    shared = main_root / scripts_dir.name / LOCAL_ENV_NAME
    return shared if shared.is_file() else None


def _layered_values(
    scripts_dir: Path,
    keys: tuple[str, ...],
    process_environment: Mapping[str, str] | None,
) -> dict[str, str]:
    resolved = read_toolchain_env(scripts_dir / "toolchain.defaults.env")
    local_env = find_local_env(scripts_dir)
    if local_env is not None:
        resolved.update(read_toolchain_env(local_env))
    environment = os.environ if process_environment is None else process_environment
    for key in keys:
        if environment.get(key):
            resolved[key] = environment[key]
    return resolved


def resolve_runner_settings(
    scripts_dir: Path,
    *,
    process_environment: Mapping[str, str] | None = None,
) -> RunnerSettings:
    resolved = _layered_values(scripts_dir, RUNNER_KEYS, process_environment)
    for key in RUNNER_KEYS:
        if key not in resolved:
            raise ToolchainError(f"Missing required runner key: {key}")

    jobs_text = resolved["KSV_BUILD_JOBS"].strip()
    jobs = int(jobs_text) if jobs_text.isdigit() else 0
    if jobs < 1:
        raise ToolchainError(f"KSV_BUILD_JOBS must be a positive integer, not '{jobs_text}'")
    priority = resolved["KSV_BUILD_PRIORITY"].strip().lower()
    if priority not in BUILD_PRIORITIES:
        choices = ", ".join(BUILD_PRIORITIES)
        raise ToolchainError(f"KSV_BUILD_PRIORITY must be one of {choices}, not '{priority}'")
    return RunnerSettings(jobs=jobs, priority=priority)


def resolve_toolchain(
    scripts_dir: Path,
    *,
    process_environment: Mapping[str, str] | None = None,
    overrides: Mapping[str, str] | None = None,
) -> Toolchain:
    resolved = _layered_values(scripts_dir, TOOLCHAIN_KEYS, process_environment)
    for key, value in (overrides or {}).items():
        if key not in TOOLCHAIN_KEYS:
            raise ToolchainError(f"Unknown toolchain override: {key}")
        if value:
            resolved[key] = value
    for key in TOOLCHAIN_KEYS:
        if key not in resolved:
            raise ToolchainError(f"Missing required toolchain key: {key}")

    triplet = resolved["KSV_VCPKG_TRIPLET"].strip()
    if not triplet:
        raise ToolchainError("KSV_VCPKG_TRIPLET must not be empty")
    return Toolchain(
        cmake_exe=_resolve_path(resolved["KSV_CMAKE_EXE"], "CMake executable"),
        ninja_exe=_resolve_path(resolved["KSV_NINJA_EXE"], "Ninja executable"),
        c_compiler=_resolve_path(resolved["KSV_C_COMPILER"], "C compiler"),
        cxx_compiler=_resolve_path(resolved["KSV_CXX_COMPILER"], "C++ compiler"),
        vcpkg_toolchain=_resolve_path(resolved["KSV_VCPKG_TOOLCHAIN"], "vcpkg toolchain file"),
        vcpkg_triplet=triplet,
        vcpkg_bin_dir=_resolve_path(
            resolved["KSV_VCPKG_BIN_DIR"], "vcpkg triplet bin directory", directory=True
        ),
        protoc_exe=_resolve_path(resolved["KSV_PROTOC_EXE"], "protoc executable"),
        qt_bin_dir=_resolve_path(resolved["KSV_QT_BIN_DIR"], "Qt bin directory", directory=True),
        qt6_dir=_resolve_path(resolved["KSV_QT6_DIR"], "Qt6 CMake package directory", directory=True),
    )


def normalized_path(path: Path | str) -> str:
    return Path(path).resolve().as_posix().rstrip("/").lower()


def _cache_value(cache: str, key: str) -> str:
    match = re.search(rf"^{re.escape(key)}:[^=]*=(.*)$", cache, flags=re.MULTILINE)
    if not match:
        raise ToolchainError(f"{key} is missing from the generated CMake cache.")
    return match.group(1).strip()


def assert_cache(cache_path: Path, toolchain: Toolchain) -> None:
    cache = cache_path.read_text(encoding="utf-8", errors="replace")
    literal_values = {
        "CMAKE_GENERATOR": "Ninja",
        "CMAKE_BUILD_TYPE": "Debug",
        "VCPKG_TARGET_TRIPLET": toolchain.vcpkg_triplet,
        "BUILD_TESTING": "ON",
        "KSV_BUILD_GALLERY": "ON",
    }
    for key, expected in literal_values.items():
        actual = _cache_value(cache, key)
        if actual != expected:
            raise ToolchainError(f"{key} in the generated cache is '{actual}'; expected '{expected}'.")

    path_values = {
        "CMAKE_MAKE_PROGRAM": toolchain.ninja_exe,
        "CMAKE_C_COMPILER": toolchain.c_compiler,
        "CMAKE_CXX_COMPILER": toolchain.cxx_compiler,
        "CMAKE_TOOLCHAIN_FILE": toolchain.vcpkg_toolchain,
        "Protobuf_PROTOC_EXECUTABLE": toolchain.protoc_exe,
        "Qt6_DIR": toolchain.qt6_dir,
    }
    for key, expected in path_values.items():
        actual = _cache_value(cache, key)
        if normalized_path(actual) != normalized_path(expected):
            raise ToolchainError(f"{key} in the generated cache is '{actual}'; expected '{expected}'.")


def safe_remove_build_tree(path: Path, repo_root: Path) -> None:
    expected = normalized_path(repo_root / "build-agent")
    if normalized_path(path) != expected:
        raise ToolchainError(f"Refusing to remove unexpected build directory: {path}")
    if path.exists():
        shutil.rmtree(path)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_fingerprint(toolchain: Toolchain) -> dict[str, object]:
    compiler_dir = toolchain.cxx_compiler.parent
    files = [
        toolchain.cmake_exe,
        toolchain.ninja_exe,
        toolchain.c_compiler,
        toolchain.cxx_compiler,
        toolchain.vcpkg_toolchain,
        toolchain.protoc_exe,
        compiler_dir / "libgcc_s_seh-1.dll",
        compiler_dir / "libstdc++-6.dll",
        compiler_dir / "libwinpthread-1.dll",
        toolchain.vcpkg_bin_dir / "libabseil_dll.dll",
        toolchain.vcpkg_bin_dir / "libprotobuf-lite.dll",
        toolchain.vcpkg_bin_dir / "libprotobuf.dll",
        toolchain.vcpkg_bin_dir / "libprotoc.dll",
        *sorted(toolchain.qt6_dir.rglob("*.cmake")),
    ]
    hashes: dict[str, str] = {}
    for path in files:
        if not path.is_file():
            raise ToolchainError(f"Fingerprint input is missing: {path}")
        hashes[normalized_path(path)] = _sha256(path)
    return {
        "schema": 1,
        "generator": "Ninja",
        "buildType": "Debug",
        "buildTesting": "ON",
        "buildGallery": "ON",
        "cmake": normalized_path(toolchain.cmake_exe),
        "ninja": normalized_path(toolchain.ninja_exe),
        "cCompiler": normalized_path(toolchain.c_compiler),
        "cxxCompiler": normalized_path(toolchain.cxx_compiler),
        "vcpkgToolchain": normalized_path(toolchain.vcpkg_toolchain),
        "vcpkgTriplet": toolchain.vcpkg_triplet,
        "vcpkgBin": normalized_path(toolchain.vcpkg_bin_dir),
        "protoc": normalized_path(toolchain.protoc_exe),
        "qtBin": normalized_path(toolchain.qt_bin_dir),
        "qt6Dir": normalized_path(toolchain.qt6_dir),
        "files": hashes,
    }


def fingerprint_matches(path: Path, expected: dict[str, object]) -> bool:
    if not path.is_file():
        return False
    try:
        return json.loads(path.read_text(encoding="utf-8")) == expected
    except (json.JSONDecodeError, OSError):
        return False


def write_fingerprint(path: Path, fingerprint: dict[str, object]) -> None:
    path.write_text(json.dumps(fingerprint, indent=2) + "\n", encoding="utf-8")
