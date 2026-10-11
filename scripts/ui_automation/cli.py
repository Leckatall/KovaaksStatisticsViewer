"""ui_driver.py: launch KSV headlessly, screenshot it, and drive its UI.

Every subcommand except the lifecycle ones (start/stop/restart/status), the
session file commands (put/rm/edit) and the runners (batch/tour/report) is one
request to the running ksv_driver session. See tools/driver/README.md.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable, Sequence
from dataclasses import dataclass, field
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import time
from typing import Any

from build_runner.config import ToolchainError, resolve_toolchain

from . import formatting, report
from .client import DriverCommandError, DriverConnectionError, request
from .session import (
    DATASETS,
    DRIVER_EXE,
    PROFILE_SPEC_DATASETS,
    LaunchOptions,
    Session,
    SessionError,
    launch_command,
    launch_environment,
    next_shot_path,
    parse_assignment,
    pid_alive,
    spawn,
    terminate,
    validate_injection,
    wait_until_ready,
)


SCRIPTS_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = SCRIPTS_DIR.parent
DRIVER_TARGET = "ksv_driver"
TOURS_DIR = REPO_ROOT / "tools" / "driver" / "tours"
# Offscreen-QPA notices that say nothing about the app (UiDriver::execute drops them too).
QPA_CHATTER = "This plugin does not support"


class UsageError(RuntimeError):
    pass


class ArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:  # raise instead of exiting, so batch lines can fail softly
        raise UsageError(f"{self.prog}: {message}")


@dataclass
class Context:
    session: Session
    json_output: bool
    out: Callable[[str], None] = print
    # Where shots and index.jsonl go: the session's shots/ unless a tour redirects them.
    shots_root: Path | None = None
    # The default shots/ subdirectory (a batch's `section NAME`).
    shot_subdir: str = ""
    # The CLI line being run, recorded with each shot.
    command_line: str = ""
    # App warnings printed during the current step.
    step_warnings: list[str] = field(default_factory=list)

    @property
    def shots_dir(self) -> Path:
        return self.shots_root or self.session.shots_dir


# ---- value / target parsing ----------------------------------------------------


def parse_value(text: str) -> Any:
    """JSON when it parses (numbers, true, [1,2], {"a":1}, "quoted"), otherwise the raw string."""
    try:
        return json.loads(text)
    except ValueError:
        return text


def parse_point_or_selector(text: str) -> Any:
    """'120,40' is a window point; anything else is a selector."""
    parts = text.split(",")
    if len(parts) == 2:
        try:
            return [float(parts[0]), float(parts[1])]
        except ValueError:
            pass
    return text


def parse_size(text: str) -> tuple[int, int]:
    width, _, height = text.lower().partition("x")
    try:
        return int(width), int(height)
    except ValueError as error:
        raise UsageError(f"size must look like 1280x800, got {text!r}") from error


def _pointer_args(ns: argparse.Namespace) -> dict[str, Any]:
    args: dict[str, Any] = {}
    if ns.at is not None:
        args["point"] = list(ns.at)
    elif ns.selector:
        args["selector"] = ns.selector
    else:
        raise UsageError("give a selector or --at X Y")
    if ns.window:
        args["window"] = ns.window
    if getattr(ns, "offset", None) is not None:
        args["offset"] = list(ns.offset)
    return args


# ---- request builders (Namespace -> protocol command + args) --------------------


def build_click(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args = _pointer_args(ns)
    button = "right" if ns.command == "rclick" else ns.button
    if button and button != "left":
        args["button"] = button
    if ns.double or ns.command == "dblclick":
        args["double"] = True
    if ns.mods:
        args["modifiers"] = ns.mods
    return "click", args


def build_hover(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args = _pointer_args(ns)
    if ns.steps is not None:
        args["steps"] = ns.steps
    return "hover", args


def build_mouse(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    """mouse press|move|release: one stage of a gesture that can span commands (and shots)."""
    if ns.action == "release" and ns.at is None and not ns.selector:
        args: dict[str, Any] = {}
        if ns.window:
            args["window"] = ns.window
    else:
        args = _pointer_args(ns)
    args["action"] = ns.action
    if ns.button and ns.button != "left":
        args["button"] = ns.button
    if ns.mods:
        args["modifiers"] = ns.mods
    if ns.steps is not None:
        args["steps"] = ns.steps
    return "mouse", args


def build_drag(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args: dict[str, Any] = {"from": parse_point_or_selector(ns.source), "to": parse_point_or_selector(ns.destination)}
    if ns.steps:
        args["steps"] = ns.steps
    if ns.window:
        args["window"] = ns.window
    if ns.mods:
        args["modifiers"] = ns.mods
    return "drag", args


def build_scroll(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args = _pointer_args(ns)
    if not ns.dy and not ns.dx:
        raise UsageError("scroll needs --dy and/or --dx (wheel notches; positive = down/right)")
    if ns.dy:
        args["dy"] = ns.dy
    if ns.dx:
        args["dx"] = ns.dx
    if ns.mods:
        args["modifiers"] = ns.mods
    return "scroll", args


def build_key(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args: dict[str, Any] = {"sequence": ns.sequence}
    if ns.repeat > 1:
        args["repeat"] = ns.repeat
    if ns.window:
        args["window"] = ns.window
    return "key", args


def build_type(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args: dict[str, Any] = {"text": ns.text}
    if ns.into:
        args["selector"] = ns.into
    if ns.clear:
        args["clear"] = True
    if ns.at_start:
        args["atStart"] = True
    if ns.enter:
        args["enter"] = True
    return "type", args


def build_get(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    return "get", {"selector": ns.selector, "property": ns.property, **({"all": True} if ns.all else {})}


def build_set(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    return "set", {"selector": ns.selector, "property": ns.property, "value": parse_value(ns.value),
                   **({"all": True} if ns.all else {})}


def build_invoke(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    return "invoke", {"selector": ns.selector, "method": ns.method, "args": [parse_value(a) for a in ns.args],
                      **({"all": True} if ns.all else {})}


def build_eval(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args: dict[str, Any] = {"expression": ns.expression}
    if ns.on:
        args["selector"] = ns.on
    return "eval", args


def build_wait(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    if not ns.selector and not ns.expr:
        raise UsageError("wait needs a selector and/or --expr")
    if ns.prop and ns.equals is None:
        raise UsageError("--prop needs --equals")
    args: dict[str, Any] = {"timeoutMs": ns.timeout}
    if ns.selector:
        args["selector"] = ns.selector
    if ns.prop:
        args["property"] = ns.prop
        args["equals"] = parse_value(ns.equals)
    if ns.expr:
        args["expression"] = ns.expr
    if ns.gone:
        args["gone"] = True
    if ns.all:
        args["all"] = True
    return "wait", args


def build_tree(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args: dict[str, Any] = {}
    if ns.selector:
        args["selector"] = ns.selector
    if ns.depth is not None:
        args["depth"] = ns.depth
    if ns.all:
        args["all"] = True
    if ns.raw:
        args["raw"] = True
    return "tree", args


def build_find(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args: dict[str, Any] = {"selector": ns.selector, "limit": ns.limit}
    if ns.all:
        args["all"] = True
    if ns.objects:
        args["objects"] = True
    return "find", args


def build_resize(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    width, height = parse_size(ns.size)
    args: dict[str, Any] = {"width": width, "height": height}
    if ns.window:
        args["window"] = ns.window
    return "resize", args


def build_logs(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    args: dict[str, Any] = {}
    if ns.all:
        args["all"] = True
    if ns.warnings:
        args["warningsOnly"] = True
    if ns.clear:
        args["clear"] = True
    return "logs", args


def build_windows(ns: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    return "windows", {}


# ---- result renderers ----------------------------------------------------------


def _target_text(target: Any) -> str:
    if isinstance(target, dict) and target.get("destroyed"):
        return "(the target no longer exists)"
    if isinstance(target, dict):
        return formatting.describe(target)
    if isinstance(target, list):
        return f"point {target[0]:g},{target[1]:g}"
    return str(target)


def render(command: str, result: dict[str, Any]) -> list[str]:
    if command == "windows":
        return [formatting.window_line(window) for window in result.get("windows", [])]
    if command == "tree":
        lines = formatting.tree_lines(result.get("nodes", []))
        if result.get("truncated"):
            lines.append("(truncated; narrow it with a selector or --depth)")
        return lines or ["(no visible items)"]
    if command == "find":
        lines = [f"{result.get('count', 0)} match(es)"]
        lines += [formatting.describe(record) for record in result.get("matches", [])]
        return lines
    if command in ("click", "hover", "scroll", "mouse"):
        verb = {"click": "clicked", "hover": "hovered", "scroll": "scrolled"}.get(command)
        if command == "mouse":
            verb = {"press": "pressed", "move": "moved to", "release": "released"}.get(result.get("action", ""),
                                                                                      result.get("action", "mouse"))
            if result.get("action") == "releaseAll":
                return ["released every held button"]
        pos = result.get("pos")
        where = f" at {pos[0]},{pos[1]}" if pos else ""
        lines = [f"{verb} {_target_text(result.get('target'))}{where}"]
        if "after" in result:
            lines.append(f"  after: {_target_text(result['after'])}")
        if command == "mouse":
            lines.append(f"  held: {', '.join(result.get('held', [])) or 'none'}")
        return lines
    if command == "drag":
        return [f"dragged {_target_text(result.get('from'))} -> {_target_text(result.get('to'))}"]
    if command in ("key", "type"):
        focus = result.get("focus")
        return [f"focus: {formatting.describe(focus)}" if focus else f"sent to window {result.get('window')}"]
    if command in ("get", "set", "invoke", "eval"):
        return [formatting.value_text(result.get("value"))]
    if command == "wait":
        return [f"ok after {result.get('elapsedMs')} ms" + (f": {formatting.value_text(result['value'])}" if result.get("value") is not None else "")]
    if command == "resize":
        return [formatting.window_line(result.get("window", {}))]
    if command == "logs":
        return [formatting.log_line(entry) for entry in result.get("entries", [])] or ["(no new log entries)"]
    if command == "screenshot":
        lines = [result.get("path", "")]
        if result.get("context"):
            lines.append(f"composited over window {result['context']}")
        if result.get("otherVisibleWindows"):
            lines.append("other visible windows: " + ", ".join(result["otherVisibleWindows"]) + " (shot --window KEY)")
        for dialog in result.get("uncapturedDialogs", []):
            lines.append(f"warning: dialog {dialog} is open but not in this image (it has no window of its own)")
        return lines
    return [json.dumps(result, ensure_ascii=False)]


# ---- talking to the session ----------------------------------------------------


def live_info(session: Session) -> dict[str, Any]:
    info = session.read_info()
    if not info or not pid_alive(int(info.get("pid", 0))):
        raise SessionError(
            f"no running ksv_driver session '{session.name}'. Start one with: python scripts/ui_driver.py start"
        )
    return info


def call(ctx: Context, command: str, args: dict[str, Any]) -> dict[str, Any]:
    info = live_info(ctx.session)
    timeout = None
    if command == "wait":
        timeout = args.get("timeoutMs", 5000) / 1000 + 30
    return request(int(info["port"]), command, args, timeout=timeout)


def emit_result(ctx: Context, command: str, result: dict[str, Any]) -> None:
    warnings = formatting.warning_lines(result)
    ctx.step_warnings.extend(warnings)
    if ctx.json_output:
        ctx.out(json.dumps(result, ensure_ascii=False))
        return
    for line in render(command, result):
        ctx.out(line)
    for line in warnings:
        ctx.out(line)


INDEX_FILE = "index.jsonl"


def record_shot(ctx: Context, path: Path, result: dict[str, Any], *, label: str, subdir: str,
                warnings: Sequence[str]) -> None:
    """Appends the shot to <shots>/index.jsonl (read by `report`); shots saved elsewhere with -o are not indexed."""
    root = ctx.shots_dir.resolve()
    resolved = path.resolve()
    if root not in resolved.parents:
        return
    entry = {
        "path": resolved.relative_to(root).as_posix(),
        "label": label,
        "dir": subdir,
        "window": result.get("window"),
        "size": result.get("size"),
        "command": ctx.command_line,
        "time": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "warnings": list(warnings),
    }
    with open(root / INDEX_FILE, "a", encoding="utf-8") as index:
        index.write(json.dumps(entry, ensure_ascii=False) + "\n")


def take_screenshot(ctx: Context, *, window: str | None = None, item: str | None = None, pad: int | None = None,
                    output: str | None = None, label: str | None = None, with_context: bool = False,
                    subdir: str | None = None, warnings: Sequence[str] = ()) -> dict[str, Any]:
    args: dict[str, Any] = {}
    if item:
        args["selector"] = item
    elif window:
        args["window"] = window
    if pad is not None:
        args["pad"] = pad
    if with_context:
        args["withContext"] = True
    label = label or item or window or "main"
    subdir = ctx.shot_subdir if subdir is None else subdir
    path = Path(output) if output else next_shot_path(ctx.shots_dir, label, subdir=subdir)
    args["path"] = str(path.resolve())
    result = call(ctx, "screenshot", args)
    record_shot(ctx, path, result, label=label, subdir=subdir,
                warnings=[*warnings, *formatting.warning_lines(result)])
    return result


def run_protocol(ns: argparse.Namespace, ctx: Context) -> int:
    command, args = ns.build(ns)
    if getattr(ns, "settle", None) is not None:
        args["settleMs"] = ns.settle
    result = call(ctx, command, args)
    emit_result(ctx, command, result)
    if getattr(ns, "shot", False):
        # The window the action happened in: its target's, else the one it reports
        # (key, type, mouse), else --window.
        target = result.get("target") or result.get("to") or {}
        window = (target.get("window") if isinstance(target, dict) else None) or result.get("window") \
            or getattr(ns, "window", None)
        if isinstance(window, dict):  # resize reports the whole window record
            window = window.get("key")
        label = ns.shot_name or f"{ns.command}-{getattr(ns, 'selector', '') or ''}"
        shot = take_screenshot(ctx, window=window, label=label, subdir=ns.shot_dir,
                               warnings=formatting.warning_lines(result))
        emit_result(ctx, "screenshot", shot)
    return 0


def handle_shot(ns: argparse.Namespace, ctx: Context) -> int:
    if ns.all_windows:
        windows = call(ctx, "windows", {}).get("windows", [])
        for window in windows:
            if window.get("visible") and window.get("capturable", True):
                label = f"{ns.name}-{window['key']}" if ns.name else window["key"]
                emit_result(ctx, "screenshot", take_screenshot(ctx, window=window["key"], label=label, subdir=ns.dir))
        return 0
    result = take_screenshot(ctx, window=ns.window, item=ns.item, pad=ns.pad, output=ns.output,
                             with_context=ns.with_context, label=ns.name, subdir=ns.dir)
    emit_result(ctx, "screenshot", result)
    return 0


# ---- lifecycle -----------------------------------------------------------------


def driver_exe() -> Path:
    return REPO_ROOT / "build-agent" / DRIVER_EXE


def stale_binary_warning(exe: Path) -> str | None:
    """ksv_driver loads QML from source, but C++ edits need a rebuild."""
    try:
        built = exe.stat().st_mtime
    except OSError:
        return None
    for root in (REPO_ROOT / "src", REPO_ROOT / "tools" / "driver", REPO_ROOT / "tools" / "gallery"):
        for pattern in ("*.cpp", "*.h"):
            for source in root.rglob(pattern):
                if source.stat().st_mtime > built:
                    return f"note: {source.relative_to(REPO_ROOT).as_posix()} is newer than {DRIVER_EXE}; pass --build to rebuild"
    return None


def run_build(ctx: Context) -> int:
    ctx.out(f"building {DRIVER_TARGET} ...")
    command = [sys.executable, str(SCRIPTS_DIR / "build_and_test.py"), "--build-only", "--target", DRIVER_TARGET]
    return subprocess.run(command, cwd=REPO_ROOT, check=False).returncode


def release_held_buttons(port: int) -> None:
    """Ends any gesture a `mouse press` left open."""
    request(port, "mouse", {"action": "releaseAll", "settleMs": 0}, timeout=10)


def stop_session(ctx: Context, *, quiet: bool = False) -> bool:
    info = ctx.session.read_info()
    pid = int(info.get("pid", 0)) if info else 0
    if not pid or not pid_alive(pid):
        if not quiet:
            ctx.out(f"session '{ctx.session.name}' is not running")
        return False
    try:
        release_held_buttons(int(info["port"]))
        request(int(info["port"]), "quit", {}, timeout=5)
    except (DriverConnectionError, DriverCommandError):
        pass
    deadline = time.monotonic() + 10
    while pid_alive(pid) and time.monotonic() < deadline:
        time.sleep(0.1)
    if pid_alive(pid):
        terminate(pid)
    ctx.out(f"stopped session '{ctx.session.name}' (pid {pid})")
    return True


def start_session(ctx: Context, options: LaunchOptions, *, build: bool, keep: bool, timeout: float,
                  reapply: bool = False) -> int:
    session = ctx.session
    stop_session(ctx, quiet=True)
    if build and (code := run_build(ctx)) != 0:
        return code
    exe = driver_exe()
    if not exe.exists():
        raise SessionError(
            f"{exe} not found. Build it with --build, or: python scripts/build_and_test.py --build-only --target {DRIVER_TARGET}"
        )
    if not build and (warning := stale_binary_warning(exe)):
        ctx.out(warning)
    toolchain = resolve_toolchain(SCRIPTS_DIR)
    if not keep:
        for attempt in range(5):  # files can stay locked briefly after the old process exits
            try:
                session.wipe()
                break
            except OSError:
                if attempt == 4:
                    raise
                time.sleep(0.5)
    session.directory.mkdir(parents=True, exist_ok=True)
    session.session_file.unlink(missing_ok=True)
    session.launch_file.write_text(json.dumps(options.to_json(), indent=2), encoding="utf-8")
    process = spawn(launch_command(exe, session, options, reapply=reapply),
                    launch_environment(os.environ, toolchain, options), session.log_file)
    info = wait_until_ready(session, process, timeout)
    startup_warnings = [f"app {entry.get('type', 'warning')} (startup): {entry.get('message', '')}"
                        for entry in request(int(info["port"]), "logs", {"all": True, "warningsOnly": True},
                                             timeout=10).get("entries", [])
                        if not str(entry.get("message", "")).startswith(QPA_CHATTER)]
    ctx.step_warnings.extend(startup_warnings)
    if ctx.json_output:
        ctx.out(json.dumps(info))
        return 0
    for line in startup_warnings:
        ctx.out(line)
    mode = "visible" if options.visible else f"headless ({info.get('platform')})"
    qml = "compiled module" if info.get("qmlSource") == "compiled" else "src/ui/qml (restart picks up QML edits)"
    ctx.out(f"session '{session.name}' ready: pid {info.get('pid')}, port {info.get('port')}, {mode}, "
            f"dataset {info.get('dataset')}, QML from {qml}")
    if not info.get("profileLoaded"):
        ctx.out("note: the profile was still loading when the driver came up")
    ctx.out(f"screenshots: {session.shots_dir.relative_to(REPO_ROOT).as_posix()}/   log: {session.log_file.relative_to(REPO_ROOT).as_posix()}")
    return 0


def resolve_overlays(directories: Sequence[str]) -> tuple[str, ...]:
    resolved = []
    for directory in directories:
        path = Path(directory).resolve()
        if not path.is_dir():
            raise UsageError(f"--overlay {directory!r} is not a directory")
        resolved.append(str(path))
    return tuple(resolved)


def start_options(ns: argparse.Namespace) -> LaunchOptions:
    """Validates and resolves `start`'s options (before anything is launched)."""
    parse_size(ns.size)
    for assignment in ns.setting:
        parse_assignment(assignment, "--setting")
    for assignment in ns.env:
        parse_assignment(assignment, "--env")
    for injection in ns.inject:
        validate_injection(injection)
    profile_spec = ""
    if ns.profile_spec:
        spec = Path(ns.profile_spec).resolve()
        if not spec.is_file():
            raise UsageError(f"--profile-spec {ns.profile_spec!r} is not a file")
        profile_spec = str(spec)
    # A spec replaces the profile, so without an explicit dataset it starts from empty.
    dataset = ns.dataset or ("empty" if profile_spec else "rich")
    if profile_spec and dataset not in PROFILE_SPEC_DATASETS:
        raise UsageError(f"--profile-spec replaces the profile, so it combines only with --dataset "
                         f"{', '.join(PROFILE_SPEC_DATASETS)}, not {dataset!r}")
    return LaunchOptions(dataset=dataset, visible=ns.visible, size=ns.size, compiled_qml=ns.compiled_qml,
                         overlays=resolve_overlays(ns.overlay), settings=tuple(ns.setting), env=tuple(ns.env),
                         profile_spec=profile_spec, injections=tuple(ns.inject))


