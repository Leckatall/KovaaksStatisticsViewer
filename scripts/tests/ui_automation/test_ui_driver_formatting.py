from __future__ import annotations

from ui_automation.cli import render
from ui_automation.formatting import describe, error_lines, tree_lines


def test_describe_is_one_compact_line() -> None:
    record = {
        "type": "TabButton",
        "name": "benchmarksTab",
        "text": "Benchmarks",
        "rect": [640, 28, 640, 21],
        "checked": True,
        "window": "settingsDialog",
    }

    assert describe(record) == (
        'TabButton#benchmarksTab "Benchmarks" @640,28 640x21 [checked=true] (window settingsDialog)'
    )


def test_tree_lines_indent_children_and_label_windows() -> None:
    nodes = [{"type": "Main", "window": "main", "rect": [0, 0, 10, 10],
              "children": [{"type": "Button", "name": "ok", "rect": [1, 2, 3, 4], "enabled": False}]}]

    assert tree_lines(nodes) == [
        "[window main] Main @0,0 10x10",
        "  Button#ok @1,2 3x4 [disabled]",
    ]


def test_error_lines_list_candidates_and_suggestions() -> None:
    lines = error_lines("ambiguous", {"candidates": [{"type": "Button", "text": "OK"}], "similar": ["okButton"]})

    assert lines == ['error: ambiguous', '  candidate: Button "OK"', "  similar names/ids/texts: okButton"]


def test_render_screenshot_points_at_other_windows() -> None:
    lines = render("screenshot", {"path": "shot.png", "otherVisibleWindows": ["settingsDialog"]})

    assert lines[0] == "shot.png"
    assert "settingsDialog" in lines[1]
