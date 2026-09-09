from __future__ import annotations

import os
from pathlib import Path

import pytest

from scripts.build_runner.locking import BuildLockBusy, build_tree_lock


def test_second_acquisition_fails_fast(tmp_path: Path) -> None:
    lock = tmp_path / ".temp" / "build-agent.lock"
    with build_tree_lock(lock):
        with pytest.raises(BuildLockBusy):
            with build_tree_lock(lock):
                pass


def test_busy_message_names_the_holder(tmp_path: Path) -> None:
    lock = tmp_path / ".temp" / "build-agent.lock"
    with build_tree_lock(lock):
        with pytest.raises(BuildLockBusy, match=rf"pid {os.getpid()}\b"):
            with build_tree_lock(lock):
                pass


def test_lock_is_released_on_exit(tmp_path: Path) -> None:
    lock = tmp_path / ".temp" / "build-agent.lock"
    with build_tree_lock(lock):
        pass
    with build_tree_lock(lock):
        pass


def test_missing_parent_directory_is_created(tmp_path: Path) -> None:
    lock = tmp_path / "does-not-exist" / "build-agent.lock"
    with build_tree_lock(lock):
        assert lock.is_file()
