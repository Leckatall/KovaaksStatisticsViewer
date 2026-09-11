from __future__ import annotations

from dataclasses import dataclass
import fnmatch
import re
from typing import Mapping


QML_ENVIRONMENT = {
    "QT_QPA_PLATFORM": "offscreen",
    "QT_QUICK_BACKEND": "software",
    "QT_QUICK_CONTROLS_STYLE": "Fusion",
}


@dataclass(frozen=True)
class RunnerSpec:
    target: str
    kind: str
    relative_path: str
    environment: Mapping[str, str]


def _gtest(target: str) -> RunnerSpec:
    return RunnerSpec(target, "gtest", f"{target}.exe", {})


RUNNERS = {
    "domain": [_gtest("domain_tests")],
    "data": [
        _gtest(target)
        for target in (
            "proto_decoder_tests",
            "stats_csv_parser_tests",
            "run_filename_tests",
            "run_ingestor_tests",
            "benchmarks_service_tests",
            "profile_service_tests",
            "profile_builder_tests",
            "profile_serializer_tests",
            "profile_v3_migrator_tests",
        )
    ],
    "qt-data": [_gtest("qt_data_tests")],
    "app": [_gtest("app_tests")],
    "ui-cpp": [_gtest("ui_tests")],
    "qml": [RunnerSpec("ui_qml_tests", "qml", "ui_qml_tests.exe", QML_ENVIRONMENT)],
    "integration": [RunnerSpec("integration_tests", "gtest", "integration_tests.exe", QML_ENVIRONMENT)],
}


def catalog_for_scope(scope: str) -> list[RunnerSpec]:
    if scope == "all":
        return []
    if scope == "ui":
        return [*RUNNERS["ui-cpp"], *RUNNERS["qml"]]
    return list(RUNNERS[scope])


def parse_gtest_list(output: str) -> list[str]:
    tests: list[str] = []
    suite: str | None = None
    for raw_line in output.splitlines():
        uncommented = raw_line.split("#", 1)[0].rstrip()
        if uncommented and not uncommented[0].isspace() and uncommented.endswith("."):
            suite = uncommented[:-1].strip()
        elif suite and uncommented[:1].isspace():
            case = uncommented.strip()
            if case:
                tests.append(f"{suite}.{case}")
    return tests


def parse_qml_functions(output: str) -> list[str]:
    tests: list[str] = []
    pattern = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)::((?:test|benchmark)_[A-Za-z0-9_]+)\(\)$")
    for line in output.splitlines():
        match = pattern.match(line.strip())
        if match:
            tests.append(f"{match.group(1)}::{match.group(2)}")
    return sorted(set(tests))


def matches_pattern(name: str, pattern: str) -> bool:
    return fnmatch.fnmatchcase(name.casefold(), pattern.casefold())
