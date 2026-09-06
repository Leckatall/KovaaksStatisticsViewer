from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


PROJECT_ROOT = Path(__file__).resolve().parents[3]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))


@dataclass(frozen=True)
class CliResult:
    returncode: int
    stdout: str
    stderr: str


@pytest.fixture
def script_source() -> Path:
    return PROJECT_ROOT / "scripts" / "arch_doc.py"


@pytest.fixture
def run_cli(tmp_path: Path, script_source: Path):
    repo = tmp_path / "repo"
    (repo / "scripts").mkdir(parents=True)
    shutil.copy2(script_source, repo / "scripts" / "arch_doc.py")
    outside = tmp_path / "outside"
    outside.mkdir()

    def run(*args: str) -> CliResult:
        completed = subprocess.run(
            [sys.executable, str(repo / "scripts" / "arch_doc.py"), *args],
            cwd=outside,
            text=True,
            capture_output=True,
            check=False,
        )
        return CliResult(completed.returncode, completed.stdout, completed.stderr)

    run.repo = repo
    return run