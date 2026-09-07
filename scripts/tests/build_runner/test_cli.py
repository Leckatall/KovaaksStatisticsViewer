from __future__ import annotations

import pytest

from scripts.build_runner.cli import BUILD_JOBS, UsageError, parse_args


def test_cli_defaults_to_all_scope() -> None:
    args = parse_args([])

    assert args.scope == "all"
    assert not args.clean


def test_jobs_is_not_a_public_option() -> None:
    with pytest.raises(UsageError, match="unrecognized arguments: --jobs 4"):
        parse_args(["--jobs", "4"])


def test_match_requires_focused_scope() -> None:
    with pytest.raises(UsageError, match="--match requires a focused scope"):
        parse_args(["--match", "*Profile*"])


def test_build_only_rejects_list_and_match() -> None:
    with pytest.raises(UsageError, match="--build-only and --list cannot be combined"):
        parse_args(["--scope", "domain", "--build-only", "--list"])
    with pytest.raises(UsageError, match="--build-only and --match cannot be combined"):
        parse_args(["--scope", "domain", "--build-only", "--match", "*"])


def test_fixed_parallelism_is_eight() -> None:
    assert BUILD_JOBS == 8
