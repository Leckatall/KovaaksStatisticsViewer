from __future__ import annotations

from pathlib import Path

from scripts.build_runner.failures import (
    Diagnostic,
    Failure,
    compress_names,
    group,
    render_diagnostic_report,
    render_test_report,
)


REPO = Path("C:/repo")


def test_identical_reasons_collapse_into_one_key_that_still_names_every_test() -> None:
    failures = [
        Failure(f"Suite::test_{index}", "did not load", "tests/ui/qml/tst_X.qml:46") for index in range(3)
    ]

    reasons = group(failures)

    assert len(reasons) == 1
    assert reasons[0].key == "R1"
    assert reasons[0].tests == ["Suite::test_0", "Suite::test_1", "Suite::test_2"]


def test_distinct_reasons_are_keyed_in_first_seen_order() -> None:
    reasons = group(
        [
            Failure("A.one", "second cause", "a.cpp:2"),
            Failure("A.two", "first cause", "a.cpp:1"),
            Failure("A.three", "second cause", "a.cpp:2"),
        ]
    )

    assert [(reason.key, reason.tests) for reason in reasons] == [
        ("R1", ["A.one", "A.three"]),
        ("R2", ["A.two"]),
    ]


def test_shared_suite_prefix_is_written_once() -> None:
    assert compress_names(["Draft.AddScenario", "Draft.RemoveTier"]) == ["Draft.{AddScenario, RemoveTier}"]
    assert compress_names(["Case::test_a", "Case::test_b"]) == ["Case::{test_a, test_b}"]


def test_a_lone_name_keeps_its_suite_inline() -> None:
    assert compress_names(["Draft.AddScenario"]) == ["Draft.AddScenario"]


def test_data_driven_rows_fold_into_one_tag_list() -> None:
    names = ["Case::test_state(empty)", "Case::test_state(full)", "Case::test_other"]

    assert compress_names(names) == ["Case::{test_state(empty|full), test_other}"]


def test_names_without_a_suite_are_left_alone() -> None:
    assert compress_names(["tst_Workspace", "plain"]) == ["tst_Workspace", "plain"]


def test_several_assertions_in_one_test_count_as_one_failing_test() -> None:
    failures = [
        Failure("A.one", "first", "a.cpp:1"),
        Failure("A.one", "second", "a.cpp:2"),
    ]

    rendered = render_test_report(
        failures, scope="app", total=10, elapsed=1.0, repo_root=REPO, log_paths=[]
    )

    assert rendered[1] == "FAILED (test) | scope app | 1 of 10 failed | 1.0s"


def test_a_grouped_reason_lists_its_tests_after_the_message() -> None:
    failures = [Failure(f"Case::test_{index}", "did not load", "tests/ui/qml/tst_X.qml:46") for index in range(2)]

    rendered = render_test_report(
        failures, scope="qml", total=5, elapsed=2.0, repo_root=REPO, log_paths=[Path(".temp/run/qml.log")]
    )

    assert rendered == [
        "",
        "FAILED (test) | scope qml | 2 of 5 failed | 2.0s",
        "paths relative to C:/repo",
        "",
        "R1 x2 tests/ui/qml/tst_X.qml:46",
        "  did not load",
        "  -> Case::{test_0, test_1}",
        "",
        "log (read only if the above is insufficient): .temp/run/qml.log",
    ]


def test_a_single_failure_puts_its_test_name_on_the_key_line() -> None:
    rendered = render_test_report(
        [Failure("Draft.Move", "Expected equality", "tests/app/draft_test.cpp:408")],
        scope="app",
        total=3,
        elapsed=1.0,
        repo_root=REPO,
        log_paths=[],
    )

    assert rendered[4] == "R1 tests/app/draft_test.cpp:408 Draft.Move"
    assert rendered[5] == "  Expected equality"


def test_an_unlocated_failure_still_renders_with_its_name() -> None:
    rendered = render_test_report(
        [Failure("TimeoutCase", "Timeout")], scope="all", total=705, elapsed=9.0, repo_root=REPO, log_paths=[]
    )

    assert rendered[4] == "R1 TimeoutCase"
    assert rendered[5] == "  Timeout"


def test_notes_about_a_degraded_source_appear_under_the_header() -> None:
    rendered = render_test_report(
        [Failure("", "crashed")],
        scope="domain",
        total=1,
        elapsed=1.0,
        repo_root=REPO,
        log_paths=[],
        notes=["(domain_tests wrote no usable report)"],
    )

    assert rendered[3] == "(domain_tests wrote no usable report)"


def test_one_diagnostic_across_many_targets_names_each_target_by_key() -> None:
    rendered = render_diagnostic_report(
        [Diagnostic("widget.h:12: error: broken", ("a.obj", "b.obj", "c.obj"))],
        kind="build",
        scope="ui",
        elapsed=9.0,
        repo_root=REPO,
        log_paths=[],
    )

    assert rendered[1] == "FAILED (build) | scope ui | 1 diagnostic in 3 targets | 9.0s"
    assert rendered[3:6] == ["T1 = a.obj", "T2 = b.obj", "T3 = c.obj"]
    assert rendered[7] == "B1 T1,T2,T3"
    assert rendered[8] == "  widget.h:12: error: broken"


def test_a_single_target_is_named_once_in_the_header() -> None:
    rendered = render_diagnostic_report(
        [Diagnostic("error: broken", ("a.obj",))],
        kind="build",
        scope="ui",
        elapsed=1.0,
        repo_root=REPO,
        log_paths=[],
    )

    assert rendered[3] == "target a.obj"
    assert rendered[5] == "B1"


def test_later_errors_in_the_same_target_keep_their_location_but_drop_the_excerpt() -> None:
    cascade = [
        Diagnostic("a.cpp:1: error: root cause\n    1 | broken\n      | ^~~~", ("a.obj",)),
        Diagnostic("a.cpp:9: error: consequence\n    9 | uses\n      | ^~~~", ("a.obj",)),
        Diagnostic("a.cpp:12: error: another consequence\n   12 | uses\n      | ^~~~", ("a.obj",)),
    ]

    rendered = render_diagnostic_report(
        cascade, kind="build", scope="domain", elapsed=2.0, repo_root=REPO, log_paths=[]
    )

    assert "(source excerpt shown once per target; later errors there are listed by location)" in rendered
    assert rendered[-5:] == [
        "  a.cpp:1: error: root cause",
        "      1 | broken",
        "        | ^~~~",
        "B2 a.cpp:9: error: consequence",
        "B3 a.cpp:12: error: another consequence",
    ]


def test_the_log_pointer_is_omitted_when_there_is_nothing_to_point_at() -> None:
    rendered = render_test_report(
        [Failure("A.one", "boom", "a.cpp:1")], scope="app", total=1, elapsed=1.0, repo_root=REPO, log_paths=[]
    )

    assert not any("log (read only" in line for line in rendered)
