from __future__ import annotations

from pathlib import Path

import pytest

from scripts.build_runner.cli import UsageError, parse_args
from scripts.build_runner.config import Toolchain
from scripts.build_runner.orchestrator import run_pipeline
from scripts.build_runner.process import CommandResult
from scripts.build_runner.qmllint import Note, parse_qmllint_json, render_qmllint_report
from scripts.tests.build_runner.test_orchestrator import SETTINGS, FakeExecutor, make_toolchain, ticking_clock


def write_fixture(fixtures_dir: Path, repo: Path, destination: Path) -> Path:
    text = (fixtures_dir / "qmllint.json").read_text(encoding="utf-8").replace("C:/repo", repo.as_posix())
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(text, encoding="utf-8")
    return destination


def test_parser_folds_notes_into_the_finding_they_follow_and_keeps_suggested_edits(
    fixtures_dir: Path, tmp_path: Path
) -> None:
    repo = tmp_path / "repo"
    report = write_fixture(fixtures_dir, repo, tmp_path / "ksv_ui_qmllint.json")

    findings = parse_qmllint_json(report, repo)

    assert [(finding.path, finding.line, finding.category) for finding in findings] == [
        ("src/ui/qml/Main.qml", 5, "unused-imports"),
        ("src/ui/qml/Main.qml", 70, "id-shadows-member"),
        ("src/ui/qml/Main.qml", 114, "Quick.layout-positioning"),
        ("src/ui/qml/benchmark/BenchmarkCellEditor.qml", 136, "unqualified"),
        ("src/ui/qml/benchmark/BenchmarkCellEditor.qml", 163, "unqualified"),
    ]
    assert findings[1].notes == (Note("src/ui/qml/Main.qml", 20, 13, "Id defined here"),)
    assert findings[3].fix == 'insert "editor."'
    assert not findings[0].fails


def test_report_keys_files_and_groups_findings_by_message(fixtures_dir: Path, tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    findings = parse_qmllint_json(write_fixture(fixtures_dir, repo, tmp_path / "report.json"), repo)
    log = tmp_path / "qmllint.log"

    lines = render_qmllint_report(findings, scope="qmllint", elapsed=3.0, repo_root=repo, log_paths=[log])

    assert lines == [
        "",
        "FAILED (qmllint) | scope qmllint | 4 findings in 2 files | 3.0s",
        f"paths relative to {repo.resolve().as_posix()}",
        "(1 info-level finding not shown; they do not fail the scope)",
        "F1 = src/ui/qml/Main.qml",
        "F2 = src/ui/qml/benchmark/BenchmarkCellEditor.qml",
        "",
        'R1 warning [id-shadows-member] Id for object VisualSettingsManager shadows property "visualSettings". '
        "Rename one or the other.",
        "  F1:70:25  note F1:20:13 Id defined here",
        "",
        "R2 warning [Quick.layout-positioning] Detected width on an item that is managed by a layout in "
        "src/ui/qml/Main.qml.",
        "  F1:114:20",
        "",
        "R3 x2 warning [unqualified] Unqualified access",
        '  F2:136:126  insert "editor."',
        "  F2:163:24",
        "",
        f"log (read only if the above is insufficient): {log.as_posix()}",
    ]


def test_report_for_a_single_file_names_it_once_and_drops_the_key(fixtures_dir: Path, tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    findings = parse_qmllint_json(write_fixture(fixtures_dir, repo, tmp_path / "report.json"), repo)
    editor_only = [finding for finding in findings if finding.category == "unqualified"]

    lines = render_qmllint_report(editor_only, scope="qmllint", elapsed=1.0, repo_root=repo, log_paths=[])

    assert lines[3:] == [
        "file src/ui/qml/benchmark/BenchmarkCellEditor.qml",
        "",
        "R1 x2 warning [unqualified] Unqualified access",
        '  136:126  insert "editor."',
        "  163:24",
    ]


class LintExecutor(FakeExecutor):
    """Writes `report` into the build tree when the qmllint target is built, as qmllint would."""

    def __init__(self, *args, write_report, **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self.write_report = write_report

    def __call__(self, command, **kwargs) -> CommandResult:
        if "all_qmllint_json" in [str(value) for value in command]:
            self.write_report()
        return super().__call__(command, **kwargs)


def run_lint(
    toolchain: Toolchain, repo: Path, build_result: tuple[int, str], write_report=lambda: None
) -> tuple[int, list[str], LintExecutor]:
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True, exist_ok=True)
    executor = LintExecutor(
        build_dir, toolchain, [(0, "-- Configuring done"), build_result], write_report=write_report
    )
    output: list[str] = []
    result = run_pipeline(
        parse_args(["--scope", "qmllint"]),
        repo_root=repo,
        log_dir=repo / ".temp" / "run",
        toolchain=toolchain,
        settings=SETTINGS,
        process_environment={},
        execute=executor,
        emit=output.append,
        clock=ticking_clock(0.0, 3.0),
    )
    return result, output, executor


def test_scope_builds_only_the_lint_target_and_fails_on_warnings(
    toolchain_tree: tuple[Path, dict[str, str]], fixtures_dir: Path, tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    report = repo / "build-agent" / "ksv_ui_qmllint.json"

    result, output, executor = run_lint(
        toolchain, repo, (0, "[1/1] qmllint"), write_report=lambda: write_fixture(fixtures_dir, repo, report)
    )

    assert result == 1
    assert executor.commands[1][-4:] == ["--parallel", "8", "--target", "all_qmllint_json"]
    assert len(executor.commands) == 2
    assert output[1] == "FAILED (qmllint) | scope qmllint | 4 findings in 2 files | 3.0s"


def test_scope_passes_when_only_info_findings_remain(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    report = repo / "build-agent" / "ksv_ui_qmllint.json"
    info_only = (
        '{"files": [{"filename": "C:/repo/src/ui/qml/Main.qml", "success": true, "warnings": ['
        '{"column": 1, "id": "unused-imports", "line": 5, "message": "Unused import", "type": "info"}]}]}'
    )

    result, output, _ = run_lint(toolchain, repo, (0, ""), write_report=lambda: report.write_text(info_only))

    assert result == 0
    assert output == ["Scope qmllint | jobs 8 | findings 0 (1 info) | 3.0s | PASS"]


def test_scope_does_not_read_the_last_runs_report_when_qmllint_writes_none(
    toolchain_tree: tuple[Path, dict[str, str]], fixtures_dir: Path, tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    write_fixture(fixtures_dir, repo, repo / "build-agent" / "ksv_ui_qmllint.json")

    with pytest.raises(Exception, match="wrote no \\*_qmllint.json report"):
        run_lint(toolchain, repo, (0, ""))


def test_compile_failure_before_qmllint_is_reported_as_a_build_failure(
    toolchain_tree: tuple[Path, dict[str, str]], fixtures_dir: Path, tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    compiler_output = (fixtures_dir / "compiler.log").read_text(encoding="utf-8").replace("C:/repo", repo.as_posix())

    result, output, _ = run_lint(toolchain, repo, (1, compiler_output))

    assert result == 1
    assert output[1] == "FAILED (build) | scope qmllint | 1 diagnostic | 3.0s"


def test_qmllint_scope_rejects_test_selection_options() -> None:
    for extra in (["--match", "*"], ["--list"], ["--build-only"]):
        with pytest.raises(UsageError, match="--scope qmllint takes no"):
            parse_args(["--scope", "qmllint", *extra])