def handle_start(ns: argparse.Namespace, ctx: Context) -> int:
    options = start_options(ns)
    # A kept session only gets overlays/settings again when they are passed explicitly.
    reapply = ns.keep and bool(options.overlays or options.settings)
    if ns.keep and options.profile_spec:
        ctx.out("note: --keep keeps the session's profile; --profile-spec applies only to a fresh session")
    return start_session(ctx, options, build=ns.build, keep=ns.keep, timeout=ns.timeout, reapply=reapply)


def handle_restart(ns: argparse.Namespace, ctx: Context) -> int:
    options = ctx.session.read_launch_options() or LaunchOptions()
    return start_session(ctx, options, build=ns.build, keep=True, timeout=ns.timeout)


# ---- session data files (put / rm / edit) --------------------------------------

DATA_FOLDERS = {
    "performances": "FPSAimTrainer/performances",
    "stats": "FPSAimTrainer/stats",
    "benchmarks": "benchmarks",
    ".": ".",
}
_FOLDER_BY_SUFFIX = {".perf": "performances", ".csv": "stats", ".json": "benchmarks"}
QUIET_SECONDS = 0.3


def wait_for_app_quiet(ctx: Context, timeout: float) -> None:
    """Waits until the app has processed a data change: no profile build in progress and
    no new log entries for QUIET_SECONDS, capped by `timeout`. Skipped when the session is not running."""
    info = ctx.session.read_info()
    if not info or not pid_alive(int(info.get("pid", 0))):
        ctx.out(f"note: session '{ctx.session.name}' is not running; the change applies on its next start")
        return
    port = int(info["port"])
    started = time.monotonic()
    last_seq: Any = None
    quiet_since = started
    while True:
        now = time.monotonic()
        seq = request(port, "ping", {}, timeout=10).get("logSeq")
        if seq != last_seq:
            last_seq, quiet_since = seq, now
        busy = request(port, "eval", {"expression": "sessionVm.profileBuildInProgress", "settleMs": 0},
                       timeout=10).get("value")
        if busy:
            quiet_since = now
        if now - quiet_since >= QUIET_SECONDS and now - started >= QUIET_SECONDS:
            ctx.out(f"app settled after {int((now - started) * 1000)} ms")
            return
        if now - started >= timeout:
            ctx.out(f"note: the app was still busy after {timeout:g}s")
            return
        time.sleep(0.05)


