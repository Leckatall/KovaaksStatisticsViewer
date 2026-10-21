from __future__ import annotations

from collections import deque
from pathlib import Path
from typing import Callable

from scripts.build_runner.catalog import all_runners
from scripts.build_runner.cli import main, parse_args
from scripts.build_runner.config import BUILD_PRIORITIES, RunnerSettings, Toolchain
from scripts.build_runner.orchestrator import run_pipeline
from scripts.build_runner.process import CommandResult


def write_cache(path: Path, toolchain: Toolchain) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "\n".join(
            [
                "CMAKE_GENERATOR:INTERNAL=Ninja",
                "CMAKE_BUILD_TYPE:STRING=Debug",
                f"CMAKE_MAKE_PROGRAM:FILEPATH={toolchain.ninja_exe}",
                f"CMAKE_C_COMPILER:FILEPATH={toolchain.c_compiler}",
                f"CMAKE_CXX_COMPILER:FILEPATH={toolchain.cxx_compiler}",
                f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain.vcpkg_toolchain}",
                f"VCPKG_TARGET_TRIPLET:STRING={toolchain.vcpkg_triplet}",
                f"Protobuf_PROTOC_EXECUTABLE:FILEPATH={toolchain.protoc_exe}",
                f"Qt6_DIR:PATH={toolchain.qt6_dir}",
                "BUILD_TESTING:BOOL=ON",
                "KSV_BUILD_GALLERY:BOOL=ON",
            ]
        ),
        encoding="utf-8",
    )


def make_toolchain(values: dict[str, str], tmp_path: Path) -> Toolchain:
    ctest = Path(values["KSV_CMAKE_EXE"]).with_name("ctest.exe")
    ctest.write_bytes(b"ctest")
    return Toolchain(
        cmake_exe=Path(values["KSV_CMAKE_EXE"]),
        ninja_exe=Path(values["KSV_NINJA_EXE"]),
        c_compiler=Path(values["KSV_C_COMPILER"]),
        cxx_compiler=Path(values["KSV_CXX_COMPILER"]),
        vcpkg_toolchain=Path(values["KSV_VCPKG_TOOLCHAIN"]),
        vcpkg_triplet=values["KSV_VCPKG_TRIPLET"],
        vcpkg_bin_dir=Path(values["KSV_VCPKG_BIN_DIR"]),
        protoc_exe=Path(values["KSV_PROTOC_EXE"]),
        qt_bin_dir=Path(values["KSV_QT_BIN_DIR"]),
        qt6_dir=Path(values["KSV_QT6_DIR"]),
    )


class FakeExecutor:
    def __init__(
        self,
        build_dir: Path,
        toolchain: Toolchain,
        results: list[tuple[int, str]],
    ) -> None:
        self.build_dir = build_dir
        self.toolchain = toolchain
        self.results = deque(results)
        self.commands: list[list[str]] = []
        self.invocations: list[dict] = []

    def __call__(self, command, **kwargs) -> CommandResult:
        arguments = [str(value) for value in command]
        self.commands.append(arguments)
        self.invocations.append(kwargs)
        if "-S" in arguments:
            write_cache(self.build_dir / "CMakeCache.txt", self.toolchain)
        exit_code, output = self.results.popleft()
        return CommandResult(exit_code, kwargs["log_path"], output)


SETTINGS = RunnerSettings(jobs=8)


def ticking_clock(*values: float) -> Callable[[], float]:
    ticks = iter(values)
    return lambda: next(ticks)


