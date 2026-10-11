from __future__ import annotations

import json
from pathlib import Path
import shlex

import pytest

from ui_automation import report
from ui_automation.cli import (
    TOURS_DIR,
    BatchOutcome,
    Context,
    UsageError,
    parse_args,
    record_shot,
    run_batch,
    substitute,
)
from ui_automation.session import Session, SessionError, next_shot_path


PNG = bytes.fromhex(
    "89504e470d0a1a0a0000000d4948445200000001000000010806000000"
    "1f15c4890000000d49444154789c6360000002000154a24f5d0000000049454e44ae426082"
)


def make_context(tmp_path: Path, lines: list[str] | None = None) -> Context:
    session = Session(tmp_path, "demo")
    session.data_dir.mkdir(parents=True)
    return Context(session=session, json_output=False, out=(lines.append if lines is not None else print))


def test_named_shots_keep_the_running_number_across_subdirectories(tmp_path: Path) -> None:
    root = tmp_path / "shots"
    first = next_shot_path(root, "benchmarks-default")
    first.write_bytes(PNG)
    second = next_shot_path(root, "Manager: dirty close", subdir="manager")
    second.write_bytes(PNG)

    assert first.name == "001-benchmarks-default.png"
    assert second == root / "manager" / "002-Manager-dirty-close.png"
    assert next_shot_path(root, "x").name == "003-x.png"
    with pytest.raises(SessionError, match="must stay under"):
        next_shot_path(root, "x", subdir="../escape")


def test_shot_option_takes_an_optional_label_without_breaking_the_flag() -> None:
    named = parse_args(["click", "benchmarksTab", "--shot=benchmarks-default", "--shot-dir", "benchmarks"])
    assert (named.shot, named.shot_name, named.shot_dir) == (True, "benchmarks-default", "benchmarks")
    plain = parse_args(["click", "--shot", "benchmarksTab"])
    assert (plain.shot, plain.shot_name, plain.selector) == (True, None, "benchmarksTab")
    shot = parse_args(["shot", "--name", "first-run", "--dir", "scenarios"])
    assert (shot.name, shot.dir) == ("first-run", "scenarios")


def test_each_shot_appends_its_metadata_to_index_jsonl(tmp_path: Path) -> None:
    ctx = make_context(tmp_path)
    ctx.command_line = "click benchmarksTab --shot=benchmarks-default"
    path = next_shot_path(ctx.shots_dir, "benchmarks-default", subdir="benchmarks")
    path.write_bytes(PNG)

    record_shot(ctx, path, {"window": "main", "size": [1280, 800]}, label="benchmarks-default",
                subdir="benchmarks", warnings=["app warning: oops"])
    record_shot(ctx, tmp_path / "elsewhere.png", {}, label="ignored", subdir="", warnings=[])

    lines = (ctx.shots_dir / "index.jsonl").read_text(encoding="utf-8").splitlines()
    assert len(lines) == 1
    entry = json.loads(lines[0])
    assert entry["path"] == "benchmarks/001-benchmarks-default.png"
    assert (entry["label"], entry["dir"], entry["window"], entry["size"]) == (
        "benchmarks-default", "benchmarks", "main", [1280, 800])
    assert entry["command"] == ctx.command_line
    assert entry["warnings"] == ["app warning: oops"]
    assert entry["time"]


def test_batch_variables_sections_and_a_failure_summary(tmp_path: Path) -> None:
    ctx = make_context(tmp_path, [])
    source = tmp_path / "bench.json"
    source.write_text('{"name": "Old"}')
    text = "\n".join([
        "# a comment",
        f"set SRC={source.as_posix()}",
        "set TARGET=benchmarks/bench.json",
        "section manager",
        "put ${SRC}",
        "edit ${TARGET} --replace Old New",
        "edit ${TARGET} --replace Absent x",
        "put ${MISSING}",
        "rm ${TARGET}",
    ])

    outcome = run_batch(text, ctx, keep_going=True, fail_on_warnings=False, variables={})

    assert isinstance(outcome, BatchOutcome)
    assert outcome.steps == 4
    assert [failure["line"] for failure in outcome.failures] == [7, 8]
    assert "'Absent'" in outcome.failures[0]["error"]
    assert "${MISSING}" in outcome.failures[1]["error"]
    assert ctx.shot_subdir == "manager"
    assert not (ctx.session.data_dir / "benchmarks" / "bench.json").exists()