def _data_relative(ctx: Context, path: Path) -> str:
    return path.relative_to(ctx.session.data_dir.resolve()).as_posix()


def handle_put(ns: argparse.Namespace, ctx: Context) -> int:
    sources = [Path(source) for source in ns.sources]
    for source in sources:
        if not source.is_file():
            raise UsageError(f"put: {source} is not a file")
    for source in sources:
        folder = ns.to or _FOLDER_BY_SUFFIX.get(source.suffix.lower())
        if folder is None:
            raise UsageError(f"put: cannot tell where {source.name} goes; pass --to "
                             f"({', '.join(DATA_FOLDERS)} or a path under data/)")
        destination = ctx.session.data_path(DATA_FOLDERS.get(folder, folder)) / source.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        ctx.out(f"put {_data_relative(ctx, destination)}")
    wait_for_app_quiet(ctx, ns.timeout)
    return 0


def handle_rm(ns: argparse.Namespace, ctx: Context) -> int:
    targets = [ctx.session.data_path(path) for path in ns.paths]
    for relative, target in zip(ns.paths, targets):
        if not target.is_file():
            raise UsageError(f"rm: {relative} is not a file under data/")
    for target in targets:
        target.unlink()
        ctx.out(f"removed {_data_relative(ctx, target)}")
    wait_for_app_quiet(ctx, ns.timeout)
    return 0


