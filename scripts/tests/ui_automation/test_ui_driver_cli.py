from __future__ import annotations

from pathlib import Path

import pytest

from ui_automation.cli import Context, UsageError, parse_args, parse_point_or_selector, parse_value, start_options
from ui_automation.session import Session, SessionError


def request_for(*arguments: str) -> tuple[str, dict]:
    ns = parse_args(list(arguments))
    return ns.build(ns)


def test_click_targets_a_selector_or_a_point() -> None:
    assert request_for("click", "benchmarksTab") == ("click", {"selector": "benchmarksTab"})
    assert request_for("click", "--at", "10", "20", "--window", "settingsDialog") == (
        "click",
        {"point": [10.0, 20.0], "window": "settingsDialog"},
    )


def test_click_variants_map_to_button_and_double() -> None:
    assert request_for("rclick", "item") == ("click", {"selector": "item", "button": "right"})
    assert request_for("dblclick", "item", "--mods", "ctrl") == (
        "click",
        {"selector": "item", "double": True, "modifiers": "ctrl"},
    )


def test_pointer_commands_need_a_target() -> None:
    ns = parse_args(["hover"])
    with pytest.raises(UsageError, match="selector or --at"):
        ns.build(ns)


def test_type_set_and_invoke_parse_json_values() -> None:
    assert request_for("type", "Pasu", "--into", "scenarioSearchField", "--clear", "--enter") == (
        "type",
        {"text": "Pasu", "selector": "scenarioSearchField", "clear": True, "enter": True},
    )
    assert request_for("set", "workspaceTabBar", "currentIndex", "1") == (
        "set",
        {"selector": "workspaceTabBar", "property": "currentIndex", "value": 1},
    )
    assert request_for("invoke", "dialog", "open")[1]["args"] == []
    assert request_for("invoke", "vm", "select", "3", "name", '{"a": true}')[1]["args"] == [3, "name", {"a": True}]


def test_drag_accepts_points_and_selectors() -> None:
    assert request_for("drag", "12,30", "dropZone") == ("drag", {"from": [12.0, 30.0], "to": "dropZone"})


def test_scroll_requires_a_delta() -> None:
    ns = parse_args(["scroll", "list"])
    with pytest.raises(UsageError, match="--dy"):
        ns.build(ns)
    assert request_for("scroll", "list", "--dy", "3") == ("scroll", {"selector": "list", "dy": 3})


def test_wait_validates_its_conditions() -> None:
    ns = parse_args(["wait"])
    with pytest.raises(UsageError, match="selector and/or --expr"):
        ns.build(ns)
    ns = parse_args(["wait", "tabBar", "--prop", "currentIndex"])
    with pytest.raises(UsageError, match="--equals"):
        ns.build(ns)
    assert request_for("wait", "tabBar", "--prop", "currentIndex", "--equals", "1", "--timeout", "900") == (
        "wait",
        {"timeoutMs": 900, "selector": "tabBar", "property": "currentIndex", "equals": 1},
    )


def test_resize_parses_size() -> None:
    assert request_for("resize", "800x600") == ("resize", {"width": 800, "height": 600})
    ns = parse_args(["resize", "huge"])
    with pytest.raises(UsageError, match="1280x800"):
        ns.build(ns)


def test_session_and_json_flags_work_before_or_after_the_subcommand() -> None:
    before = parse_args(["--session", "a", "--json", "windows"])
    after = parse_args(["windows", "--session", "b", "--json"])

    assert (before.global_session, before.global_json) == ("a", True)
    assert (after.session, after.json) == ("b", True)


def test_batch_lines_cannot_run_lifecycle_commands() -> None:
    with pytest.raises(UsageError):
        parse_args(["start"], batch=True)
    assert parse_args(["click", "x", "--shot"], batch=True).shot


def test_value_and_point_parsing() -> None:
    assert parse_value("true") is True
    assert parse_value("#ff0000") == "#ff0000"
    assert parse_point_or_selector("1.5,2") == [1.5, 2.0]
    assert parse_point_or_selector("text:a,b") == "text:a,b"


def make_context(tmp_path: Path) -> tuple[Context, list[str]]:
    lines: list[str] = []
    session = Session(tmp_path, "demo")
    session.data_dir.mkdir(parents=True)
    return Context(session=session, json_output=False, out=lines.append), lines


def run_handler(ctx: Context, *arguments: str) -> int:
    ns = parse_args(list(arguments), batch=True)
    return ns.handler(ns, ctx)


def test_start_validates_composition_options_before_launching(tmp_path: Path) -> None:
    overlay = tmp_path / "overlay"
    overlay.mkdir()
    ns = parse_args(["start", "--overlay", str(overlay), "--setting", "view/a=false", "--env", "X=1"])
    options = start_options(ns)
    assert options.overlays == (str(overlay.resolve()),)
    assert options.settings == ("view/a=false",)
    assert options.env == ("X=1",)

    with pytest.raises(UsageError, match="missing-dir"):
        start_options(parse_args(["start", "--overlay", str(tmp_path / "missing-dir")]))
    with pytest.raises(SessionError, match="'novalue'"):
        start_options(parse_args(["start", "--setting", "novalue"]))
    with pytest.raises(SessionError, match="'=1'"):
        start_options(parse_args(["start", "--env", "=1"]))


