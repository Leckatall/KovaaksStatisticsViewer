from __future__ import annotations

from pathlib import Path
import sys

import pytest


PROJECT_ROOT = Path(__file__).resolve().parents[3]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.build_runner.config import MINGW_RUNTIME_DLLS  # noqa: E402


@pytest.fixture
def fixtures_dir() -> Path:
    return Path(__file__).with_name("fixtures")


def _materialize(paths: dict[str, Path]) -> dict[str, str]:
    for key, path in paths.items():
        if key.endswith("_DIR"):
            path.mkdir(parents=True, exist_ok=True)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(key.encode())
    return {key: str(path) for key, path in paths.items()}


def _write_env(path: Path, values: dict[str, str]) -> None:
    path.write_text("\n".join(f"{key}={value}" for key, value in values.items()) + "\n", encoding="utf-8")


@pytest.fixture
def toolchain_tree(tmp_path: Path) -> tuple[Path, dict[str, str]]:
    """A scripts/ dir with the defaults, a mingw and an msvc profile; returns the resolved mingw (default) values."""
    scripts_dir = tmp_path / "scripts"
    scripts_dir.mkdir()

    shared = _materialize(
        {
            "KSV_CMAKE_EXE": tmp_path / "cmake" / "cmake.exe",
            "KSV_NINJA_EXE": tmp_path / "ninja" / "ninja.exe",
        }
    )
    dependencies = {
        "KSV_VCPKG_TOOLCHAIN": tmp_path / "vcpkg" / "vcpkg.cmake",
        "KSV_VCPKG_BIN_DIR": tmp_path / "vcpkg" / "bin",
        "KSV_PROTOC_EXE": tmp_path / "vcpkg" / "protoc.exe",
        "KSV_QT_BIN_DIR": tmp_path / "qt" / "bin",
        "KSV_QT6_DIR": tmp_path / "qt" / "lib" / "cmake" / "Qt6",
    }
    mingw = _materialize(
        {
            "KSV_C_COMPILER": tmp_path / "mingw" / "gcc.exe",
            "KSV_CXX_COMPILER": tmp_path / "mingw" / "c++.exe",
            **dependencies,
        }
    )
    mingw["KSV_VCPKG_TRIPLET"] = "x64-mingw-dynamic"
    for dll in MINGW_RUNTIME_DLLS:
        (tmp_path / "mingw" / dll).write_bytes(dll.encode())
    msvc = _materialize(
        {
            "KSV_C_COMPILER": tmp_path / "msvc" / "cl.exe",
            "KSV_CXX_COMPILER": tmp_path / "msvc" / "cl.exe",
            "KSV_VCVARS_BAT": tmp_path / "msvc" / "vcvars64.bat",
            **dependencies,
        }
    )
    msvc["KSV_VCPKG_TRIPLET"] = "x64-windows"

    _write_env(
        scripts_dir / "toolchain.defaults.env",
        {"KSV_TOOLCHAIN": "mingw", **shared, "KSV_BUILD_JOBS": "8", "KSV_BUILD_PRIORITY": "normal"},
    )
    _write_env(scripts_dir / "toolchain.mingw.env", mingw)
    _write_env(scripts_dir / "toolchain.msvc.env", msvc)
    return scripts_dir, {"KSV_TOOLCHAIN": "mingw", **shared, **mingw}