def handle_edit(ns: argparse.Namespace, ctx: Context) -> int:
    target = ctx.session.data_path(ns.path)
    if not target.is_file():
        raise UsageError(f"edit: {ns.path} is not a file under data/")
    if bool(ns.replace) == bool(ns.from_file):
        raise UsageError("edit needs --replace OLD NEW (repeatable) or --from SRC")
    if ns.from_file:
        source = Path(ns.from_file)
        if not source.is_file():
            raise UsageError(f"edit: --from {source} is not a file")
        content = source.read_bytes()
    else:
        text = target.read_text(encoding="utf-8")
        for old, new in ns.replace:
            if old not in text:
                raise UsageError(f"edit: {old!r} does not occur in {ns.path}")
            text = text.replace(old, new)
        content = text.encode("utf-8")
    target.write_bytes(content)
    ctx.out(f"edited {_data_relative(ctx, target)}")
    wait_for_app_quiet(ctx, ns.timeout)
    return 0


def handle_stop(ns: argparse.Namespace, ctx: Context) -> int:
    stop_session(ctx)
    return 0


def handle_status(ns: argparse.Namespace, ctx: Context) -> int:
    info = ctx.session.read_info()
    if not info or not pid_alive(int(info.get("pid", 0))):
        ctx.out(f"session '{ctx.session.name}' is not running")
        return 1
    ping = request(int(info["port"]), "ping", {}, timeout=10)
    windows = request(int(info["port"]), "windows", {}, timeout=10).get("windows", [])
    if ctx.json_output:
        ctx.out(json.dumps({"session": info, "ping": ping, "windows": windows}))
        return 0
    ctx.out(f"session '{ctx.session.name}' running: pid {info['pid']}, port {info['port']}, "
            f"platform {ping.get('platform')}, dataset {info.get('dataset')}")
    ctx.out(f"directory: {ctx.session.directory}")
    if options := ctx.session.read_launch_options():
        if options.profile_spec:
            ctx.out(f"  profile spec: {options.profile_spec}")
        for label, values in (("overlay", options.overlays), ("setting", options.settings), ("env", options.env)):
            for value in values:
                ctx.out(f"  {label}: {value}")
    for injection in info.get("injections", []):
        ctx.out(f"  inject: {injection}")
    for window in windows:
        ctx.out("  " + formatting.window_line(window))
    return 0


