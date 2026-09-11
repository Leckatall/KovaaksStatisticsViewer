from __future__ import annotations

import os
from pathlib import Path
import re
import time
from typing import Callable, Mapping
from xml.etree import ElementTree

from .catalog import RunnerSpec, catalog_for_scope, matches_pattern, parse_gtest_list, parse_qml_functions
from .config import Toolchain, ToolchainError, assert_cache, write_fingerprint
from .diagnostics import reduce_build_failure, reduce_configure_failure, reduce_test_failure
from .failures import Failure, render_diagnostic_report, render_test_report
from .parsers import failures_from_ctest, parse_ctest_junit, parse_gtest_xml, parse_qt_txt
from .process import CommandResult, describe_exit_code, run_command


# A runner killed mid-report leaves a truncated file; ElementTree raises a SyntaxError subclass for it.
UNUSABLE_REPORT = (OSError, ValueError, ElementTree.ParseError)


Execute = Callable[..., CommandResult]
Emit = Callable[[str], None]
BUILD_JOBS = 8


def _summary_duration(started: float, clock: Callable[[], float]) -> float:
    return max(0.0, clock() - started)


def _command_environment(
    base: Mapping[str, str],
    toolchain: Toolchain,
    additions: Mapping[str, str] | None = None,
) -> dict[str, str]:
    environment = dict(base)
    path_entries = [
        str(toolchain.cxx_compiler.parent),
        str(toolchain.vcpkg_bin_dir),
        str(toolchain.qt_bin_dir),
    ]
    if environment.get("PATH"):
        path_entries.append(environment["PATH"])
    environment["PATH"] = os.pathsep.join(path_entries)
    environment.update(additions or {})
    return environment


def _invoke(
    execute: Execute,
    command: list[str | Path],
    *,
    log_path: Path,
    repo_root: Path,
    process_environment: Mapping[str, str],
    toolchain: Toolchain,
    runner_environment: Mapping[str, str] | None,
    show_all_output: bool,
    phase: str,
    emit: Emit,
) -> CommandResult:
    additions = runner_environment or {}
    return execute(
        command,
        log_path=log_path,
        cwd=repo_root,
        environment=_command_environment(process_environment, toolchain, additions),
        logged_environment=additions,
        show_all_output=show_all_output,
        phase=phase,
        emit=emit,
    )


def _configure_command(repo_root: Path, build_dir: Path, toolchain: Toolchain) -> list[str | Path]:
    return [
        toolchain.cmake_exe,
        "-S",
        repo_root,
        "-B",
        build_dir,
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_MAKE_PROGRAM={toolchain.ninja_exe}",
        f"-DCMAKE_C_COMPILER={toolchain.c_compiler}",
        f"-DCMAKE_CXX_COMPILER={toolchain.cxx_compiler}",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain.vcpkg_toolchain}",
        f"-DVCPKG_TARGET_TRIPLET={toolchain.vcpkg_triplet}",
        f"-DProtobuf_PROTOC_EXECUTABLE={toolchain.protoc_exe}",
        f"-DQt6_DIR={toolchain.qt6_dir}",
        "-DBUILD_TESTING=ON",
        "-DKSV_BUILD_GALLERY=ON",
    ]


def _silent_runner_failure(runner: RunnerSpec, exit_code: int, selected: list[str]) -> Failure:
    if len(selected) <= 10:
        selection = ", ".join(selected)
    else:
        selection = f"{len(selected)} tests in scope"
    return Failure(
        test="",
        message=(
            f"Runner {runner.target} exited with code {describe_exit_code(exit_code)} "
            "before reporting a test failure.\n"
            f"Selected tests: {selection}"
        ),
    )


def _read_report(read: Callable[[], list[Failure]]) -> list[Failure]:
    """A runner that died mid-report leaves no usable file; the caller falls back to its output."""
    try:
        return read()
    except UNUSABLE_REPORT:
        return []


