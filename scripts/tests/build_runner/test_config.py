from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess

import pytest

from scripts.build_runner.config import (
    BUILD_PRIORITIES,
    RunnerSettings,
    ToolchainError,
    assert_cache,
    find_local_env,
    fingerprint_matches,
    main_worktree_root,
    read_toolchain_env,
    resolve_runner_settings,
    resolve_toolchain,
    safe_remove_build_tree,
)


def test_environment_overrides_local_and_defaults(toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path) -> None:
    scripts_dir, values = toolchain_tree
    local_ninja = tmp_path / "local" / "ninja.exe"
    process_ninja = tmp_path / "process" / "ninja.exe"
    local_ninja.parent.mkdir()
    process_ninja.parent.mkdir()
    local_ninja.write_bytes(b"local")
    process_ninja.write_bytes(b"process")
    (scripts_dir / "toolchain.local.env").write_text(
        f"KSV_NINJA_EXE={local_ninja}\nKSV_VCPKG_TRIPLET=local-triplet\n",
        encoding="utf-8",
    )

    config = resolve_toolchain(
        scripts_dir,
        process_environment={"KSV_NINJA_EXE": str(process_ninja)},
    )

    assert config.ninja_exe == process_ninja.resolve()
    assert config.vcpkg_triplet == "local-triplet"
    assert config.cmake_exe == Path(values["KSV_CMAKE_EXE"]).resolve()


def test_runner_settings_layer_like_the_toolchain(toolchain_tree: tuple[Path, dict[str, str]]) -> None:
    scripts_dir, _ = toolchain_tree
    assert resolve_runner_settings(scripts_dir, process_environment={}) == RunnerSettings(jobs=8, priority="normal")

    (scripts_dir / "toolchain.local.env").write_text(
        "KSV_BUILD_JOBS=4\nKSV_BUILD_PRIORITY=Below_Normal\n", encoding="utf-8"
    )
    local = resolve_runner_settings(scripts_dir, process_environment={})
    assert local == RunnerSettings(jobs=4, priority="below_normal")
    assert local.creationflags == BUILD_PRIORITIES["below_normal"]

    process = resolve_runner_settings(scripts_dir, process_environment={"KSV_BUILD_JOBS": "2"})
    assert process == RunnerSettings(jobs=2, priority="below_normal")


@pytest.mark.parametrize(
    ("environment", "message"),
    [
        ({"KSV_BUILD_JOBS": "0"}, "KSV_BUILD_JOBS must be a positive integer, not '0'"),
        ({"KSV_BUILD_JOBS": "four"}, "KSV_BUILD_JOBS must be a positive integer, not 'four'"),
        ({"KSV_BUILD_PRIORITY": "high"}, "KSV_BUILD_PRIORITY must be one of normal, below_normal, idle, not 'high'"),
    ],
)
def test_invalid_runner_settings_are_actionable(
    toolchain_tree: tuple[Path, dict[str, str]], environment: dict[str, str], message: str
) -> None:
    scripts_dir, _ = toolchain_tree

    with pytest.raises(ToolchainError, match=message):
        resolve_runner_settings(scripts_dir, process_environment=environment)


def test_runner_settings_do_not_change_the_resolved_toolchain(toolchain_tree: tuple[Path, dict[str, str]]) -> None:
    scripts_dir, _ = toolchain_tree
    before = resolve_toolchain(scripts_dir, process_environment={})
    (scripts_dir / "toolchain.local.env").write_text("KSV_BUILD_JOBS=2\n", encoding="utf-8")

    assert resolve_toolchain(scripts_dir, process_environment={}) == before


def test_linked_worktree_without_local_env_uses_the_main_worktrees(tmp_path: Path) -> None:
    main_scripts = tmp_path / "main" / "scripts"
    linked_scripts = tmp_path / "linked" / "scripts"
    main_scripts.mkdir(parents=True)
    linked_scripts.mkdir(parents=True)
    shared = main_scripts / "toolchain.local.env"
    shared.write_text("KSV_BUILD_JOBS=4\n", encoding="utf-8")

    def main_root_of(_: Path) -> Path:
        return tmp_path / "main"

    assert find_local_env(linked_scripts, main_root_of=main_root_of) == shared

    own = linked_scripts / "toolchain.local.env"
    own.write_text("KSV_BUILD_JOBS=2\n", encoding="utf-8")
    assert find_local_env(linked_scripts, main_root_of=main_root_of) == own


