from __future__ import annotations

from ui_automation.cli import render
from ui_automation.formatting import describe, error_lines, tree_lines, window_line


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


def test_actions_report_the_resolved_target_and_its_state_after() -> None:
    result = {"target": {"type": "MenuItem", "text": "&About", "rect": [1, 2, 3, 4]},
              "after": {"type": "MenuItem", "text": "&About", "rect": [1, 2, 3, 4], "visible": False},
              "pos": [2, 4]}

    assert render("click", result) == [
        'clicked MenuItem "&About" @1,2 3x4 at 2,4',
        '  after: MenuItem "&About" @1,2 3x4 [hidden]',
    ]


def test_mouse_results_show_held_buttons() -> None:
    lines = render("mouse", {"action": "press", "target": [10.0, 20.0], "pos": [10, 20], "held": ["left"]})

    assert lines == ["pressed point 10,20 at 10,20", "  held: left"]
    assert render("mouse", {"action": "releaseAll"}) == ["released every held button"]


def test_window_lines_show_dialog_kinds() -> None:
    assert window_line({"key": "dirtyClosePrompt", "title": "Unsaved changes", "rect": [0, 0, 300, 120],
                        "visible": True, "kind": "fallback", "owner": "dirtyClosePrompt"}) == (
        'dirtyClosePrompt: "Unsaved changes" 300x120 (visible, fallback)')
    assert window_line({"key": "folderDialog", "title": "Pick", "visible": True, "kind": "native",
                        "capturable": False}) == 'folderDialog: "Pick" (visible, native, not capturable)'


def test_screenshots_warn_about_dialogs_they_cannot_show() -> None:
    lines = render("screenshot", {"path": "a.png", "uncapturedDialogs": ["kovaaksFolderDialog"], "context": "main"})

    assert lines[1] == "composited over window main"
    assert "kovaaksFolderDialog" in lines[2] and lines[2].startswith("warning:")