@dataclass
class BatchOutcome:
    steps: int = 0
    failures: list[dict[str, Any]] = field(default_factory=list)  # {"line", "step", "error"}

    def to_json(self) -> dict[str, Any]:
        return {"steps": self.steps, "failed": len(self.failures), "failures": self.failures}


_VARIABLE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")
_LIFECYCLE = ("start", "stop", "restart")


def substitute(line: str, variables: dict[str, str]) -> str:
    """Replaces ${NAME} with a variable from `set NAME=value` (or a predefined one)."""
    def replace(match: re.Match[str]) -> str:
        if match.group(1) not in variables:
            raise UsageError(f"unknown variable ${{{match.group(1)}}} (known: {', '.join(sorted(variables)) or 'none'})")
        return variables[match.group(1)]
    return _VARIABLE.sub(replace, line)


def batch_variables(ctx: Context, batch_file: Path | None) -> dict[str, str]:
    directory = batch_file.resolve().parent if batch_file else Path.cwd()
    return {"BATCH_DIR": directory.as_posix(), "REPO": REPO_ROOT.as_posix(), "SESSION": ctx.session.name}


def run_batch(text: str, ctx: Context, *, keep_going: bool, fail_on_warnings: bool,
              variables: dict[str, str]) -> BatchOutcome:
    """Runs batch lines: CLI steps or JSON requests, plus the directives
    `set NAME=value`, `section NAME`, `options [--keep-going] [--fail-on-warnings]`
    and the lifecycle steps `start ...`/`restart`/`stop`, which act on the batch's session."""
    outcome = BatchOutcome()
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        ctx.out(f"> {line}")
        ctx.step_warnings = []
        try:
            line = substitute(line, variables)
            if line.startswith("{"):
                outcome.steps += 1
                step = json.loads(line)
                result = call(ctx, step["cmd"], step.get("args", {}))
                emit_result(ctx, step["cmd"], result)
            else:
                words = shlex.split(line)
                if words[0] == "set":
                    name, value = parse_assignment(line[len("set"):].strip(), "set")
                    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
                        raise UsageError(f"set: {name!r} is not a valid variable name")
                    variables[name] = value
                    continue
                if words[0] == "section":
                    ctx.shot_subdir = words[1] if len(words) > 1 else ""
                    continue
                if words[0] == "options":
                    for option in words[1:]:
                        if option == "--keep-going":
                            keep_going = True
                        elif option == "--fail-on-warnings":
                            fail_on_warnings = True
                        else:
                            raise UsageError(f"options: unknown option {option!r} (--keep-going, --fail-on-warnings)")
                    continue
                outcome.steps += 1
                ctx.command_line = line
                if words[0] in _LIFECYCLE:
                    step_ns = parse_args(words)
                    if step_ns.session or step_ns.global_session:
                        raise UsageError(f"{words[0]} in a batch runs in the batch's session '{ctx.session.name}'; "
                                         "drop --session")
                else:
                    step_ns = parse_args(words, batch=True)
                code = step_ns.handler(step_ns, ctx)
                if code:
                    raise UsageError(f"step exited with {code}")
            if fail_on_warnings and ctx.step_warnings:
                raise UsageError(f"{len(ctx.step_warnings)} app warning(s) and --fail-on-warnings is on")
        except Exception as error:  # noqa: BLE001 -- report and decide whether to continue
            messages = describe_error(error)
            for message in messages:
                ctx.out(message)
            outcome.failures.append({"line": number, "step": line, "error": messages[0].removeprefix("error: ")})
            if not keep_going:
                ctx.out(f"batch stopped at line {number}")
                break
    return outcome


def print_batch_summary(ctx: Context, outcome: BatchOutcome) -> None:
    ctx.out(f"batch: {outcome.steps} step(s), {len(outcome.failures)} failed")
    for failure in outcome.failures:
        ctx.out(f"  line {failure['line']}: {failure['step']}")
        ctx.out(f"    {failure['error']}")


def handle_batch(ns: argparse.Namespace, ctx: Context) -> int:
    batch_file = None if ns.file == "-" else Path(ns.file)
    text = sys.stdin.read() if batch_file is None else batch_file.read_text(encoding="utf-8")
    variables = batch_variables(ctx, batch_file)
    for assignment in ns.var:
        name, value = parse_assignment(assignment, "--var")
        variables[name] = value
    outcome = run_batch(text, ctx, keep_going=ns.keep_going, fail_on_warnings=ns.fail_on_warnings,
                        variables=variables)
    release_after_batch(ctx)
    print_batch_summary(ctx, outcome)
    return 1 if outcome.failures else 0


def tour_output(name: str) -> Path:
    return REPO_ROOT / ".temp" / "ui-driver" / "tours" / name


