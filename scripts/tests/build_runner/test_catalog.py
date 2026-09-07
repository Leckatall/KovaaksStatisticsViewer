from __future__ import annotations

from scripts.build_runner.catalog import (
    catalog_for_scope,
    matches_pattern,
    parse_gtest_list,
    parse_qml_functions,
)


def test_gtest_list_parser_ignores_comments_and_combines_suite_and_case() -> None:
    output = "SuiteOne.  # TypeParam = int\n  FirstCase\n  SecondCase  # GetParam() = 4\nSuiteTwo.\n  ThirdCase\n"

    assert parse_gtest_list(output) == [
        "SuiteOne.FirstCase",
        "SuiteOne.SecondCase",
        "SuiteTwo.ThirdCase",
    ]


def test_qml_functions_parser_uses_framework_enumeration_output() -> None:
    output = "AppMenuBar::test_opensSettings()\nDashboard::benchmark_layout()\n"

    assert parse_qml_functions(output) == [
        "AppMenuBar::test_opensSettings",
        "Dashboard::benchmark_layout",
    ]


def test_matching_is_case_insensitive_and_uses_shell_wildcards() -> None:
    assert matches_pattern("ProfileServiceTest.LoadsProfile", "*profileservice*")
    assert not matches_pattern("ProfileServiceTest.LoadsProfile", "Graph*")


def test_ui_scope_contains_native_and_qml_runners() -> None:
    runners = catalog_for_scope("ui")

    assert [runner.target for runner in runners] == ["ui_tests", "ui_qml_tests"]


def test_all_scope_uses_ctest_instead_of_direct_runners() -> None:
    assert catalog_for_scope("all") == []
