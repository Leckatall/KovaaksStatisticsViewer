from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from typing import Any, Mapping

from build_runner.catalog import QML_ENVIRONMENT
from build_runner.config import Toolchain
from build_runner.orchestrator import _command_environment


DATASETS = ("rich", "single", "short", "bunched", "fixtures", "empty", "first-run")
DRIVER_EXE = "ksv_driver.exe"
_SESSION_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")


class SessionError(RuntimeError):
    pass


@dataclass(frozen=True)
class LaunchOptions:
    dataset: str = "rich"
    visible: bool = False
    size: str = "1280x800"
    compiled_qml: bool = False

    def to_json(self) -> dict[str, Any]:
        return {"dataset": self.dataset, "visible": self.visible, "size": self.size, "compiledQml": self.compiled_qml}

    @staticmethod
    def from_json(data: Mapping[str, Any]) -> LaunchOptions:
        return LaunchOptions(
            dataset=str(data.get("dataset", "rich")),
            visible=bool(data.get("visible", False)),
            size=str(data.get("size", "1280x800")),
            compiled_qml=bool(data.get("compiledQml", False)),
        )


@dataclass(frozen=True)
class Session:
    """A named driver session: .temp/ui-driver/<name>/ holding data, settings, shots and logs."""

    repo_root: Path
    name: str

    def __post_init__(self) -> None:
        if not _SESSION_NAME.match(self.name):
            raise SessionError(f"invalid session name {self.name!r} (use letters, digits, '.', '_' or '-')")

    @property
    def sessions_root(self) -> Path:
        return self.repo_root / ".temp" / "ui-driver"

    @property
    def directory(self) -> Path:
        return self.sessions_root / self.name

    @property
    def session_file(self) -> Path:
        return self.directory / "session.json"

    @property
    def launch_file(self) -> Path:
        return self.directory / "launch.json"

    @property
    def log_file(self) -> Path:
        return self.directory / "app.log"

    @property
    def shots_dir(self) -> Path:
        return self.directory / "shots"

    def read_info(self) -> dict[str, Any] | None:
        try:
            return json.loads(self.session_file.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None

    def read_launch_options(self) -> LaunchOptions | None:
        try:
            return LaunchOptions.from_json(json.loads(self.launch_file.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            return None

    def wipe(self) -> None:
        directory = self.directory.resolve()
        if self.sessions_root.resolve() not in directory.parents:
            raise SessionError(f"refusing to delete {directory}: not under {self.sessions_root}")
        if directory.exists():
            shutil.rmtree(directory)

    def next_shot_path(self, label: str) -> Path:
        self.shots_dir.mkdir(parents=True, exist_ok=True)
        numbers = [int(match.group(1)) for path in self.shots_dir.glob("*.png") if (match := re.match(r"^(\d+)-", path.name))]
        slug = re.sub(r"[^A-Za-z0-9]+", "-", label).strip("-")[:48] or "shot"
        return self.shots_dir / f"{max(numbers, default=0) + 1:03d}-{slug}.png"

    def log_tail(self, lines: int = 40) -> str:
        try:
            content = self.log_file.read_text(encoding="utf-8", errors="replace")
        except OSError:
            return "(no app.log)"
        return "\n".join(content.splitlines()[-lines:])


def pid_alive(pid: int) -> bool:
    if os.name != "nt":
        try:
            os.kill(pid, 0)
        except OSError:
            return False
        return True
    # os.kill(pid, 0) would TerminateProcess on Windows; query the exit code instead.
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel32.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
    process_query_limited_information = 0x1000
    still_active = 259
    handle = kernel32.OpenProcess(process_query_limited_information, False, pid)
    if not handle:
        return False
    try:
        code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
            return False
        return code.value == still_active
    finally:
        kernel32.CloseHandle(handle)


def launch_command(exe: Path, session: Session, options: LaunchOptions) -> list[str]:
    command = [
        str(exe),
        "--session-dir",
        str(session.directory),
        "--dataset",
        options.dataset,
        "--size",
        options.size,
    ]
    if options.compiled_qml:
        command.append("--compiled-qml")
    return command


def launch_environment(base: Mapping[str, str], toolchain: Toolchain, options: LaunchOptions) -> dict[str, str]:
    """The build runner's runtime PATH, plus the offscreen/software QPA when headless."""
    additions = {} if options.visible else dict(QML_ENVIRONMENT)
    environment = _command_environment(base, toolchain, additions)
    if options.visible:
        # A caller's headless environment must not leak into a visible run.
        for key in QML_ENVIRONMENT:
            environment.pop(key, None)
    return environment


def spawn(command: list[str], environment: Mapping[str, str], log_file: Path) -> subprocess.Popen:
    """Starts ksv_driver detached so it outlives the CLI invocation that launched it."""
    log_file.parent.mkdir(parents=True, exist_ok=True)
    log = open(log_file, "ab")
    kwargs: dict[str, Any] = {
        "stdin": subprocess.DEVNULL,
        "stdout": log,
        "stderr": subprocess.STDOUT,
        "env": dict(environment),
        "close_fds": True,
    }
    try:
        if os.name == "nt":
            base_flags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
            try:
                return subprocess.Popen(command, creationflags=base_flags | subprocess.CREATE_BREAKAWAY_FROM_JOB, **kwargs)
            except OSError:
                # The caller's job object forbids breakaway; detach as far as allowed.
                return subprocess.Popen(command, creationflags=base_flags, **kwargs)
        return subprocess.Popen(command, start_new_session=True, **kwargs)
    finally:
        log.close()


def wait_until_ready(session: Session, process: subprocess.Popen, timeout: float) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        info = session.read_info()
        if info is not None and info.get("pid") == process.pid:
            return info
        if process.poll() is not None:
            raise SessionError(
                f"ksv_driver exited with code {process.returncode} before it was ready. app.log tail:\n{session.log_tail()}"
            )
        time.sleep(0.2)
    raise SessionError(f"ksv_driver was not ready after {timeout:.0f}s. app.log tail:\n{session.log_tail()}")


def terminate(pid: int) -> None:
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, check=False)
    else:
        os.kill(pid, 15)