def handle_tour(ns: argparse.Namespace, ctx: Context) -> int:
    """Runs a checked-in tour in its own session. Shots, the batch summary and the
    report land in .temp/ui-driver/tours/NAME/ (wiped first), so `start` lines inside
    the tour can re-seed the session without losing earlier shots."""
    available = sorted(path.stem for path in TOURS_DIR.glob("*.txt"))
    if not ns.name:
        ctx.out("tours: " + (", ".join(available) or "(none)"))
        return 0
    tour_file = TOURS_DIR / f"{ns.name}.txt"
    if not tour_file.is_file():
        raise UsageError(f"no tour {ns.name!r} in {TOURS_DIR.relative_to(REPO_ROOT).as_posix()} "
                         f"(tours: {', '.join(available)})")
    output = tour_output(ns.name)
    if output.exists():
        shutil.rmtree(output)
    (output / "shots").mkdir(parents=True)
    coverage = TOURS_DIR / ns.name / "coverage.json"
    if coverage.is_file():
        shutil.copyfile(coverage, output / "coverage.json")

    tour_ctx = Context(session=Session(REPO_ROOT, f"tour-{ns.name}"), json_output=False, out=ctx.out,
                       shots_root=output / "shots")
    variables = batch_variables(tour_ctx, tour_file)
    variables["TOUR_DATA"] = (TOURS_DIR / ns.name).as_posix()
    started = time.monotonic()
    outcome = run_batch(tour_file.read_text(encoding="utf-8"), tour_ctx, keep_going=not ns.stop_on_failure,
                        fail_on_warnings=ns.fail_on_warnings, variables=variables)
    release_after_batch(tour_ctx)
    if not ns.keep_running:
        stop_session(tour_ctx, quiet=True)
    seconds = time.monotonic() - started
    summary = {"tour": ns.name, "seconds": round(seconds, 1), **outcome.to_json()}
    (output / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print_batch_summary(ctx, outcome)
    ctx.out(f"tour {ns.name}: {seconds:.1f}s, shots in {output.relative_to(REPO_ROOT).as_posix()}/shots/")
    if ns.report:
        html = report.write_report(output / "shots", output / "index.html", title=f"Tour: {ns.name}",
                                   coverage=output / "coverage.json", summary=summary)
        ctx.out(f"report: {html}")
    return 1 if outcome.failures else 0


def handle_report(ns: argparse.Namespace, ctx: Context) -> int:
    directory = Path(ns.directory) if ns.directory else ctx.shots_dir
    if not directory.is_dir():
        raise UsageError(f"report: {directory} is not a directory")
    coverage = Path(ns.coverage) if ns.coverage else next(
        (path for path in (directory / "coverage.json", directory.parent / "coverage.json") if path.is_file()), None)
    if ns.coverage and not coverage.is_file():
        raise UsageError(f"report: --coverage {ns.coverage} is not a file")
    summary_file = next((path for path in (directory / "summary.json", directory.parent / "summary.json")
                         if path.is_file()), None)
    summary = json.loads(summary_file.read_text(encoding="utf-8")) if summary_file else None
    output = Path(ns.output) if ns.output else directory / "index.html"
    html = report.write_report(directory, output, title=ns.title or f"Shots: {directory.name}", coverage=coverage,
                               summary=summary, embed=not ns.link)
    ctx.out(str(html))
    return 0


def release_after_batch(ctx: Context) -> None:
    """A batch that ends mid-gesture must not leave a button held."""
    info = ctx.session.read_info()
    if info and pid_alive(int(info.get("pid", 0))):
        try:
            release_held_buttons(int(info["port"]))
        except (DriverConnectionError, DriverCommandError):
            pass


# ---- argument parsing ----------------------------------------------------------


def _add_pointer_arguments(parser: argparse.ArgumentParser, *, offset: bool = True) -> None:
    parser.add_argument("selector", nargs="?", help="item selector (see README); or use --at")
    parser.add_argument("--at", nargs=2, type=float, metavar=("X", "Y"), help="window coordinates instead of a selector")
    parser.add_argument("--window", help="window key for --at (default: main)")
    if offset:
        parser.add_argument("--offset", nargs=2, type=float, metavar=("DX", "DY"),
                            help="point relative to the item's top-left instead of its center")


def _add_action_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--shot", action="store_true",
                        help="screenshot the window after the action; --shot=LABEL names it shots/NNN-LABEL.png")
    parser.add_argument("--shot-name", help=argparse.SUPPRESS)  # set by --shot=LABEL (see parse_args)
    parser.add_argument("--shot-dir", metavar="SUBDIR", help="save that screenshot under shots/SUBDIR/")
    parser.add_argument("--settle", type=int, metavar="MS", help="wait after the action (default 120)")


