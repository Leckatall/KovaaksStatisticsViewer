from __future__ import annotations

import json
from pathlib import Path

import pytest

from scripts.build_runner.config import (
    ToolchainError,
    assert_cache,
    fingerprint_matches,
    read_toolchain_env,
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
