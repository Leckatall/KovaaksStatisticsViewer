from __future__ import annotations

import argparse
from collections.abc import Sequence
from datetime import datetime
import os
from pathlib import Path
import traceback

from .config import (
    ToolchainError,
    build_fingerprint,
    fingerprint_matches,
    resolve_toolchain,
    safe_remove_build_tree,
)
from .locking import build_tree_lock
from .orchestrator import BUILD_JOBS, run_pipeline


SCOPES = ("all", "domain", "data", "qt-data", "app", "ui", "ui-cpp", "qml", "integration")


class UsageError(RuntimeError):
    pass


class ArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise UsageError(message)


def parse_args(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    parser = ArgumentParser(description="Deterministically configure, build, and test KovaaksStatsViewer.")
    parser.add_argument("--scope", choices=SCOPES, default="all")
    parser.add_argument("--match")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--show-all-output", action="store_true")
    args = parser.parse_args(arguments)
    if args.scope == "all" and args.match:
        raise UsageError("--match requires a focused scope; it cannot be used with --scope all.")
    if args.build_only and args.list:
        raise UsageError("--build-only and --list cannot be combined.")
    if args.build_only and args.match:
        raise UsageError("--build-only and --match cannot be combined.")
    return args


def main(arguments: Sequence[str] | None = None) -> int:
    try:
        args = parse_args(arguments)
    except UsageError as error:
        print(f"error: {error}")
        return 2

    scripts_dir = Path(__file__).resolve().parents[1]
    repo_root = scripts_dir.parent
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")[:-3]
    log_dir = repo_root / ".temp" / "build-and-test" / f"{stamp}-{os.getpid()}"
    runner_log = log_dir / "runner-error.log"
    try:
        log_dir.mkdir(parents=True, exist_ok=True)
        toolchain = resolve_toolchain(scripts_dir)
        fingerprint = build_fingerprint(toolchain)
        build_dir = repo_root / "build-agent"
        fingerprint_path = build_dir / ".ksv-toolchain-fingerprint.json"
        with build_tree_lock(repo_root / ".temp" / "build-agent.lock"):
            if args.clean or (build_dir.exists() and not fingerprint_matches(fingerprint_path, fingerprint)):
                safe_remove_build_tree(build_dir, repo_root)
            return run_pipeline(
                args,
                repo_root=repo_root,
                log_dir=log_dir,
                toolchain=toolchain,
                process_environment=os.environ,
                fingerprint=fingerprint,
            )
    except (ToolchainError, OSError, ValueError) as error:
        log_dir.mkdir(parents=True, exist_ok=True)
        runner_log.write_text(str(error) + "\n", encoding="utf-8")
        print("")
        print("FAILED (runner)")
        print(error)
        print(f"Full log: {runner_log}")
        return 1
    except Exception as error:
        log_dir.mkdir(parents=True, exist_ok=True)
        runner_log.write_text(traceback.format_exc(), encoding="utf-8")
        print("")
        print("FAILED (runner)")
        print(f"{type(error).__name__}: {error}")
        print(f"Full log: {runner_log}")
        return 1
