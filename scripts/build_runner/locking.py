from __future__ import annotations

from contextlib import contextmanager
import os
from pathlib import Path
from typing import Iterator

from .config import ToolchainError


class BuildLockBusy(ToolchainError):
    pass


def _holder_pid(lock_path: Path) -> str:
    try:
        return lock_path.read_text(encoding="utf-8").strip() or "unknown"
    except OSError:
        return "unknown"


if os.name == "nt":
    import msvcrt

    _LOCK_OFFSET = 1_000_000_000

    def _try_acquire(handle) -> bool:
        handle.seek(_LOCK_OFFSET)
        try:
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError:
            return False
        return True

    def _release(handle) -> None:
        handle.seek(_LOCK_OFFSET)
        msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
else:
    import fcntl

    def _try_acquire(handle) -> bool:
        try:
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            return False
        return True

    def _release(handle) -> None:
        fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


@contextmanager
def build_tree_lock(lock_path: Path) -> Iterator[None]:
    """Hold an exclusive OS lock on the shared build tree, or fail fast.

    The lock is advisory and tied to the open handle, so a crashed or killed
    holder releases it automatically — there is no stale-lock state to recover.
    """
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    handle = os.fdopen(os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o644), "r+", encoding="utf-8")
    try:
        if not _try_acquire(handle):
            raise BuildLockBusy(
                f"another build is in progress (pid {_holder_pid(lock_path)}); refusing to run"
            )
        handle.seek(0)
        handle.truncate()
        handle.write(str(os.getpid()))
        handle.flush()
        try:
            yield
        finally:
            _release(handle)
    finally:
        handle.close()
