from __future__ import annotations

from pathlib import Path

from scripts.build_runner.diagnostics import reduce_build_failure, reduce_configure_failure, reduce_test_failure


def load_fixture(fixtures_dir: Path, name: str) -> str:
    return (fixtures_dir / name).read_text(encoding="utf-8")


def test_compiler_failure_omits_commands_and_successful_edges(fixtures_dir: Path) -> None:
    reduced = reduce_build_failure(load_fixture(fixtures_dir, "compiler.log"), Path("C:/repo"))

    assert reduced == (
        "Target: src/domain/CMakeFiles/ksv_domain.dir/run.cpp.obj\n"
        "src/domain/run.cpp:54:1: error: 'this_is_a_deliberate_compiler_error' does not name a type\n"
        "   54 | this_is_a_deliberate_compiler_error\n"
        "      | ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    )


def test_linker_failure_omits_expanded_link_command(fixtures_dir: Path) -> None:
    reduced = reduce_build_failure(load_fixture(fixtures_dir, "linker.log"), Path("C:/repo"))

    assert reduced == (
        "Target: domain_tests.exe\n"
        "ld.exe: required symbol `ksv_deliberate_missing_symbol' not defined\n"
        "collect2.exe: error: ld returned 1 exit status"
    )


def test_gtest_failure_keeps_only_actionable_assertion(fixtures_dir: Path) -> None:
    reduced = reduce_test_failure(load_fixture(fixtures_dir, "gtest.log"), Path("C:/repo"))

    assert reduced == (
        "tests/domain/domain_test.cpp:189: Failure\n"
        "Failed\n"
        "deliberate assertion diagnostic\n"
        "[  FAILED  ] AgentFaultInjection.DeliberateAssertionFailure (0 ms)"
    )


def test_qml_failure_keeps_assertion_values_and_location(fixtures_dir: Path) -> None:
    reduced = reduce_test_failure(load_fixture(fixtures_dir, "qml.log"), Path("C:/repo"))

    assert reduced == (
        "FAIL!  : ui_qml_tests::AgentQmlFault::test_deliberateAssertionFailure() deliberate QML assertion diagnostic\n"
        "   Actual   (): 1\n"
        "   Expected (): 2\n"
        "tests/ui/qml/tst_AgentFault.qml(8) : failure location"
    )


def test_configure_failure_starts_at_cmake_error_and_normalizes_repo_path(fixtures_dir: Path) -> None:
    reduced = reduce_configure_failure(load_fixture(fixtures_dir, "configure.log"), Path("C:/repo"))

    assert reduced == (
        "CMake Error at CMakeLists.txt:14 (find_package):\n"
        "  Could not find a package configuration file provided by \"Qt6\"."
    )


def test_ctest_timeout_keeps_test_identity_and_last_output_without_passing_noise(fixtures_dir: Path) -> None:
    reduced = reduce_test_failure(load_fixture(fixtures_dir, "ctest-timeout.log"), Path("C:/repo"))

    assert reduced == (
        "7/8 Test #7: HangingIntegrationTest .......***Timeout  30.00 sec\n"
        "last useful line before timeout"
    )
