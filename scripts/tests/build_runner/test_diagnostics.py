from __future__ import annotations

from pathlib import Path

from scripts.build_runner.diagnostics import (
    reduce_build_failure,
    reduce_configure_failure,
    reduce_test_failure,
)
from scripts.build_runner.failures import Diagnostic


REPO = Path("C:/repo")


def load_fixture(fixtures_dir: Path, name: str) -> str:
    return (fixtures_dir / name).read_text(encoding="utf-8")


def test_compiler_failure_omits_commands_and_successful_edges(fixtures_dir: Path) -> None:
    reduced = reduce_build_failure(load_fixture(fixtures_dir, "compiler.log"), REPO)

    assert reduced == [
        Diagnostic(
            "src/domain/run.cpp:54:1: error: 'this_is_a_deliberate_compiler_error' does not name a type\n"
            "   54 | this_is_a_deliberate_compiler_error\n"
            "      | ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
            ("src/domain/CMakeFiles/ksv_domain.dir/run.cpp.obj",),
        )
    ]


def test_one_header_error_is_reported_once_with_every_target_it_broke() -> None:
    output = "\n".join(
        line
        for index in range(3)
        for line in (
            f"[{index + 1}/40] Building CXX object src/ui/CMakeFiles/ksv_ui.dir/f{index}.cpp.obj",
            f"FAILED: src/ui/CMakeFiles/ksv_ui.dir/f{index}.cpp.obj ",
            "C:/repo/src/ui/widget.h:12:5: error: 'Broken' does not name a type",
            "   12 |     Broken thing;",
        )
    )

    reduced = reduce_build_failure(output, REPO)

    assert reduced == [
        Diagnostic(
            "src/ui/widget.h:12:5: error: 'Broken' does not name a type\n   12 |     Broken thing;",
            (
                "src/ui/CMakeFiles/ksv_ui.dir/f0.cpp.obj",
                "src/ui/CMakeFiles/ksv_ui.dir/f1.cpp.obj",
                "src/ui/CMakeFiles/ksv_ui.dir/f2.cpp.obj",
            ),
        )
    ]


def test_linker_failure_omits_expanded_link_command(fixtures_dir: Path) -> None:
    reduced = reduce_build_failure(load_fixture(fixtures_dir, "linker.log"), REPO)

    assert [diagnostic.message for diagnostic in reduced] == [
        "ld.exe: required symbol `ksv_deliberate_missing_symbol' not defined",
        "collect2.exe: error: ld returned 1 exit status",
    ]
    assert all(diagnostic.targets == ("domain_tests.exe",) for diagnostic in reduced)


def test_configure_failure_starts_at_cmake_error_and_normalizes_repo_path(fixtures_dir: Path) -> None:
    reduced = reduce_configure_failure(load_fixture(fixtures_dir, "configure.log"), REPO)

    assert reduced == [
        Diagnostic(
            'CMake Error at CMakeLists.txt:14 (find_package):\n'
            '  Could not find a package configuration file provided by "Qt6".'
        )
    ]


def test_gtest_text_fallback_keeps_only_the_actionable_assertion(fixtures_dir: Path) -> None:
    reduced = reduce_test_failure(load_fixture(fixtures_dir, "gtest.log"), REPO)

    assert [(failure.test, failure.location) for failure in reduced] == [
        ("AgentFaultInjection.DeliberateAssertionFailure", "tests/domain/domain_test.cpp:189")
    ]


def test_qml_text_fallback_keeps_assertion_values_and_location(fixtures_dir: Path) -> None:
    reduced = reduce_test_failure(load_fixture(fixtures_dir, "qml.log"), REPO)

    assert [(failure.test, failure.location) for failure in reduced] == [
        ("AgentQmlFault::test_deliberateAssertionFailure", "tests/ui/qml/tst_AgentFault.qml:8")
    ]


def test_output_no_parser_understands_is_labelled_rather_than_dumped_silently() -> None:
    reduced = reduce_test_failure("> command\nENV X=1\nsomething went wrong\n", REPO)

    assert len(reduced) == 1
    assert reduced[0].test == ""
    assert reduced[0].message == "(no diagnostic recognized; raw tail follows)\nsomething went wrong"


def test_an_unrecognized_build_failure_keeps_a_bounded_tail() -> None:
    reduced = reduce_build_failure("\n".join(f"noise {index}" for index in range(200)), REPO)

    assert len(reduced) == 1
    assert reduced[0].message.startswith("(no diagnostic recognized")
    assert len(reduced[0].message.splitlines()) == 41