def build_parser(*, batch: bool = False) -> ArgumentParser:
    common = ArgumentParser(add_help=False)
    common.add_argument("--session", help="session name (default: default)")
    common.add_argument("--json", action="store_true", help="print raw JSON results")

    parser = ArgumentParser(
        prog="ui_driver.py",
        description="Run KSV headlessly and screenshot/drive its UI. See tools/driver/README.md.",
    )
    # Accepted before the subcommand too; separate dests because subparser
    # defaults would otherwise overwrite them.
    parser.add_argument("--session", dest="global_session", help=argparse.SUPPRESS)
    parser.add_argument("--json", dest="global_json", action="store_true", help=argparse.SUPPRESS)
    commands = parser.add_subparsers(dest="command", required=True, parser_class=ArgumentParser)

    def add(name: str, help_text: str, **kwargs: Any) -> argparse.ArgumentParser:
        return commands.add_parser(name, help=help_text, description=help_text, parents=[common], **kwargs)

    if not batch:
        start = add("start", "launch a fresh session (stops a running one first)")
        start.add_argument("--dataset", choices=DATASETS,
                           help="seed data (default: rich, or empty with --profile-spec)")
        start.add_argument("--visible", action="store_true", help="show a real window instead of running headless")
        start.add_argument("--size", default="1280x800", help="main window size, WxH")
        start.add_argument("--build", action="store_true", help=f"build {DRIVER_TARGET} first")
        start.add_argument("--keep", action="store_true", help="keep the session's existing data and settings")
        start.add_argument("--compiled-qml", action="store_true", help="use the compiled QML module, not src/ui/qml")
        start.add_argument("--timeout", type=float, default=90.0, help="seconds to wait for readiness")
        start.add_argument("--overlay", action="append", default=[], metavar="DIR",
                           help="copy DIR's tree onto the session's data/ after seeding (repeatable, in order)")
        start.add_argument("--setting", action="append", default=[], metavar="KEY=VALUE",
                           help="write a QSettings key before the app starts (repeatable; VALUE is JSON when it "
                                "parses; KEY= removes it)")
        start.add_argument("--env", action="append", default=[], metavar="KEY=VALUE",
                           help="environment variable for ksv_driver (repeatable; overrides the headless "
                                "defaults; KEY= unsets)")
        start.add_argument("--profile-spec", metavar="FILE",
                           help="build profile.pb (and a benchmark) from a JSON spec; see README")
        start.add_argument("--inject", action="append", default=[], metavar="PORT[.OP]=EFFECT",
                           help="fault/latency injection, e.g. benchmarkStore.write=fail or decoder=delay:300 "
                                "(repeatable)")
        start.set_defaults(handler=handle_start)

        restart = add("restart", "restart with the same options, keeping data (picks up QML edits)")
        restart.add_argument("--build", action="store_true", help=f"build {DRIVER_TARGET} first (C++ changes)")
        restart.add_argument("--timeout", type=float, default=90.0)
        restart.set_defaults(handler=handle_restart)

        add("stop", "stop the session").set_defaults(handler=handle_stop)
        add("status", "show whether the session runs, and its windows").set_defaults(handler=handle_status)

        batch_parser = add("batch", "run steps from a file (or - for stdin): CLI lines, JSON requests, and the "
                                    "directives set/section/options/start/stop (see README)")
        batch_parser.add_argument("file")
        batch_parser.add_argument("--keep-going", action="store_true",
                                  help="run every step; the summary lists failures and the exit code is still 1")
        batch_parser.add_argument("--fail-on-warnings", action="store_true",
                                  help="count a step that makes the app log a warning as failed")
        batch_parser.add_argument("--var", action="append", default=[], metavar="NAME=VALUE",
                                  help="predefine ${NAME} (repeatable)")
        batch_parser.set_defaults(handler=handle_batch)

        tour = add("tour", "run tools/driver/tours/NAME.txt in session tour-NAME and collect its shots")
        tour.add_argument("name", nargs="?", help="tour name (omit to list them)")
        tour.add_argument("--report", action="store_true", help="build the contact sheet afterwards")
        tour.add_argument("--stop-on-failure", action="store_true", help="stop at the first failing step")
        tour.add_argument("--fail-on-warnings", action="store_true", help="as for batch")
        tour.add_argument("--keep-running", action="store_true", help="leave the tour's session running")
        tour.set_defaults(handler=handle_tour)

        report_parser = add("report", "build an HTML contact sheet from a shots directory and its index.jsonl")
        report_parser.add_argument("directory", nargs="?", help="shots directory (default: the session's shots/)")
        report_parser.add_argument("-o", "--output", help="output file (default: DIRECTORY/index.html)")
        report_parser.add_argument("--coverage", metavar="FILE",
                            help="coverage.json to check (default: one in or beside DIRECTORY)")
        report_parser.add_argument("--title")
        report_parser.add_argument("--link", action="store_true",
                            help="link the PNGs instead of embedding them (smaller, but no longer self-contained)")
        report_parser.set_defaults(handler=handle_report)

    # Session data files: usable in batches, so tours can change data mid-run.
    put = add("put", "copy files into the session's data/ and wait for the app to process them")
    put.add_argument("sources", nargs="+", metavar="SRC")
    put.add_argument("--to", metavar="FOLDER",
                     help="performances, stats, benchmarks, '.' (data/ itself) or a path under data/; "
                          "default: by extension (.perf, .csv, .json)")
    put.add_argument("--timeout", type=float, default=5.0, help="max seconds to wait for the app (default 5)")
    put.set_defaults(handler=handle_put)

    rm = add("rm", "delete files under the session's data/ and wait for the app")
    rm.add_argument("paths", nargs="+", metavar="PATH", help="relative to data/, e.g. benchmarks/driver-demo.json")
    rm.add_argument("--timeout", type=float, default=5.0)
    rm.set_defaults(handler=handle_rm)

    edit = add("edit", "rewrite a file under the session's data/ in place and wait for the app")
    edit.add_argument("path", help="relative to data/")
    edit.add_argument("--replace", nargs=2, action="append", default=[], metavar=("OLD", "NEW"),
                      help="replace every OLD with NEW (repeatable; OLD must occur)")
    edit.add_argument("--from", dest="from_file", metavar="SRC", help="replace the whole file with SRC")
    edit.add_argument("--timeout", type=float, default=5.0)
    edit.set_defaults(handler=handle_edit)

    shot = add("shot", "save a PNG of a window or an item and print its path")
    shot.add_argument("--window", help="window key (see `windows`; default: main)")
    shot.add_argument("--item", metavar="SELECTOR", help="crop to this item")
    shot.add_argument("--pad", type=int, help="padding around --item in px (default 8)")
    shot.add_argument("-o", "--output", help="output path (default: the session's shots/ dir)")
    shot.add_argument("--name", metavar="LABEL", help="save as shots/NNN-LABEL.png and index it under that label")
    shot.add_argument("--dir", metavar="SUBDIR", help="save under shots/SUBDIR/ (a report groups shots by it)")
    shot.add_argument("--all-windows", action="store_true", help="one PNG per visible window")
    shot.add_argument("--with-context", action="store_true",
                      help="paint a dialog window over the window it belongs to, at its position")
    shot.set_defaults(handler=handle_shot)

    def protocol(name: str, help_text: str, builder: Callable, *, action: bool = False) -> argparse.ArgumentParser:
        sub = add(name, help_text)
        sub.set_defaults(handler=run_protocol, build=builder)
        if action:
            _add_action_arguments(sub)
        return sub

    protocol("windows", "list the top-level windows", build_windows)

    tree = protocol("tree", "print the item tree (named/interactive items; --raw for all)", build_tree)
    tree.add_argument("selector", nargs="?", help="root item (default: every visible window)")
    tree.add_argument("--depth", type=int)
    tree.add_argument("--all", action="store_true", help="include hidden items")
    tree.add_argument("--raw", action="store_true", help="show every item, not just named/interactive ones")

    find = protocol("find", "list items matching a selector", build_find)
    find.add_argument("selector")
    find.add_argument("--all", action="store_true", help="include hidden items")
    find.add_argument("--limit", type=int, default=50)
    find.add_argument("--objects", action="store_true",
                      help="list non-visual QML objects only (Action, Timer, Settings, dialogs)")

    for name, help_text in (("click", "click an item (its center) or a point"),
                            ("dblclick", "double-click an item or a point"),
                            ("rclick", "right-click an item or a point")):
        click = protocol(name, help_text, build_click, action=True)
        _add_pointer_arguments(click)
        click.add_argument("--button", choices=("left", "right", "middle"), default="left")
        click.add_argument("--double", action="store_true")
        click.add_argument("--mods", help="modifiers, e.g. ctrl+shift")

    hover = protocol("hover", "move the mouse over an item or a point, through intermediate moves", build_hover,
                     action=True)
    _add_pointer_arguments(hover)
    hover.add_argument("--steps", type=int, help="moves from the last cursor position (default 8)")

    mouse = protocol("mouse", "one stage of a held gesture: press, move (with the button held) or release",
                     build_mouse, action=True)
    mouse.add_argument("action", choices=("press", "move", "release"))
    _add_pointer_arguments(mouse)
    mouse.add_argument("--button", choices=("left", "right", "middle"), default="left")
    mouse.add_argument("--mods", help="modifiers, e.g. ctrl+shift")
    mouse.add_argument("--steps", type=int, help="intermediate moves from the last position (default: move 12, else 1)")

    drag = protocol("drag", "press, move and release between two items or X,Y points", build_drag, action=True)
    drag.add_argument("source", help="selector or X,Y")
    drag.add_argument("destination", help="selector or X,Y")
    drag.add_argument("--steps", type=int)
    drag.add_argument("--window", help="window key for X,Y points")
    drag.add_argument("--mods")

    scroll = protocol("scroll", "send wheel notches over an item (positive --dy scrolls down)", build_scroll, action=True)
    _add_pointer_arguments(scroll, offset=False)
    scroll.add_argument("--dy", type=int, default=0)
    scroll.add_argument("--dx", type=int, default=0)
    scroll.add_argument("--mods")

    key = protocol("key", "send a key or shortcut, e.g. Return, Escape, Tab, Ctrl+A, Down", build_key, action=True)
    key.add_argument("sequence")
    key.add_argument("--repeat", type=int, default=1)
    key.add_argument("--window")

    type_parser = protocol("type", "type text into the focused item (or --into SELECTOR)", build_type, action=True)
    type_parser.add_argument("text")
    type_parser.add_argument("--into", metavar="SELECTOR", help="focus this item first")
    type_parser.add_argument("--clear", action="store_true", help="select all and delete first")
    type_parser.add_argument("--at-start", action="store_true",
                             help="with --into, put the caret at the start instead of the end")
    type_parser.add_argument("--enter", action="store_true", help="press Return afterwards")

    get = protocol("get", "read a property (QQmlProperty name, e.g. text, currentIndex, Layout.fillWidth)", build_get)
    get.add_argument("selector")
    get.add_argument("property")
    get.add_argument("--all", action="store_true", help="allow hidden items")

    set_parser = protocol("set", "write a property (VALUE is JSON when it parses, else a string)", build_set, action=True)
    set_parser.add_argument("selector")
    set_parser.add_argument("property")
    set_parser.add_argument("value")
    set_parser.add_argument("--all", action="store_true", help="allow hidden items")

    invoke = protocol("invoke", "call a method or emit a signal (e.g. open, close, clicked)", build_invoke, action=True)
    invoke.add_argument("selector")
    invoke.add_argument("method")
    invoke.add_argument("args", nargs="*", help="arguments (JSON when they parse, else strings)")
    invoke.add_argument("--all", action="store_true", help="allow hidden items")

    eval_parser = protocol("eval", "evaluate a QML/JS expression (Main.qml's ids are in scope)", build_eval, action=True)
    eval_parser.add_argument("expression")
    eval_parser.add_argument("--on", metavar="SELECTOR", help="evaluate in this item's scope instead")

    wait = protocol("wait", "wait until an item exists/disappears, a property matches, or --expr is truthy", build_wait)
    wait.add_argument("selector", nargs="?")
    wait.add_argument("--prop")
    wait.add_argument("--equals")
    wait.add_argument("--expr")
    wait.add_argument("--gone", action="store_true", help="wait until nothing matches")
    wait.add_argument("--all", action="store_true", help="match hidden items too")
    wait.add_argument("--timeout", type=int, default=5000, metavar="MS")

    resize = protocol("resize", "resize a window", build_resize, action=True)
    resize.add_argument("size", help="WxH")
    resize.add_argument("--window")

    logs = protocol("logs", "show app log messages since the last `logs` call", build_logs)
    logs.add_argument("--all", action="store_true", help="the whole buffer, not just new entries")
    logs.add_argument("--warnings", action="store_true", help="warnings and errors only")
    logs.add_argument("--clear", action="store_true")
    return parser