def test_put_routes_files_by_extension_or_to(tmp_path: Path) -> None:
    ctx, lines = make_context(tmp_path)
    perf = tmp_path / "run.perf"
    perf.write_bytes(b"perf")
    bench = tmp_path / "b.json"
    bench.write_text("{}")
    other = tmp_path / "notes.txt"
    other.write_text("x")

    assert run_handler(ctx, "put", str(perf), str(bench)) == 0
    assert (ctx.session.data_dir / "FPSAimTrainer" / "performances" / "run.perf").read_bytes() == b"perf"
    assert (ctx.session.data_dir / "benchmarks" / "b.json").exists()
    assert "put FPSAimTrainer/performances/run.perf" in lines
    with pytest.raises(UsageError, match="--to"):
        run_handler(ctx, "put", str(other))
    assert run_handler(ctx, "put", str(other), "--to", ".") == 0
    assert (ctx.session.data_dir / "notes.txt").exists()
    with pytest.raises(SessionError, match="outside"):
        run_handler(ctx, "put", str(other), "--to", "../..")


def test_rm_refuses_paths_outside_data_and_missing_files(tmp_path: Path) -> None:
    ctx, _ = make_context(tmp_path)
    target = ctx.session.data_dir / "benchmarks" / "driver-demo.json"
    target.parent.mkdir()
    target.write_text("{}")
    (ctx.session.directory / "launch.json").write_text("{}")

    with pytest.raises(SessionError, match="outside"):
        run_handler(ctx, "rm", "../launch.json")
    with pytest.raises(UsageError, match="not a file"):
        run_handler(ctx, "rm", "benchmarks/missing.json")
    assert run_handler(ctx, "rm", "benchmarks/driver-demo.json") == 0
    assert not target.exists()
    assert (ctx.session.directory / "launch.json").exists()


def test_edit_replaces_text_or_whole_files(tmp_path: Path) -> None:
    ctx, _ = make_context(tmp_path)
    target = ctx.session.data_dir / "b.json"
    target.write_text('{"name": "Old", "x": 1}', encoding="utf-8")

    assert run_handler(ctx, "edit", "b.json", "--replace", "Old", "New", "--replace", '"x": 1', '"x": 2') == 0
    assert target.read_text(encoding="utf-8") == '{"name": "New", "x": 2}'
    with pytest.raises(UsageError, match="'Absent'"):
        run_handler(ctx, "edit", "b.json", "--replace", "Absent", "y")
    with pytest.raises(UsageError, match="--replace OLD NEW"):
        run_handler(ctx, "edit", "b.json")
    source = tmp_path / "replacement.json"
    source.write_text("[]")
    assert run_handler(ctx, "edit", "b.json", "--from", str(source)) == 0
    assert target.read_text() == "[]"


def test_mouse_stages_map_to_the_mouse_command() -> None:
    assert request_for("mouse", "press", "cell_x", "--offset", "4", "8") == (
        "mouse", {"selector": "cell_x", "offset": [4.0, 8.0], "action": "press"})
    assert request_for("mouse", "move", "--at", "10", "20", "--steps", "6") == (
        "mouse", {"point": [10.0, 20.0], "action": "move", "steps": 6})
    assert request_for("mouse", "release") == ("mouse", {"action": "release"})
    assert request_for("mouse", "press", "x", "--button", "right", "--mods", "ctrl") == (
        "mouse", {"selector": "x", "action": "press", "button": "right", "modifiers": "ctrl"})
    ns = parse_args(["mouse", "press"])
    with pytest.raises(UsageError, match="selector or --at"):
        ns.build(ns)


def test_hover_type_find_and_shot_options() -> None:
    assert request_for("hover", "text:View", "--steps", "3") == ("hover", {"selector": "text:View", "steps": 3})
    assert request_for("type", " (x)", "--into", "field", "--at-start")[1] == {
        "text": " (x)", "selector": "field", "atStart": True}
    assert request_for("find", "obj:*Dialog", "--objects")[1] == {
        "selector": "obj:*Dialog", "limit": 50, "objects": True}
    assert parse_args(["shot", "--window", "dirtyClosePrompt", "--with-context"]).with_context


def test_profile_spec_defaults_to_the_empty_dataset_and_rejects_synthetic_ones(tmp_path: Path) -> None:
    spec = tmp_path / "spec.json"
    spec.write_text('{"scenarios": []}')

    options = start_options(parse_args(["start", "--profile-spec", str(spec)]))
    assert (options.dataset, options.profile_spec) == ("empty", str(spec.resolve()))
    assert start_options(parse_args(["start", "--dataset", "first-run", "--profile-spec", str(spec)])).dataset == "first-run"
    assert start_options(parse_args(["start"])).dataset == "rich"
    with pytest.raises(UsageError, match="'rich'"):
        start_options(parse_args(["start", "--dataset", "rich", "--profile-spec", str(spec)]))
    with pytest.raises(UsageError, match="not a file"):
        start_options(parse_args(["start", "--profile-spec", str(tmp_path / "missing.json")]))


def test_inject_values_are_validated_before_launch() -> None:
    options = start_options(parse_args(["start", "--inject", "decoder=delay:300", "--inject",
                                        "benchmarkStore.write=fail:after:1,delay:5"]))
    assert options.injections == ("decoder=delay:300", "benchmarkStore.write=fail:after:1,delay:5")
    with pytest.raises(SessionError, match="unknown port 'disk'.*seriesStore"):
        start_options(parse_args(["start", "--inject", "disk=fail"]))
    with pytest.raises(SessionError, match="no op 'write'"):
        start_options(parse_args(["start", "--inject", "csv.write=fail"]))
    with pytest.raises(SessionError, match="unknown effect 'fail:0'"):
        start_options(parse_args(["start", "--inject", "csv=fail:0"]))
