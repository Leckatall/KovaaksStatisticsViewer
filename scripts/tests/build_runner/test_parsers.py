from __future__ import annotations

from pathlib import Path

from scripts.build_runner.failures import Failure
from scripts.build_runner.parsers import (
    CtestCase,
    failures_from_ctest,
    parse_ctest_junit,
    parse_gtest_text,
    parse_gtest_xml,
    parse_qt_txt,
)


REPO = Path("C:/repo")


def test_gtest_xml_splits_the_location_off_the_failure_body(fixtures_dir: Path) -> None:
    failures = parse_gtest_xml(fixtures_dir / "gtest-report.xml", REPO)

    assert failures[0] == Failure(
        test="BenchmarkManagerVm.SaveSuccessUpdatesLocalBaselineFromAdmittedToken",
        message="Value of: f.vm->dirty()\n  Actual: false\nExpected: true",
        location="tests/ui/benchmark_manager_vm_test.cpp:211",
    )


def test_gtest_xml_keeps_every_assertion_of_a_multi_assertion_test(fixtures_dir: Path) -> None:
    failures = parse_gtest_xml(fixtures_dir / "gtest-report.xml", REPO)

    repeated = [failure for failure in failures if failure.test.endswith("EditorCommandErrorSurfacesInReturnMap")]
    assert [failure.location for failure in repeated] == [
        "tests/ui/benchmark_manager_vm_test.cpp:352",
        "tests/ui/benchmark_manager_vm_test.cpp:353",
    ]


def test_gtest_xml_ignores_passing_cases(fixtures_dir: Path) -> None:
    failures = parse_gtest_xml(fixtures_dir / "gtest-report.xml", REPO)

    assert not any("Discard" in failure.test for failure in failures)


def test_gtest_brief_text_attaches_assertions_to_the_name_printed_after_them(fixtures_dir: Path) -> None:
    failures = parse_gtest_text((fixtures_dir / "gtest.log").read_text(encoding="utf-8"), REPO)

    assert failures == [
        Failure(
            test="AgentFaultInjection.DeliberateAssertionFailure",
            message="Failed\ndeliberate assertion diagnostic",
            location="tests/domain/domain_test.cpp:189",
        )
    ]


def test_qt_text_keeps_the_multi_line_message_with_its_location(fixtures_dir: Path) -> None:
    failures = parse_qt_txt((fixtures_dir / "qml.log").read_text(encoding="utf-8"), REPO)

    assert failures == [
        Failure(
            test="AgentQmlFault::test_deliberateAssertionFailure",
            message="deliberate QML assertion diagnostic\n   Actual   (): 1\n   Expected (): 2",
            location="tests/ui/qml/tst_AgentFault.qml:8",
        )
    ]


def test_qt_text_separates_incidents_that_share_a_multi_line_message(fixtures_dir: Path) -> None:
    failures = parse_qt_txt((fixtures_dir / "qml-duplicates.log").read_text(encoding="utf-8"), REPO)

    assert [failure.test for failure in failures] == [
        "BenchmarkWorkspaceStateTest::test_emptyLibraryOffersCreateOrImport",
        "BenchmarkWorkspaceStateTest::test_headerIsPresentInEveryState(empty)",
        "BenchmarkWorkspaceStateTest::test_headerIsPresentInEveryState(problems)",
        "OtherCase::test_unrelated",
    ]
    assert len({failure.message for failure in failures[:3]}) == 1


def test_ctest_junit_reports_the_total_and_skips_passing_cases(fixtures_dir: Path) -> None:
    cases, total = parse_ctest_junit(fixtures_dir / "ctest-junit.xml")

    assert total == 705
    assert [case.name for case in cases] == [
        "BenchmarkDraft.MoveScenarioBackToUncategorized",
        "HangingIntegrationTest",
    ]


def test_ctest_failures_recover_the_assertion_from_the_captured_output(fixtures_dir: Path) -> None:
    cases, _ = parse_ctest_junit(fixtures_dir / "ctest-junit.xml")

    failures = failures_from_ctest(cases, REPO, fixtures_dir / "no-qml-logs-here")

    assert failures[0] == Failure(
        test="BenchmarkDraft.MoveScenarioBackToUncategorized",
        message="Expected equality of these values:\n  draft.uncategorized.front().id\n  entry",
        location="tests/app/benchmark_draft_test.cpp:408",
    )


def test_a_ctest_case_with_no_output_falls_back_to_its_status(fixtures_dir: Path) -> None:
    cases, _ = parse_ctest_junit(fixtures_dir / "ctest-junit.xml")

    failures = failures_from_ctest(cases, REPO, fixtures_dir / "no-qml-logs-here")

    assert failures[-1] == Failure(test="HangingIntegrationTest", message="Timeout", location="")


def test_a_qml_ctest_case_is_expanded_from_its_own_report(fixtures_dir: Path, tmp_path: Path) -> None:
    logs = tmp_path / "qml-test-logs"
    logs.mkdir()
    (logs / "tst_BenchmarkWorkspace.txt").write_text(
        (fixtures_dir / "qml-duplicates.log").read_text(encoding="utf-8"), encoding="utf-8"
    )

    failures = failures_from_ctest([CtestCase("tst_BenchmarkWorkspace", "Failed", "")], REPO, logs)

    assert len(failures) == 4
    assert failures[0].location == "tests/ui/qml/tst_BenchmarkWorkspace.qml:46"


def test_a_truncated_report_is_treated_as_unusable_rather_than_crashing_the_runner(tmp_path: Path) -> None:
    from scripts.build_runner.orchestrator import UNUSABLE_REPORT

    truncated = tmp_path / "ui_tests.xml"
    truncated.write_text('<?xml version="1.0"?>\n<testsuites><testsuite name="A"', encoding="utf-8")

    try:
        parse_gtest_xml(truncated, REPO)
    except UNUSABLE_REPORT:
        return
    raise AssertionError("a truncated report should raise something the runner catches")
