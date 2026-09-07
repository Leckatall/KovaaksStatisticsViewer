from __future__ import annotations

from pathlib import Path
import sys

import pytest


PROJECT_ROOT = Path(__file__).resolve().parents[3]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))


@pytest.fixture
def fixtures_dir() -> Path:
    return Path(__file__).with_name("fixtures")


@pytest.fixture
def toolchain_tree(tmp_path: Path) -> tuple[Path, dict[str, str]]:
    scripts_dir = tmp_path / "scripts"
    scripts_dir.mkdir()

    paths = {
        "KSV_CMAKE_EXE": tmp_path / "cmake" / "cmake.exe",
        "KSV_NINJA_EXE": tmp_path / "ninja" / "ninja.exe",
        "KSV_C_COMPILER": tmp_path / "mingw" / "gcc.exe",
        "KSV_CXX_COMPILER": tmp_path / "mingw" / "c++.exe",
        "KSV_VCPKG_TOOLCHAIN": tmp_path / "vcpkg" / "vcpkg.cmake",
        "KSV_VCPKG_BIN_DIR": tmp_path / "vcpkg" / "bin",
        "KSV_PROTOC_EXE": tmp_path / "vcpkg" / "protoc.exe",
        "KSV_QT_BIN_DIR": tmp_path / "qt" / "bin",
        "KSV_QT6_DIR": tmp_path / "qt" / "lib" / "cmake" / "Qt6",
    }
    for key, path in paths.items():
        if key.endswith("_DIR"):
            path.mkdir(parents=True)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(key.encode())

    values = {key: str(path) for key, path in paths.items()}
    values["KSV_VCPKG_TRIPLET"] = "x64-mingw-dynamic"
    defaults = "\n".join(f"{key}={value}" for key, value in values.items()) + "\n"
    (scripts_dir / "toolchain.defaults.env").write_text(defaults, encoding="utf-8")
    return scripts_dir, values
