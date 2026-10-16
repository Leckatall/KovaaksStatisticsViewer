from __future__ import annotations

from dataclasses import dataclass, field, fields
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
# A profile spec replaces the profile, so it only combines with these.
PROFILE_SPEC_DATASETS = ("empty", "first-run", "fixtures")
# --inject ports and their ops (tools/driver/driver_ports.cpp, FaultPlan::ports).
INJECT_PORTS = {
    "decoder": ("decode", "decode_file"),
    "csv": ("parseFile",),
    "benchmarkStore": ("scan", "write", "remove"),
    "seriesStore": ("load", "save"),
}
_INJECT_EFFECT = re.compile(r"^(delay:\d+|fail|fail:[1-9]\d*|fail:after:\d+)$")


def validate_injection(spec: str) -> None:
    """Checks one `--inject PORT[.OP]=EFFECT[,EFFECT...]` value the way ksv_driver will."""
    target, equals, effects = spec.partition("=")
    if not equals or not target:
        raise SessionError(f"--inject {spec!r} must look like PORT[.OP]=EFFECT[,EFFECT...]")
    port, _, op = target.partition(".")
    if port not in INJECT_PORTS:
        known = "; ".join(f"{name} ({', '.join(ops)})" for name, ops in INJECT_PORTS.items())
        raise SessionError(f"--inject {spec!r}: unknown port {port!r}. Ports and ops: {known}")
    if op and op not in INJECT_PORTS[port]:
        raise SessionError(f"--inject {spec!r}: {port} has no op {op!r} (ops: {', '.join(INJECT_PORTS[port])})")
    for effect in effects.split(","):
        if not _INJECT_EFFECT.match(effect.strip()):
            raise SessionError(f"--inject {spec!r}: unknown effect {effect!r} "
                               "(expected delay:MS, fail, fail:N with N >= 1, or fail:after:N)")
DRIVER_EXE = "ksv_driver.exe"
_SESSION_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")


class SessionError(RuntimeError):
    pass


def _option(default: Any, key: str, flag: str | None = None) -> Any:
    """A LaunchOptions field: `key` names it in launch.json, `flag` (if any) is its ksv_driver option.

    A str field passes `flag VALUE` (nothing when empty), a bool field passes
    `flag` when true, and a tuple field passes `flag VALUE` once per element, in order.
    """
    return field(default=default, metadata={"key": key, "flag": flag})


@dataclass(frozen=True)
class LaunchOptions:
    """What `start` launched with; stored in launch.json so `restart` reuses it."""

    dataset: str = _option("rich", "dataset", "--dataset")
    visible: bool = _option(False, "visible")
    size: str = _option("1280x800", "size", "--size")
    compiled_qml: bool = _option(False, "compiledQml", "--compiled-qml")
    overlays: tuple[str, ...] = _option((), "overlays", "--overlay")
    settings: tuple[str, ...] = _option((), "settings", "--setting")
    env: tuple[str, ...] = _option((), "env")
    profile_spec: str = _option("", "profileSpec", "--profile-spec")
    injections: tuple[str, ...] = _option((), "injections", "--inject")

    def to_json(self) -> dict[str, Any]:
        return {item.metadata["key"]: list(value) if isinstance(value := getattr(self, item.name), tuple) else value
                for item in fields(self)}

    @staticmethod
    def from_json(data: Mapping[str, Any]) -> LaunchOptions:
        values: dict[str, Any] = {}
        for item in fields(LaunchOptions):
            if item.metadata["key"] not in data:
                continue
            raw = data[item.metadata["key"]]
            if isinstance(item.default, tuple):
                values[item.name] = tuple(str(element) for element in raw or ())
            elif isinstance(item.default, bool):
                values[item.name] = bool(raw)
            else:
                values[item.name] = str(raw)
        return LaunchOptions(**values)

    def driver_arguments(self) -> list[str]:
        arguments: list[str] = []
        for item in fields(self):
            flag = item.metadata["flag"]
            value = getattr(self, item.name)
            if not flag:
                continue
            if isinstance(value, bool):
                arguments += [flag] if value else []
            elif isinstance(value, tuple):
                for element in value:
                    arguments += [flag, element]
            elif value:
                arguments += [flag, value]
        return arguments


def parse_assignment(text: str, option: str) -> tuple[str, str]:
    """'KEY=VALUE' -> (KEY, VALUE); VALUE may be empty."""
    key, equals, value = text.partition("=")
    if not equals or not key.strip():
        raise SessionError(f"{option} {text!r} must look like KEY=VALUE")
    return key.strip(), value


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

    @property
    def data_dir(self) -> Path:
        return self.directory / "data"

    def data_path(self, relative: str) -> Path:
        """`relative` under data/; refuses anything that resolves outside it."""
        root = self.data_dir.resolve()
        path = (root / relative).resolve()
        if path != root and root not in path.parents:
            raise SessionError(f"{relative!r} resolves outside the session's data/ directory")
        return path

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

    def next_shot_path(self, label: str, *, subdir: str = "", root: Path | None = None) -> Path:
        """root/[subdir/]NNN-<label>.png; NNN counts on across every subdirectory, so shots stay in order."""
        return next_shot_path(root or self.shots_dir, label, subdir=subdir)

    def log_tail(self, lines: int = 40) -> str:
        try:
            content = self.log_file.read_text(encoding="utf-8", errors="replace")
        except OSError:
            return "(no app.log)"
        return "\n".join(content.splitlines()[-lines:])


def shot_slug(label: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "-", label).strip("-")[:48] or "shot"


def next_shot_path(root: Path, label: str, *, subdir: str = "") -> Path:
    root.mkdir(parents=True, exist_ok=True)
    numbers = [int(match.group(1)) for path in root.rglob("*.png") if (match := re.match(r"^(\d+)-", path.name))]
    directory = root
    for part in Path(subdir).parts if subdir else ():
        if part in ("..", ".") or Path(part).is_absolute():
            raise SessionError(f"shot directory {subdir!r} must stay under {root}")
        directory = directory / shot_slug(part)
    directory.mkdir(parents=True, exist_ok=True)
    return directory / f"{max(numbers, default=0) + 1:03d}-{shot_slug(label)}.png"


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


def launch_command(exe: Path, session: Session, options: LaunchOptions, *, reapply: bool = False) -> list[str]:
    """ksv_driver's argv. `reapply` applies overlays/settings to a kept (already seeded) session too."""
    command = [str(exe), "--session-dir", str(session.directory), *options.driver_arguments()]
    if reapply:
        command.append("--reapply")
    return command


def launch_environment(base: Mapping[str, str], toolchain: Toolchain, options: LaunchOptions) -> dict[str, str]:
    """The build runner's runtime PATH, the offscreen/software QPA when headless, then --env (KEY= unsets)."""
    additions = {} if options.visible else dict(QML_ENVIRONMENT)
    environment = _command_environment(base, toolchain, additions)
    if options.visible:
        # A caller's headless environment must not leak into a visible run.
        for key in QML_ENVIRONMENT:
            environment.pop(key, None)
    for assignment in options.env:
        key, value = parse_assignment(assignment, "--env")
        if value:
            environment[key] = value
        else:
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