def test_batches_stop_at_the_first_failure_unless_kept_going(tmp_path: Path) -> None:
    ctx = make_context(tmp_path, [])
    variables: dict[str, str] = {}

    outcome = run_batch("rm missing.json\nset AFTER=1", ctx, keep_going=False, fail_on_warnings=False,
                        variables=variables)

    assert len(outcome.failures) == 1
    assert "AFTER" not in variables
    run_batch("options --keep-going\nrm missing.json\nset AFTER=1", ctx, keep_going=False, fail_on_warnings=False,
              variables=variables)
    assert variables["AFTER"] == "1"


def test_batch_lifecycle_steps_refuse_another_session(tmp_path: Path) -> None:
    ctx = make_context(tmp_path, [])
    outcome = run_batch("start --session other", ctx, keep_going=True, fail_on_warnings=False, variables={})

    assert "batch's session 'demo'" in outcome.failures[0]["error"]


def test_substitute_names_unknown_variables() -> None:
    assert substitute("click ${M}/saveButton", {"M": "win:benchmarkManagerDialog"}) == (
        "click win:benchmarkManagerDialog/saveButton")
    with pytest.raises(UsageError, match=r"\$\{X\}"):
        substitute("${X}", {})


def test_report_groups_shots_marks_coverage_and_embeds_images(tmp_path: Path) -> None:
    shots = tmp_path / "shots"
    entries = []
    for subdir, label, warnings in (("scenarios", "first-run-banner", []),
                                    ("manager", "dirty-close", ["app warning: x"])):
        path = next_shot_path(shots, label, subdir=subdir)
        path.write_bytes(PNG)
        entries.append({"path": path.relative_to(shots).as_posix(), "label": label, "dir": subdir, "window": "main",
                        "size": [10, 10], "command": f"shot --name {label}", "warnings": warnings})
    (shots / "index.jsonl").write_text("\n".join(json.dumps(entry) for entry in entries), encoding="utf-8")
    coverage = tmp_path / "coverage.json"
    coverage.write_text(json.dumps([
        {"area": "Scenarios", "state": "First-run banner", "shot": "first-run-banner"},
        {"area": "Manager", "state": "Drop line", "shot": "drop-line"},
    ]))
    summary = {"steps": 3, "failed": 1, "failures": [{"line": 4, "step": "click x", "error": "no item"}]}

    output = report.write_report(shots, tmp_path / "index.html", title="Tour: demo", coverage=coverage,
                                 summary=summary)
    page = output.read_text(encoding="utf-8")

    assert page.index("<h2>scenarios</h2>") < page.index("<h2>manager</h2>")
    assert 'class="captured">captured' in page and 'class="missing">missing' in page
    assert "first run banner" in page and "app warning: x" in page
    assert "data:image/png;base64," in page
    assert "http://" not in page and "https://" not in page
    assert "click x" in page and "no item" in page


def test_report_falls_back_to_listing_pngs(tmp_path: Path) -> None:
    (tmp_path / "manager").mkdir()
    (tmp_path / "002-main.png").write_bytes(PNG)
    (tmp_path / "manager" / "001-prompt.png").write_bytes(PNG)

    shots = report.load_shots(tmp_path)

    assert [(shot.group, shot.label) for shot in shots] == [("manager", "prompt"), ("", "main")]
    linked = report.build_report(shots, output=tmp_path / "index.html", title="t", coverage=[], embed=False)
    assert 'src="manager/001-prompt.png"' in linked


def test_coverage_files_must_list_area_state_and_shot(tmp_path: Path) -> None:
    coverage = tmp_path / "coverage.json"
    coverage.write_text('[{"area": "A"}]')
    with pytest.raises(ValueError, match=r"\[0\]"):
        report.load_coverage(coverage)


def test_every_checked_in_tour_parses() -> None:
    tours = sorted(TOURS_DIR.glob("*.txt"))
    assert {tour.stem for tour in tours} >= {"smoke", "unreachable", "composing-example", "full"}
    for tour in tours:
        for number, raw in enumerate(tour.read_text(encoding="utf-8").splitlines(), start=1):
            line = raw.strip()
            if not line or line.startswith("#") or line.split()[0] in ("set", "section", "options"):
                continue
            # Variables only fill in values; any placeholder parses the same way.
            words = shlex.split(substitute(line, VariableStub()))
            lifecycle = words[0] in ("start", "stop", "restart")
            try:
                parse_args(words, batch=not lifecycle)
            except UsageError as error:
                pytest.fail(f"{tour.name}:{number}: {error}")
    for coverage in TOURS_DIR.glob("*/coverage.json"):
        report.load_coverage(coverage)


class VariableStub(dict):
    def __contains__(self, key: object) -> bool:
        return True

    def __getitem__(self, key: str) -> str:
        return f"VAR_{key}"
