from __future__ import annotations

from datetime import datetime
import os
from pathlib import Path
import sys

import pytest

from scripts.build_runner.process import ProgressReporter, ProgressState, describe_exit_code, run_command


def test_sparse_reporter_emits_at_thirty_seconds_then_once_per_minute() -> None:
    reporter = ProgressReporter(first_after=30.0, interval=60.0)

    assert not reporter.should_emit(29.9)
    assert reporter.should_emit(30.0)
    assert not reporter.should_emit(89.9)
    assert reporter.should_emit(90.0)


def test_ninja_progress_line_produces_percentage_and_eta() -> None:
    progress = ProgressState()
    progress.observe("[43/101] Building CXX object example.obj")

    message = progress.format_message(
        "build",
        elapsed=90.0,
        now=datetime(2026, 9, 7, 12, 0, 0),
    )

    assert message == "BUILD | 43/101 (43%) | 1m30s elapsed | ETA ~12:02 (about 2m)"


def test_phase_without_numeric_progress_still_reports_elapsed_time() -> None:
    progress = ProgressState()

    assert progress.format_message("configure", elapsed=90.0) == "CONFIGURE | 1m30s elapsed | still running"


@pytest.mark.parametrize(
    ("code", "description"),
    [
        (0xC0000005, "access violation"),
        (0xC0000135, "required DLL not found"),
        (0xC0000139, "DLL entry point not found"),
    ],
)
def test_windows_process_statuses_are_named(code: int, description: str) -> None:
    assert describe_exit_code(code) == f"{code} (0x{code:08X}: {description})"


def test_command_capture_merges_stderr_into_output_and_retained_log(tmp_path: Path) -> None:
    log_path = tmp_path / "command.log"

    result = run_command(
        [
            sys.executable,
            "-c",
            "import sys; print('normal', flush=True); print('failure', file=sys.stderr, flush=True)",
        ],
        log_path=log_path,
        cwd=tmp_path,
        environment=os.environ,
        phase="test",
    )

    assert result.exit_code == 0
    assert result.output.splitlines() == ["normal", "failure"]
    retained = log_path.read_text(encoding="utf-8")
    assert retained.startswith("> ")
    assert retained.endswith("normal\nfailure\n")
