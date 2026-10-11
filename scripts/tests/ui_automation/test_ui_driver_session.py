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


def test_launch_options_carry_composition_options_through_launch_json(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    session.directory.mkdir(parents=True)
    options = LaunchOptions(overlays=("C:/o1", "C:/o2"), settings=("view/x=false",), env=("QT_SCALE_FACTOR=1.5",))
    session.launch_file.write_text(json.dumps(options.to_json()))

    assert session.read_launch_options() == options
    assert json.loads(session.launch_file.read_text())["overlays"] == ["C:/o1", "C:/o2"]


def test_old_launch_json_without_new_keys_still_reads(tmp_path: Path) -> None:
    assert LaunchOptions.from_json({"dataset": "fixtures", "visible": False, "size": "800x600",
                                    "compiledQml": False}) == LaunchOptions(dataset="fixtures", size="800x600")


def test_launch_command_repeats_overlay_and_setting_flags_in_order(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    options = LaunchOptions(overlays=("A", "B"), settings=("k=1", "file/kovaaksDirs="), env=("X=1",))

    command = launch_command(Path("ksv_driver.exe"), session, options, reapply=True)

    assert command[3:] == ["--dataset", "rich", "--size", "1280x800", "--overlay", "A", "--overlay", "B",
                           "--setting", "k=1", "--setting", "file/kovaaksDirs=", "--reapply"]


def test_env_options_override_and_unset_the_headless_defaults(tmp_path: Path) -> None:
    toolchain = make_toolchain(tmp_path)
    options = LaunchOptions(env=("QT_SCALE_FACTOR=1.5", "QT_QUICK_BACKEND=", "QT_QPA_PLATFORM=minimal"))

    environment = launch_environment({"PATH": "base"}, toolchain, options)

    assert environment["QT_SCALE_FACTOR"] == "1.5"
    assert environment["QT_QPA_PLATFORM"] == "minimal"
    assert "QT_QUICK_BACKEND" not in environment


def test_malformed_env_assignments_name_the_input(tmp_path: Path) -> None:
    with pytest.raises(SessionError, match="'NOEQUALS'"):
        launch_environment({}, make_toolchain(tmp_path), LaunchOptions(env=("NOEQUALS",)))


def test_data_paths_stay_inside_the_session_data_dir(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    session.data_dir.mkdir(parents=True)

    assert session.data_path("benchmarks/a.json") == (session.data_dir / "benchmarks" / "a.json").resolve()
    with pytest.raises(SessionError, match="outside"):
        session.data_path("../settings/x.ini")
    with pytest.raises(SessionError, match="outside"):
        session.data_path(str(tmp_path / "elsewhere.txt"))


def test_profile_spec_and_injections_reach_the_driver_argv(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    options = LaunchOptions(dataset="empty", profile_spec="C:/specs/a.json", injections=("csv=fail", "decoder=delay:5"))

    command = launch_command(Path("ksv_driver.exe"), session, options)

    assert command[3:] == ["--dataset", "empty", "--size", "1280x800", "--profile-spec", "C:/specs/a.json",
                           "--inject", "csv=fail", "--inject", "decoder=delay:5"]
    assert "--profile-spec" not in launch_command(Path("ksv_driver.exe"), session, LaunchOptions())


def test_log_tail_returns_the_last_lines_for_startup_errors(tmp_path: Path) -> None:
    session = Session(tmp_path, "demo")
    assert session.log_tail() == "(no app.log)"
    session.directory.mkdir(parents=True)
    session.log_file.write_text("\n".join(f"line {n}" for n in range(50)), encoding="utf-8")

    assert session.log_tail(2) == "line 48\nline 49"
