from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path
from queue import Empty, Queue
import re
import subprocess
from threading import Thread
import time
from typing import Callable, Mapping, Sequence


WINDOWS_STATUS = {
    0xC0000005: "access violation",
    0xC0000135: "required DLL not found",
    0xC0000139: "DLL entry point not found",
    0xC0000409: "stack buffer overrun or fast-fail",
}


def format_duration(seconds: float) -> str:
    total = max(0, int(round(seconds)))
    minutes, remaining = divmod(total, 60)
    if minutes:
        return f"{minutes}m{remaining:02d}s"
    return f"{remaining}s"


def describe_exit_code(code: int) -> str:
    unsigned = code & 0xFFFFFFFF
    if unsigned in WINDOWS_STATUS:
        return f"{unsigned} (0x{unsigned:08X}: {WINDOWS_STATUS[unsigned]})"
    return str(code)


@dataclass
class ProgressReporter:
    first_after: float = 30.0
    interval: float = 60.0
    _next_at: float | None = None

    def should_emit(self, elapsed: float) -> bool:
        if self._next_at is None:
            self._next_at = self.first_after
        if elapsed < self._next_at:
            return False
        self._next_at = elapsed + self.interval
        return True


@dataclass
class ProgressState:
    completed: int | None = None
    total: int | None = None

    def observe(self, line: str) -> None:
        match = re.match(r"^\[(\d+)/(\d+)\]", line)
        if not match:
            match = re.match(r"^\s*(\d+)/(\d+)\s+Test\s+#?\d+:", line)
        if match:
            completed, total = (int(value) for value in match.groups())
            if total > 0 and completed >= 0:
                self.completed = completed
                self.total = total

    def format_message(self, phase: str, *, elapsed: float, now: datetime | None = None) -> str:
        prefix = phase.upper()
        elapsed_text = format_duration(elapsed)
        if not self.completed or not self.total:
            return f"{prefix} | {elapsed_text} elapsed | still running"
        percentage = round(self.completed * 100 / self.total)
        remaining_seconds = elapsed * (self.total - self.completed) / self.completed
        current = now or datetime.now()
        eta = current + timedelta(seconds=remaining_seconds)
        if remaining_seconds >= 60:
            approximate = f"{max(1, round(remaining_seconds / 60))}m"
        else:
            approximate = f"{max(1, round(remaining_seconds))}s"
        return (
            f"{prefix} | {self.completed}/{self.total} ({percentage}%) | {elapsed_text} elapsed | "
            f"ETA ~{eta:%H:%M} (about {approximate})"
        )


@dataclass(frozen=True)
class CommandResult:
    exit_code: int
    log_path: Path
    output: str


def run_command(
    command: Sequence[str | Path],
    *,
    log_path: Path,
    cwd: Path,
    environment: Mapping[str, str],
    logged_environment: Mapping[str, str] | None = None,
    show_all_output: bool = False,
    phase: str,
    emit: Callable[[str], None] = print,
) -> CommandResult:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    arguments = [str(argument) for argument in command]
    progress = ProgressState()
    reporter = ProgressReporter()
    started = time.monotonic()
    lines: list[str] = []
    queue: Queue[str | None] = Queue()

    with log_path.open("w", encoding="utf-8", newline="\n") as log:
        log.write(f"> {subprocess.list2cmdline(arguments)}\n")
        for name, value in sorted((logged_environment or {}).items()):
            log.write(f"ENV {name}={value}\n")
        log.flush()
        process = subprocess.Popen(
            arguments,
            cwd=cwd,
            env=dict(environment),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )

        def read_output() -> None:
            assert process.stdout is not None
            for raw_line in process.stdout:
                queue.put(raw_line)
            queue.put(None)

        reader = Thread(target=read_output, daemon=True)
        reader.start()
        finished = False
        while not finished:
            try:
                raw_line = queue.get(timeout=0.25)
            except Empty:
                raw_line = ""
            if raw_line is None:
                finished = True
            elif raw_line:
                line = raw_line.rstrip("\r\n")
                lines.append(line)
                log.write(line + "\n")
                log.flush()
                progress.observe(line)
                if show_all_output:
                    emit(line)
            elapsed = time.monotonic() - started
            if not show_all_output and not finished and reporter.should_emit(elapsed):
                emit(progress.format_message(phase, elapsed=elapsed))
        exit_code = process.wait()
        reader.join()
    return CommandResult(exit_code, log_path, "\n".join(lines))