def run_pipeline(
    args,
    *,
    repo_root: Path,
    log_dir: Path,
    toolchain: Toolchain,
    process_environment: Mapping[str, str],
    fingerprint: dict[str, object] | None = None,
    execute: Execute = run_command,
    emit: Emit = print,
    clock: Callable[[], float] = time.monotonic,
) -> int:
    started = clock()
    build_dir = repo_root / "build-agent"
    runners = catalog_for_scope(args.scope)
    log_dir.mkdir(parents=True, exist_ok=True)

    def report(lines: list[str]) -> None:
        for line in lines:
            emit(line)

    configure_log = log_dir / "configure.log"
    configure = _invoke(
        execute,
        _configure_command(repo_root, build_dir, toolchain),
        log_path=configure_log,
        repo_root=repo_root,
        process_environment=process_environment,
        toolchain=toolchain,
        runner_environment=None,
        show_all_output=args.show_all_output,
        phase="configure",
        emit=emit,
    )
    if configure.exit_code:
        report(
            render_diagnostic_report(
                reduce_configure_failure(configure.output, repo_root),
                kind="configure",
                scope=args.scope,
                elapsed=_summary_duration(started, clock),
                repo_root=repo_root,
                log_paths=[configure_log],
            )
        )
        return configure.exit_code

    assert_cache(build_dir / "CMakeCache.txt", toolchain)
    if fingerprint is not None:
        write_fingerprint(build_dir / ".ksv-toolchain-fingerprint.json", fingerprint)

    build_command: list[str | Path] = [
        toolchain.cmake_exe,
        "--build",
        build_dir,
        "--parallel",
        str(BUILD_JOBS),
    ]
    if args.scope != "all":
        build_command.extend(["--target", *(runner.target for runner in runners)])
    build_log = log_dir / "build.log"
    build = _invoke(
        execute,
        build_command,
        log_path=build_log,
        repo_root=repo_root,
        process_environment=process_environment,
        toolchain=toolchain,
        runner_environment=None,
        show_all_output=args.show_all_output,
        phase="build",
        emit=emit,
    )
    if build.exit_code:
        report(
            render_diagnostic_report(
                reduce_build_failure(build.output, repo_root),
                kind="build",
                scope=args.scope,
                elapsed=_summary_duration(started, clock),
                repo_root=repo_root,
                log_paths=[build_log],
            )
        )
        return build.exit_code

    if args.build_only:
        elapsed = _summary_duration(started, clock)
        emit(f"Scope {args.scope} | jobs {BUILD_JOBS} | tests 0 (build only) | {elapsed:.1f}s | PASS")
        return 0

    ctest_exe = toolchain.cmake_exe.with_name("ctest.exe")
    if not ctest_exe.is_file():
        raise ToolchainError(f"CTest executable not found beside CMake: {ctest_exe}")
    if args.scope == "all":
        list_log = log_dir / "ctest-list.log"
        listed = _invoke(
            execute,
            [ctest_exe, "--test-dir", build_dir, "-N"],
            log_path=list_log,
            repo_root=repo_root,
            process_environment=process_environment,
            toolchain=toolchain,
            runner_environment=None,
            show_all_output=args.show_all_output,
            phase="test discovery",
            emit=emit,
        )
        if listed.exit_code:
            report(
                render_test_report(
                    reduce_test_failure(listed.output, repo_root),
                    scope=args.scope,
                    total=0,
                    elapsed=_summary_duration(started, clock),
                    repo_root=repo_root,
                    log_paths=[list_log],
                )
            )
            return listed.exit_code
        count_match = re.search(r"Total Tests:\s+(\d+)", listed.output)
        test_count = int(count_match.group(1)) if count_match else 0
        if args.list:
            for line in listed.output.splitlines():
                if re.match(r"^\s*Test\s+#\d+:", line):
                    emit(line)
            emit(f"Total: {test_count}")
            return 0

        ctest_log = log_dir / "ctest.log"
        junit_path = log_dir / "ctest.xml"
        tested = _invoke(
            execute,
            [ctest_exe, "--test-dir", build_dir, "--output-on-failure", "--output-junit", junit_path],
            log_path=ctest_log,
            repo_root=repo_root,
            process_environment=process_environment,
            toolchain=toolchain,
            runner_environment=None,
            show_all_output=args.show_all_output,
            phase="test",
            emit=emit,
        )
        if tested.exit_code:
            qml_log_dir = build_dir / "tests" / "ui" / "qml-test-logs"
            failures: list[Failure] = []
            note = ""
            if junit_path.is_file():
                cases, reported_total = _read_ctest_junit(junit_path, repo_root, qml_log_dir)
                failures = cases
                test_count = reported_total or test_count
            if not failures:
                failures = reduce_test_failure(tested.output, repo_root)
                note = "(CTest wrote no usable JUnit report; recovered from console output)"
            report(
                render_test_report(
                    failures,
                    scope=args.scope,
                    total=test_count,
                    elapsed=_summary_duration(started, clock),
                    repo_root=repo_root,
                    log_paths=[ctest_log],
                    notes=[note] if note else [],
                )
            )
            return tested.exit_code
        elapsed = _summary_duration(started, clock)
        emit(f"Scope all | jobs {BUILD_JOBS} | tests {test_count} | {elapsed:.1f}s | PASS")
        return 0

    qml_input = repo_root / "tests" / "ui" / "qml"
    catalog: list[tuple[RunnerSpec, str]] = []
    for runner in runners:
        executable = build_dir / runner.relative_path
        if not executable.is_file():
            raise ToolchainError(f"Built test executable not found: {executable}")
        enumeration_log = log_dir / f"{runner.target}-list.log"
        if runner.kind == "gtest":
            enumeration_command: list[str | Path] = [executable, "--gtest_list_tests"]
            enumeration_environment = runner.environment
        else:
            enumeration_environment = {**runner.environment, "QT_FORCE_STDERR_LOGGING": "1"}
            enumeration_command = [executable, "-input", qml_input, "-functions"]
        enumeration = _invoke(
            execute,
            enumeration_command,
            log_path=enumeration_log,
            repo_root=repo_root,
            process_environment=process_environment,
            toolchain=toolchain,
            runner_environment=enumeration_environment,
            show_all_output=args.show_all_output,
            phase="test discovery",
            emit=emit,
        )
        if enumeration.exit_code:
            failures = reduce_test_failure(enumeration.output, repo_root)
            if not any(failure.location for failure in failures):
                failures = [_silent_runner_failure(runner, enumeration.exit_code, [])]
            report(
                render_test_report(
                    failures,
                    scope=args.scope,
                    total=0,
                    elapsed=_summary_duration(started, clock),
                    repo_root=repo_root,
                    log_paths=[enumeration_log],
                )
            )
            return enumeration.exit_code
        discovered = (
            parse_gtest_list(enumeration.output)
            if runner.kind == "gtest"
            else parse_qml_functions(enumeration.output)
        )
        catalog.extend((runner, test) for test in discovered)

    selected = [
        entry
        for entry in catalog
        if not args.match or matches_pattern(entry[1], args.match)
    ]
    if not selected:
        if args.match:
            raise ToolchainError(f"No tests in scope '{args.scope}' matched '{args.match}'.")
        raise ToolchainError(f"No tests were discovered in scope '{args.scope}'.")
    if args.list:
        for _, test in selected:
            emit(test)
        emit(f"Total: {len(selected)}")
        return 0

    failures = []
    failed_logs: list[Path] = []
    notes: list[str] = []
    for runner in runners:
        runner_tests = [test for selected_runner, test in selected if selected_runner.target == runner.target]
        if not runner_tests:
            continue
        executable = build_dir / runner.relative_path
        if runner.kind == "gtest":
            test_log = log_dir / f"{runner.target}.log"
            report_path = log_dir / f"{runner.target}.xml"
            test_command: list[str | Path] = [
                executable,
                "--gtest_brief=1",
                f"--gtest_output=xml:{report_path}",
            ]
            if args.match:
                test_command.append(f"--gtest_filter={':'.join(runner_tests)}")
        else:
            report_path = log_dir / f"{runner.target}.log"
            test_log = log_dir / f"{runner.target}-console.log"
            test_command = [
                executable,
                "-input",
                qml_input,
                "-silent",
                "-o",
                f"{report_path},txt",
            ]
            if args.match:
                test_command.extend(runner_tests)
        result = _invoke(
            execute,
            test_command,
            log_path=test_log,
            repo_root=repo_root,
            process_environment=process_environment,
            toolchain=toolchain,
            runner_environment=runner.environment,
            show_all_output=args.show_all_output,
            phase="test",
            emit=emit,
        )
        if runner.kind == "qml" and report_path.is_file():
            report_output = report_path.read_text(encoding="utf-8", errors="replace")
            merged = "\n".join(part for part in (result.output, report_output) if part)
            report_path.write_text(merged + "\n", encoding="utf-8")
            test_log = report_path
        if not result.exit_code:
            continue
        failed_logs.append(test_log)
        if runner.kind == "gtest":
            reported = _read_report(lambda: parse_gtest_xml(report_path, repo_root))
        else:
            reported = _read_report(
                lambda: parse_qt_txt(report_path.read_text(encoding="utf-8", errors="replace"), repo_root)
            )
        if not reported:
            reported = reduce_test_failure(result.output, repo_root)
            if not any(failure.location for failure in reported):
                reported = [_silent_runner_failure(runner, result.exit_code, runner_tests)]
            notes.append(f"({runner.target} wrote no usable report; recovered from console output)")
        failures.extend(reported)

    elapsed = _summary_duration(started, clock)
    if failures:
        report(
            render_test_report(
                failures,
                scope=args.scope,
                total=len(selected),
                elapsed=elapsed,
                repo_root=repo_root,
                log_paths=failed_logs,
                notes=notes,
            )
        )
        return 1
    emit(f"Scope {args.scope} | jobs {BUILD_JOBS} | tests {len(selected)} | {elapsed:.1f}s | PASS")
    return 0


def _read_ctest_junit(junit_path: Path, repo_root: Path, qml_log_dir: Path) -> tuple[list[Failure], int]:
    try:
        cases, total = parse_ctest_junit(junit_path)
    except UNUSABLE_REPORT:
        return [], 0
    return failures_from_ctest(cases, repo_root, qml_log_dir), total