def parse_args(arguments: Sequence[str], *, batch: bool = False) -> argparse.Namespace:
    # `--shot=LABEL` names the action's screenshot; plain `--shot` stays a flag, so
    # `click --shot item` keeps meaning what it always did.
    expanded: list[str] = []
    for argument in arguments:
        if argument.startswith("--shot="):
            expanded += ["--shot", "--shot-name", argument.partition("=")[2]]
        else:
            expanded.append(argument)
    return build_parser(batch=batch).parse_args(expanded)


def describe_error(error: BaseException) -> list[str]:
    if isinstance(error, DriverCommandError):
        return formatting.error_lines(str(error), error.details)
    return [f"error: {error}"]


def main(arguments: Sequence[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    try:
        ns = parse_args(sys.argv[1:] if arguments is None else arguments)
    except UsageError as error:
        print(f"error: {error}")
        return 2
    try:
        ctx = Context(session=Session(REPO_ROOT, ns.session or ns.global_session or "default"),
                      json_output=ns.json or ns.global_json,
                      command_line=shlex.join(sys.argv[1:] if arguments is None else arguments))
        return ns.handler(ns, ctx)
    except UsageError as error:
        print(f"error: {error}")
        return 2
    except (DriverCommandError, DriverConnectionError, SessionError, ToolchainError, ValueError) as error:
        for line in describe_error(error):
            print(line)
        return 1