def test_focused_success_is_one_line_and_build_parallelism_comes_from_settings(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    (build_dir / "domain_tests.exe").write_bytes(b"test")
    log_dir = repo / ".temp" / "build-and-test" / "run"
    executor = FakeExecutor(
        build_dir,
        toolchain,
        [
            (0, "-- Configuring done\n-- Generating done"),
            (0, "ninja: no work to do."),
            (0, "DomainSuite.\n  Works\n"),
            (0, "[==========] 1 test ran.\n[  PASSED  ] 1 test."),
        ],
    )
    output: list[str] = []

    result = run_pipeline(
        parse_args(["--scope", "domain"]),
        repo_root=repo,
        log_dir=log_dir,
        toolchain=toolchain,
        settings=RunnerSettings(jobs=3, priority="below_normal"),
        process_environment={},
        execute=executor,
        emit=output.append,
        clock=ticking_clock(10.0, 15.6),
    )

    assert result == 0
    assert output == ["Scope domain | jobs 3 | tests 1 | 5.6s | PASS"]
    assert executor.commands[1][-4:] == ["--parallel", "3", "--target", "domain_tests"]
    assert {invocation["creationflags"] for invocation in executor.invocations} == {
        BUILD_PRIORITIES["below_normal"]
    }


def test_build_only_target_builds_just_the_named_target(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    executor = FakeExecutor(
        build_dir,
        toolchain,
        [
            (0, "-- Configuring done"),
            (0, "ninja: no work to do."),
        ],
    )
    output: list[str] = []

    result = run_pipeline(
        parse_args(["--build-only", "--target", "ksv_driver"]),
        repo_root=repo,
        log_dir=repo / ".temp" / "build-and-test" / "run",
        toolchain=toolchain,
        settings=SETTINGS,
        process_environment={},
        execute=executor,
        emit=output.append,
        clock=ticking_clock(10.0, 12.0),
    )

    assert result == 0
    assert output == ["Targets ksv_driver | jobs 8 | tests 0 (build only) | 2.0s | PASS"]
    assert executor.commands[1][-4:] == ["--parallel", "8", "--target", "ksv_driver"]
    assert len(executor.commands) == 2


def test_qml_listing_uses_framework_functions_instead_of_parsing_sources(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    (build_dir / "ui_qml_tests.exe").write_bytes(b"test")
    executor = FakeExecutor(
        build_dir,
        toolchain,
        [
            (0, "-- Configuring done"),
            (0, "ninja: no work to do."),
            (0, "AppMenuBar::test_opensSettings()\nDashboard::benchmark_layout()\n"),
        ],
    )
    output: list[str] = []

    result = run_pipeline(
        parse_args(["--scope", "qml", "--list"]),
        repo_root=repo,
        log_dir=repo / ".temp" / "run",
        toolchain=toolchain,
        settings=SETTINGS,
        process_environment={},
        execute=executor,
        emit=output.append,
        clock=ticking_clock(0.0),
    )

    assert result == 0
    assert output == ["AppMenuBar::test_opensSettings", "Dashboard::benchmark_layout", "Total: 2"]
    assert executor.commands[2][-3:] == ["-input", str(repo / "tests" / "ui" / "qml"), "-functions"]
    assert executor.invocations[2]["logged_environment"]["QT_FORCE_STDERR_LOGGING"] == "1"


def test_compiler_failure_is_reduced_and_ends_with_summary(
    toolchain_tree: tuple[Path, dict[str, str]], fixtures_dir: Path, tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    compiler_output = (fixtures_dir / "compiler.log").read_text(encoding="utf-8").replace("C:/repo", repo.as_posix())
    executor = FakeExecutor(
        build_dir,
        toolchain,
        [(0, "-- Configuring done"), (1, compiler_output)],
    )
    output: list[str] = []

    result = run_pipeline(
        parse_args(["--scope", "domain"]),
        repo_root=repo,
        log_dir=repo / ".temp" / "run",
        toolchain=toolchain,
        settings=SETTINGS,
        process_environment={},
        execute=executor,
        emit=output.append,
        clock=ticking_clock(1.0, 4.0),
    )

    assert result == 1
    assert output == [
        "",
        "FAILED (build) | scope domain | 1 diagnostic | 3.0s",
        f"paths relative to {repo.resolve().as_posix()}",
        "target src/domain/CMakeFiles/ksv_domain.dir/run.cpp.obj",
        "",
        "B1",
        "  src/domain/run.cpp:54:1: error: 'this_is_a_deliberate_compiler_error' does not name a type",
        "     54 | this_is_a_deliberate_compiler_error",
        "        | ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
        "",
        f"log (read only if the above is insufficient): {(repo / '.temp' / 'run' / 'build.log').as_posix()}",
    ]


def test_silent_gtest_crash_is_named_without_requiring_a_log_read(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    (build_dir / "domain_tests.exe").write_bytes(b"test")
    executor = FakeExecutor(
        build_dir,
        toolchain,
        [
            (0, "-- Configuring done"),
            (0, "ninja: no work to do."),
            (0, "AgentFaultInjection.\n  DeliberateCrash\n"),
            (0xC0000005, "Running main() from gtest_main.cc"),
        ],
    )
    output: list[str] = []

    result = run_pipeline(
        parse_args(["--scope", "domain", "--match", "*Crash"]),
        repo_root=repo,
        log_dir=repo / ".temp" / "run",
        toolchain=toolchain,
        settings=SETTINGS,
        process_environment={},
        execute=executor,
        emit=output.append,
        clock=ticking_clock(2.0, 7.0),
    )

    assert result == 1
    assert output[1] == "FAILED (test) | scope domain | 1 of 1 failed | 5.0s"
    assert "(domain_tests wrote no usable report; recovered from console output)" in output
    start = output.index("R1")
    assert output[start + 1 : start + 3] == [
        "  Runner domain_tests exited with code 3221225477 (0xC0000005: access violation) "
        "before reporting a test failure.",
        "  Selected tests: AgentFaultInjection.DeliberateCrash",
    ]
    assert output[-1].endswith("domain_tests.log")


CTEST_LISTING = (
    "Test project build-agent\n"
    "  Test #1: domain_tests\n"
    "  Test #2: tst_Main\n"
    "  Test #3: contracts_qt_free\n"
    "\nTotal Tests: 3\n"
)


class CtestSideEffectExecutor(FakeExecutor):
    """Runs `before_ctest` when the ctest run (the one with --output-junit) is invoked."""

    def __init__(self, *args, before_ctest: Callable[[list[str]], None], **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self.before_ctest = before_ctest

    def __call__(self, command, **kwargs) -> CommandResult:
        arguments = [str(value) for value in command]
        if "--output-junit" in arguments:
            self.before_ctest(arguments)
        return super().__call__(command, **kwargs)


def run_scope_all(
    toolchain: Toolchain, repo: Path, results: list[tuple[int, str]], args: list[str],
    before_ctest: Callable[[list[str]], None] = lambda _: None,
) -> tuple[int, list[str], FakeExecutor]:
    build_dir = repo / "build-agent"
    executor = CtestSideEffectExecutor(build_dir, toolchain, results, before_ctest=before_ctest)
    output: list[str] = []
    result = run_pipeline(
        parse_args(args),
        repo_root=repo,
        log_dir=repo / ".temp" / "run",
        toolchain=toolchain,
        settings=SETTINGS,
        process_environment={},
        execute=executor,
        emit=output.append,
        clock=ticking_clock(0.0, 3.0, 3.0),
    )
    return result, output, executor


def write_gtest_report(path: Path, cases: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f'<testsuites tests="{cases}" failures="0"></testsuites>', encoding="utf-8")


def test_scope_all_counts_gtest_cases_from_the_reports_in_place_of_their_executables(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)

    result, output, _ = run_scope_all(
        toolchain,
        repo,
        [(0, "-- Configuring done"), (0, "ninja: no work to do."), (0, CTEST_LISTING), (0, "100% tests passed")],
        ["--scope", "all"],
        before_ctest=lambda _: write_gtest_report(build_dir / "tests" / "gtest-reports" / "domain_tests.xml", 5),
    )

    assert result == 0
    assert output == ["Scope all | jobs 8 | tests 7 | 3.0s | PASS"]


def test_scope_all_removes_last_runs_reports_before_ctest_starts(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    stale_gtest = build_dir / "tests" / "gtest-reports" / "domain_tests.xml"
    stale_qml = build_dir / "tests" / "ui" / "qml-test-logs" / "tst_Main.txt"
    write_gtest_report(stale_gtest, 5)
    stale_qml.parent.mkdir(parents=True)
    stale_qml.write_text("Totals: 1 passed, 0 failed", encoding="utf-8")
    present_when_ctest_started: list[bool] = []

    run_scope_all(
        toolchain,
        repo,
        [(0, "-- Configuring done"), (0, "ninja: no work to do."), (0, CTEST_LISTING), (0, "")],
        ["--scope", "all"],
        before_ctest=lambda _: present_when_ctest_started.extend([stale_gtest.exists(), stale_qml.exists()]),
    )

    assert present_when_ctest_started == [False, False]


def test_scope_all_failure_names_the_failing_cases_of_an_executable_and_counts_cases(
    toolchain_tree: tuple[Path, dict[str, str]], fixtures_dir: Path, tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    listing = CTEST_LISTING.replace("domain_tests", "ui_tests")

    def write_reports(arguments: list[str]) -> None:
        junit = Path(arguments[arguments.index("--output-junit") + 1])
        junit.parent.mkdir(parents=True, exist_ok=True)
        junit.write_text(
            (fixtures_dir / "ctest-junit-per-executable.xml").read_text(encoding="utf-8"), encoding="utf-8"
        )
        reports = build_dir / "tests" / "gtest-reports"
        reports.mkdir(parents=True, exist_ok=True)
        (reports / "ui_tests.xml").write_text(
            (fixtures_dir / "gtest-report.xml").read_text(encoding="utf-8"), encoding="utf-8"
        )

    result, output, _ = run_scope_all(
        toolchain,
        repo,
        [(0, "-- Configuring done"), (0, "ninja: no work to do."), (0, listing), (1, "")],
        ["--scope", "all"],
        before_ctest=write_reports,
    )

    assert result == 1
    assert output[1] == "FAILED (test) | scope all | 3 of 7 failed | 3.0s"
    text = "\n".join(output)
    assert "BenchmarkManagerVm.SaveSuccessUpdatesLocalBaselineFromAdmittedToken" in text
    assert "tests/ui/benchmark_manager_vm_test.cpp:211" in text
    assert "integration_tests" in text


def test_scope_all_counts_a_crashed_executables_cases_by_enumerating_it(
    toolchain_tree: tuple[Path, dict[str, str]], fixtures_dir: Path, tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    (build_dir / "ui_tests.exe").write_bytes(b"test")
    listing = CTEST_LISTING.replace("domain_tests", "ui_tests")

    def write_junit_without_report(arguments: list[str]) -> None:
        junit = Path(arguments[arguments.index("--output-junit") + 1])
        junit.parent.mkdir(parents=True, exist_ok=True)
        junit.write_text(
            (fixtures_dir / "ctest-junit-per-executable.xml").read_text(encoding="utf-8"), encoding="utf-8"
        )

    result, output, executor = run_scope_all(
        toolchain,
        repo,
        [
            (0, "-- Configuring done"),
            (0, "ninja: no work to do."),
            (0, listing),
            (1, ""),
            (0, "SuiteA.\n  One\n  Two\nSuiteB.\n  Three\n  Four\n  Five\n  Six\n"),
        ],
        ["--scope", "all"],
        before_ctest=write_junit_without_report,
    )

    assert result == 1
    assert executor.commands[-1][1:] == ["--gtest_list_tests"]
    assert output[1] == "FAILED (test) | scope all | 2 of 9 failed | 3.0s"


def test_scope_all_list_enumerates_every_runner_and_keeps_unrelated_ctest_tests(
    toolchain_tree: tuple[Path, dict[str, str]], tmp_path: Path
) -> None:
    _, values = toolchain_tree
    toolchain = make_toolchain(values, tmp_path)
    repo = tmp_path / "repo"
    build_dir = repo / "build-agent"
    build_dir.mkdir(parents=True)
    runners = all_runners()
    enumerations: list[tuple[int, str]] = []
    for index, runner in enumerate(runners):
        (build_dir / runner.relative_path).write_bytes(b"test")
        enumerations.append(
            (0, f"Suite{index}.\n  Case\n" if runner.kind == "gtest" else "Q::test_a()\nQ::test_b()\n")
        )

    result, output, executor = run_scope_all(
        toolchain,
        repo,
        [(0, "-- Configuring done"), (0, "ninja: no work to do."), (0, CTEST_LISTING), *enumerations],
        ["--scope", "all", "--list"],
    )

    assert result == 0
    assert len(executor.commands) == 3 + len(runners)
    assert output[0] == "Suite0.Case"
    assert "Q::test_a" in output
    assert output[-2:] == ["contracts_qt_free", f"Total: {len(runners) - 1 + 2 + 1}"]
    assert not any(line.lstrip().startswith("Test #") for line in output)


def test_usage_errors_are_written_only_to_stdout(capsys) -> None:
    result = main(["--jobs", "4"])

    captured = capsys.readouterr()
    assert result == 2
    assert captured.out == "error: unrecognized arguments: --jobs 4\n"
    assert captured.err == ""
