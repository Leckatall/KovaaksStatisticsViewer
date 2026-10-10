from __future__ import annotations

import pytest

from ui_automation.cli import UsageError, parse_args, parse_point_or_selector, parse_value


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
