"""ui_driver.py: launch KSV headlessly, screenshot it, and drive its UI.

Every subcommand except start/stop/restart/status/batch is one request to the
running ksv_driver session. See tools/driver/README.md for the selector syntax.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable, Sequence
from dataclasses import dataclass
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time
from typing import Any

from build_runner.config import ToolchainError, resolve_toolchain

from . import formatting
from .client import DriverCommandError, DriverConnectionError, request
from .session import (
    DATASETS,
    DRIVER_EXE,
    LaunchOptions,
    Session,
    SessionError,
    launch_command,
    launch_environment,
    pid_alive,
    spawn,
    terminate,
    wait_until_ready,
)


SCRIPTS_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = SCRIPTS_DIR.parent
DRIVER_TARGET = "ksv_driver"


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
    return "hover", _pointer_args(ns)


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
    if command in ("click", "hover", "scroll"):
        verb = {"click": "clicked", "hover": "hovered", "scroll": "scrolled"}[command]
        pos = result.get("pos")
        where = f" at {pos[0]},{pos[1]}" if pos else ""
        return [f"{verb} {_target_text(result.get('target'))}{where}"]
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
        if result.get("otherVisibleWindows"):
            lines.append("other visible windows: " + ", ".join(result["otherVisibleWindows"]) + " (shot --window KEY)")
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
    if ctx.json_output:
        ctx.out(json.dumps(result, ensure_ascii=False))
        return
    for line in render(command, result):
        ctx.out(line)
    for line in formatting.warning_lines(result):
        ctx.out(line)


def take_screenshot(ctx: Context, *, window: str | None = None, item: str | None = None, pad: int | None = None,
                    output: str | None = None, label: str | None = None) -> dict[str, Any]:
    args: dict[str, Any] = {}
    if item:
        args["selector"] = item
    elif window:
        args["window"] = window
    if pad is not None:
        args["pad"] = pad
    path = Path(output) if output else ctx.session.next_shot_path(label or item or window or "main")
    args["path"] = str(path.resolve())
    return call(ctx, "screenshot", args)


def run_protocol(ns: argparse.Namespace, ctx: Context) -> int:
    command, args = ns.build(ns)
    if getattr(ns, "settle", None) is not None:
        args["settleMs"] = ns.settle
    result = call(ctx, command, args)
    emit_result(ctx, command, result)
    if getattr(ns, "shot", False):
        target = result.get("target") or result.get("to") or {}
        window = target.get("window") if isinstance(target, dict) else getattr(ns, "window", None)
        shot = take_screenshot(ctx, window=window, label=f"{ns.command}-{getattr(ns, 'selector', '') or ''}")
        emit_result(ctx, "screenshot", shot)
    return 0


def handle_shot(ns: argparse.Namespace, ctx: Context) -> int:
    if ns.all_windows:
        windows = call(ctx, "windows", {}).get("windows", [])
        for window in windows:
            if window.get("visible"):
                emit_result(ctx, "screenshot", take_screenshot(ctx, window=window["key"], label=window["key"]))
        return 0
    result = take_screenshot(ctx, window=ns.window, item=ns.item, pad=ns.pad, output=ns.output)
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


def stop_session(ctx: Context, *, quiet: bool = False) -> bool:
    info = ctx.session.read_info()
    pid = int(info.get("pid", 0)) if info else 0
    if not pid or not pid_alive(pid):
        if not quiet:
            ctx.out(f"session '{ctx.session.name}' is not running")
        return False
    try:
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


def start_session(ctx: Context, options: LaunchOptions, *, build: bool, keep: bool, timeout: float) -> int:
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
    process = spawn(launch_command(exe, session, options), launch_environment(os.environ, toolchain, options),
                    session.log_file)
    info = wait_until_ready(session, process, timeout)
    if ctx.json_output:
        ctx.out(json.dumps(info))
        return 0
    mode = "visible" if options.visible else f"headless ({info.get('platform')})"
    qml = "compiled module" if info.get("qmlSource") == "compiled" else "src/ui/qml (restart picks up QML edits)"
    ctx.out(f"session '{session.name}' ready: pid {info.get('pid')}, port {info.get('port')}, {mode}, "
            f"dataset {info.get('dataset')}, QML from {qml}")
    if not info.get("profileLoaded"):
        ctx.out("note: the profile was still loading when the driver came up")
    ctx.out(f"screenshots: {session.shots_dir.relative_to(REPO_ROOT).as_posix()}/   log: {session.log_file.relative_to(REPO_ROOT).as_posix()}")
    return 0


def handle_start(ns: argparse.Namespace, ctx: Context) -> int:
    parse_size(ns.size)
    options = LaunchOptions(dataset=ns.dataset, visible=ns.visible, size=ns.size, compiled_qml=ns.compiled_qml)
    return start_session(ctx, options, build=ns.build, keep=ns.keep, timeout=ns.timeout)


def handle_restart(ns: argparse.Namespace, ctx: Context) -> int:
    options = ctx.session.read_launch_options() or LaunchOptions()
    return start_session(ctx, options, build=ns.build, keep=True, timeout=ns.timeout)


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
    for window in windows:
        ctx.out("  " + formatting.window_line(window))
    return 0


def handle_batch(ns: argparse.Namespace, ctx: Context) -> int:
    text = sys.stdin.read() if ns.file == "-" else Path(ns.file).read_text(encoding="utf-8")
    failures = 0
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        ctx.out(f"> {line}")
        try:
            if line.startswith("{"):
                step = json.loads(line)
                result = call(ctx, step["cmd"], step.get("args", {}))
                emit_result(ctx, step["cmd"], result)
            else:
                step_ns = parse_args(shlex.split(line), batch=True)
                code = step_ns.handler(step_ns, ctx)
                if code:
                    raise UsageError(f"step exited with {code}")
        except Exception as error:  # noqa: BLE001 -- report and decide whether to continue
            failures += 1
            for message in describe_error(error):
                ctx.out(message)
            if not ns.keep_going:
                ctx.out(f"batch stopped at line {number}")
                return 1
    return 1 if failures else 0


# ---- argument parsing ----------------------------------------------------------


def _add_pointer_arguments(parser: argparse.ArgumentParser, *, offset: bool = True) -> None:
    parser.add_argument("selector", nargs="?", help="item selector (see README); or use --at")
    parser.add_argument("--at", nargs=2, type=float, metavar=("X", "Y"), help="window coordinates instead of a selector")
    parser.add_argument("--window", help="window key for --at (default: main)")
    if offset:
        parser.add_argument("--offset", nargs=2, type=float, metavar=("DX", "DY"),
                            help="point relative to the item's top-left instead of its center")


def _add_action_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--shot", action="store_true", help="screenshot the window after the action")
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
        start.add_argument("--dataset", choices=DATASETS, default="rich")
        start.add_argument("--visible", action="store_true", help="show a real window instead of running headless")
        start.add_argument("--size", default="1280x800", help="main window size, WxH")
        start.add_argument("--build", action="store_true", help=f"build {DRIVER_TARGET} first")
        start.add_argument("--keep", action="store_true", help="keep the session's existing data and settings")
        start.add_argument("--compiled-qml", action="store_true", help="use the compiled QML module, not src/ui/qml")
        start.add_argument("--timeout", type=float, default=90.0, help="seconds to wait for readiness")
        start.set_defaults(handler=handle_start)

        restart = add("restart", "restart with the same options, keeping data (picks up QML edits)")
        restart.add_argument("--build", action="store_true", help=f"build {DRIVER_TARGET} first (C++ changes)")
        restart.add_argument("--timeout", type=float, default=90.0)
        restart.set_defaults(handler=handle_restart)

        add("stop", "stop the session").set_defaults(handler=handle_stop)
        add("status", "show whether the session runs, and its windows").set_defaults(handler=handle_status)

        batch_parser = add("batch", "run steps from a file (or - for stdin): CLI lines or JSON requests")
        batch_parser.add_argument("file")
        batch_parser.add_argument("--keep-going", action="store_true")
        batch_parser.set_defaults(handler=handle_batch)

    shot = add("shot", "save a PNG of a window or an item and print its path")
    shot.add_argument("--window", help="window key (see `windows`; default: main)")
    shot.add_argument("--item", metavar="SELECTOR", help="crop to this item")
    shot.add_argument("--pad", type=int, help="padding around --item in px (default 8)")
    shot.add_argument("-o", "--output", help="output path (default: the session's shots/ dir)")
    shot.add_argument("--all-windows", action="store_true", help="one PNG per visible window")
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

    for name, help_text in (("click", "click an item (its center) or a point"),
                            ("dblclick", "double-click an item or a point"),
                            ("rclick", "right-click an item or a point")):
        click = protocol(name, help_text, build_click, action=True)
        _add_pointer_arguments(click)
        click.add_argument("--button", choices=("left", "right", "middle"), default="left")
        click.add_argument("--double", action="store_true")
        click.add_argument("--mods", help="modifiers, e.g. ctrl+shift")

    hover = protocol("hover", "move the mouse over an item or a point", build_hover, action=True)
    _add_pointer_arguments(hover)

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
    return build_parser(batch=batch).parse_args(list(arguments))


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
                      json_output=ns.json or ns.global_json)
        return ns.handler(ns, ctx)
    except UsageError as error:
        print(f"error: {error}")
        return 2
    except (DriverCommandError, DriverConnectionError, SessionError, ToolchainError) as error:
        for line in describe_error(error):
            print(line)
        return 1
