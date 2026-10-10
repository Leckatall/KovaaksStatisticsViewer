from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

from build_runner.config import Toolchain
from ui_automation.session import (
    LaunchOptions,
    Session,
    SessionError,
    launch_command,
    launch_environment,
    pid_alive,
)


def make_toolchain(root: Path) -> Toolchain:
    return Toolchain(
        cmake_exe=root / "cmake.exe",
        ninja_exe=root / "ninja.exe",
        c_compiler=root / "mingw" / "gcc.exe",
        cxx_compiler=root / "mingw" / "g++.exe",
        vcpkg_toolchain=root / "vcpkg.cmake",
        vcpkg_triplet="x64-mingw-dynamic",
        vcpkg_bin_dir=root / "vcpkg" / "bin",
        protoc_exe=root / "protoc.exe",
        qt_bin_dir=root / "qt" / "bin",
        qt6_dir=root / "qt" / "lib",
    )


def test_session_layout_lives_under_dot_temp(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")

    assert session.directory == tmp_path / ".temp" / "ui-driver" / "demo"
    assert session.session_file.name == "session.json"
    assert session.read_info() is None


def test_session_names_are_restricted(tmp_path: Path) -> None:
    with pytest.raises(SessionError):
        Session(tmp_path, "../escape")


def test_shot_paths_count_up_and_slugify_labels(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")

    first = session.next_shot_path("click-win:settingsDialog/text:Close")
    first.write_bytes(b"png")
    second = session.next_shot_path("")

    assert first.name == "001-click-win-settingsDialog-text-Close.png"
    assert second.name == "002-shot.png"


def test_launch_options_round_trip(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    session.directory.mkdir(parents=True)
    options = LaunchOptions(dataset="fixtures", visible=True, size="800x600", compiled_qml=True)
    session.launch_file.write_text(json.dumps(options.to_json()))

    assert session.read_launch_options() == options


def test_launch_command_passes_session_and_options(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    command = launch_command(Path("ksv_driver.exe"), session, LaunchOptions(dataset="empty", compiled_qml=True))

    assert command[:3] == ["ksv_driver.exe", "--session-dir", str(session.directory)]
    assert command[3:] == ["--dataset", "empty", "--size", "1280x800", "--compiled-qml"]


def test_headless_environment_adds_offscreen_qpa_and_runtime_path(tmp_path: Path) -> None:
    toolchain = make_toolchain(tmp_path)

    headless = launch_environment({"PATH": "base"}, toolchain, LaunchOptions())
    visible = launch_environment({"PATH": "base", "QT_QPA_PLATFORM": "offscreen"}, toolchain, LaunchOptions(visible=True))

    assert headless["QT_QPA_PLATFORM"] == "offscreen"
    assert headless["QT_QUICK_BACKEND"] == "software"
    assert headless["PATH"].split(os.pathsep)[:3] == [
        str(tmp_path / "mingw"),
        str(tmp_path / "vcpkg" / "bin"),
        str(tmp_path / "qt" / "bin"),
    ]
    assert "QT_QPA_PLATFORM" not in visible


def test_wipe_only_touches_the_session_directory(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    (session.directory / "data").mkdir(parents=True)
    neighbour = session.sessions_root / "other"
    neighbour.mkdir()

    session.wipe()

    assert not session.directory.exists()
    assert neighbour.exists()


def test_pid_alive_reports_this_process() -> None:
    assert pid_alive(os.getpid())