def test_main_worktree_without_local_env_has_none(tmp_path: Path) -> None:
    scripts_dir = tmp_path / "main" / "scripts"
    scripts_dir.mkdir(parents=True)

    assert find_local_env(scripts_dir, main_root_of=lambda _: tmp_path / "main") is None
    assert find_local_env(scripts_dir, main_root_of=lambda _: None) is None


@pytest.mark.skipif(shutil.which("git") is None, reason="git is not installed")
def test_main_worktree_root_resolves_from_a_linked_worktree(tmp_path: Path) -> None:
    main = tmp_path / "main"
    linked = tmp_path / "linked"
    main.mkdir()

    def git(*arguments: str) -> None:
        subprocess.run(["git", "-C", str(main), *arguments], check=True, capture_output=True)

    git("init", "-q")
    git("-c", "user.name=test", "-c", "user.email=test@example.invalid", "commit", "-q", "--allow-empty", "-m", "init")
    git("worktree", "add", "-q", "--detach", str(linked))

    assert main_worktree_root(linked) == main.resolve()
    assert main_worktree_root(main) == main.resolve()
    assert main_worktree_root(tmp_path / "missing") is None


@pytest.mark.parametrize(
    ("contents", "message"),
    [
        ("NOT_AN_ENTRY\n", "expected NAME=value"),
        ("UNKNOWN=value\n", "Unknown toolchain key 'UNKNOWN'"),
        ("KSV_CMAKE_EXE=one\nKSV_CMAKE_EXE=two\n", "Duplicate toolchain key 'KSV_CMAKE_EXE'"),
        ("KSV_CMAKE_EXE=\n", "Empty toolchain value for 'KSV_CMAKE_EXE'"),
    ],
)
def test_malformed_toolchain_entries_are_actionable(tmp_path: Path, contents: str, message: str) -> None:
    path = tmp_path / "toolchain.env"
    path.write_text(contents, encoding="utf-8")

    with pytest.raises(ToolchainError, match=message):
        read_toolchain_env(path)


def test_assert_cache_accepts_semantically_matching_paths(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    scripts_dir, _ = toolchain_tree
    config = resolve_toolchain(scripts_dir, process_environment={})
    cache = tmp_path / "CMakeCache.txt"
    cache.write_text(
        "\n".join(
            [
                "CMAKE_GENERATOR:INTERNAL=Ninja",
                "CMAKE_BUILD_TYPE:STRING=Debug",
                f"CMAKE_MAKE_PROGRAM:FILEPATH={config.ninja_exe}",
                f"CMAKE_C_COMPILER:FILEPATH={config.c_compiler}",
                f"CMAKE_CXX_COMPILER:FILEPATH={config.cxx_compiler}",
                f"CMAKE_TOOLCHAIN_FILE:FILEPATH={config.vcpkg_toolchain}",
                f"VCPKG_TARGET_TRIPLET:STRING={config.vcpkg_triplet}",
                f"Protobuf_PROTOC_EXECUTABLE:FILEPATH={config.protoc_exe}",
                f"Qt6_DIR:PATH={config.qt6_dir}",
                "BUILD_TESTING:BOOL=ON",
                "KSV_BUILD_GALLERY:BOOL=ON",
            ]
        ),
        encoding="utf-8",
    )

    assert_cache(cache, config)


def test_safe_remove_refuses_any_directory_except_repo_build_agent(tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    repo.mkdir()
    unexpected = repo / "build-release"
    unexpected.mkdir()

    with pytest.raises(ToolchainError, match="Refusing to remove unexpected build directory"):
        safe_remove_build_tree(unexpected, repo)

    assert unexpected.exists()


def test_existing_fingerprint_json_is_compared_semantically(tmp_path: Path) -> None:
    left = tmp_path / "left.json"
    right = tmp_path / "right.json"
    left.write_text('{"schema":1,"files":{"b":"2","a":"1"}}', encoding="utf-8")
    right.write_text(json.dumps({"files": {"a": "1", "b": "2"}, "schema": 1}, indent=2), encoding="utf-8")

    assert fingerprint_matches(left, json.loads(right.read_text(encoding="utf-8")))
