from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
from xml.etree import ElementTree

from .failures import Failure
from .normalize import normalize


# Frame that every Google Test run prints around the part that says what went wrong.
_GTEST_BANNER = re.compile(
    r"^(\[(=|-)+\]|\[\s*(RUN|OK|FAILED|PASSED|SKIPPED)\s*\]|Note: Google Test filter"
    r"|Running main\(\) from|\s*\d+ FAILED TESTS?$|YOU HAVE )"
)
_GTEST_ASSERTION = re.compile(r"^(.+:\d+): (?:Failure|Skipped)$")
_GTEST_RUN = re.compile(r"^\[\s*RUN\s*\] (.+)$")
_GTEST_FAILED = re.compile(r"^\[\s*FAILED\s*\] (\S+) \(\d+ ms\)$")
_QT_INCIDENT = re.compile(r"^(FAIL!|XPASS)\s*:\s+(\S+)\s*(.*)$")
_QT_LOCATION = re.compile(r"^(.*)\((\d+)\) : failure location$")


def parse_gtest_xml(path: Path, repo_root: Path) -> list[Failure]:
    """Read the report written by `--gtest_output=xml:`. One Failure per `<failure>` element."""
    document = ElementTree.parse(path).getroot()
    failures: list[Failure] = []
    for case in document.iter("testcase"):
        name = f"{case.get('classname', '')}.{case.get('name', '')}"
        for element in case.findall("failure"):
            body = normalize(element.text or element.get("message") or "", repo_root)
            lines = body.splitlines()
            if not lines:
                continue
            # Google Test puts `path:line` on the first line of the failure body.
            located = bool(re.match(r"^.+:\d+$", lines[0]))
            location = lines[0] if located else ""
            message = "\n".join(lines[1:] if located else lines).strip()
            failures.append(Failure(test=name, message=message, location=location))
    return failures


def parse_gtest_text(output: str, repo_root: Path) -> list[Failure]:
    """Recover failures from a Google Test console run, dropping its banner.

    Under `--gtest_brief=1` a test's assertions are printed before the `[  FAILED  ] <name>`
    line that names it, so assertions are held pending until a name arrives.
    """
    failures: list[Failure] = []
    pending: list[tuple[str, str]] = []
    name = ""
    location = ""
    body: list[str] = []

    def seal() -> None:
        nonlocal location, body
        if location:
            pending.append((location, "\n".join(body).strip()))
        location, body = "", []

    def flush(test: str) -> None:
        nonlocal pending
        seal()
        failures.extend(Failure(test=test, message=message, location=where) for where, message in pending)
        pending = []

    for raw_line in output.splitlines():
        started = _GTEST_RUN.match(raw_line)
        if started:
            flush(name)
            name = started.group(1)
            continue
        finished = _GTEST_FAILED.match(raw_line)
        if finished:
            flush(finished.group(1))
            name = ""
            continue
        if _GTEST_BANNER.match(raw_line):
            seal()
            continue
        line = normalize(raw_line, repo_root)
        assertion = _GTEST_ASSERTION.match(line)
        if assertion:
            seal()
            location = assertion.group(1)
        elif location:
            body.append(line)
    flush(name)
    return failures


def parse_qt_txt(output: str, repo_root: Path) -> list[Failure]:
    """Parse the Qt Test plain-text report.

    Qt's junitxml logger is deliberately not used here: it carries no failure location and
    splits a multi-line message between the `message` attribute and the CDATA body. The text
    report carries all three parts, and an incident is unambiguously terminated by its
    `... : failure location` line or by the next incident.
    """
    failures: list[Failure] = []
    name = ""
    location = ""
    body: list[str] = []

    def flush() -> None:
        nonlocal name, location, body
        if name:
            failures.append(Failure(test=name, message="\n".join(body).strip(), location=location))
        name, location, body = "", "", []

    for raw_line in output.splitlines():
        incident = _QT_INCIDENT.match(raw_line)
        if incident:
            flush()
            # Qt qualifies every name with the runner executable; the suite is what identifies it.
            qualified = incident.group(2)
            name = qualified.split("::", 1)[1] if "::" in qualified else qualified
            if name.endswith("()"):
                name = name[:-2]
            body = [normalize(incident.group(3), repo_root)] if incident.group(3) else []
            continue
        placed = _QT_LOCATION.match(raw_line)
        if placed and name:
            location = normalize(f"{placed.group(1)}:{placed.group(2)}", repo_root)
            flush()
            continue
        if raw_line.startswith("Totals:"):
            flush()
            continue
        if name:
            body.append(normalize(raw_line, repo_root))
    flush()
    return failures


@dataclass(frozen=True)
class CtestCase:
    name: str
    reason: str
    output: str


def parse_ctest_junit(path: Path) -> tuple[list[CtestCase], int]:
    """Read `ctest --output-junit`. Returns the non-passing cases and the total test count."""
    document = ElementTree.parse(path).getroot()
    total = int(document.get("tests") or 0)
    cases: list[CtestCase] = []
    for case in document.iter("testcase"):
        if case.get("status") == "run":
            continue
        element = case.find("failure")
        if element is None:
            element = case.find("skipped")
        reason = (element.get("message") if element is not None else None) or case.get("status", "failed")
        system_out = case.find("system-out")
        captured = (system_out.text if system_out is not None else "") or ""
        cases.append(CtestCase(case.get("name", ""), reason, captured))
    return cases, total


def failures_from_ctest(cases: list[CtestCase], repo_root: Path, qml_log_dir: Path) -> list[Failure]:
    """Turn non-passing CTest cases into Failures.

    `gtest_discover_tests` registers one CTest case per Google Test case, so the assertion is
    in the captured output. The Qt Quick tests are registered one per `tst_*.qml` file and
    write their report into `qml_log_dir` instead, leaving the captured output empty.
    """
    failures: list[Failure] = []
    for case in cases:
        qml_report = qml_log_dir / f"{case.name}.txt"
        if case.name.startswith("tst_") and qml_report.is_file():
            found = parse_qt_txt(qml_report.read_text(encoding="utf-8", errors="replace"), repo_root)
            if found:
                failures.extend(found)
                continue
        found = parse_gtest_text(case.output, repo_root)
        if found:
            failures.extend(
                failure if failure.test else Failure(case.name, failure.message, failure.location)
                for failure in found
            )
            continue
        remainder = [
            normalize(line, repo_root)
            for line in case.output.splitlines()
            if line.strip() and not _GTEST_BANNER.match(line)
        ]
        failures.append(Failure(case.name, "\n".join([case.reason, *remainder])))
    return failures
