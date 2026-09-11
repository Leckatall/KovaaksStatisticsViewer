from __future__ import annotations

from pathlib import Path
import re

from .failures import Diagnostic, Failure
from .normalize import normalize_line
from .parsers import parse_gtest_text, parse_qt_txt


# How much raw tail an unrecognized failure is allowed to contribute. Reaching this means
# no parser understood the output, so the tail is the only evidence the report can offer.
UNPARSED_TAIL_LINES = 40


def _unique(lines: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for line in lines:
        if line not in seen:
            seen.add(line)
            result.append(line)
    return result


def unparsed_tail(output: str, repo_root: Path) -> str:
    lines = [
        normalize_line(line, repo_root)
        for line in output.splitlines()
        if line and not line.startswith("> ") and not line.startswith("ENV ")
    ]
    kept = _unique(lines)[-UNPARSED_TAIL_LINES:]
    return "\n".join(["(no diagnostic recognized; raw tail follows)", *kept])


def reduce_configure_failure(output: str, repo_root: Path) -> list[Diagnostic]:
    lines = output.splitlines()
    blocks: list[list[str]] = []
    current: list[str] | None = None
    for line in lines:
        if re.match(r"^(CMake Error|CMake Warning at.*error)", line):
            current = []
            blocks.append(current)
        if current is not None and line.startswith("-- Configuring incomplete"):
            current = None
        if current is not None:
            current.append(normalize_line(line, repo_root))
    messages = _unique(["\n".join(_unique(block)).strip() for block in blocks if any(block)])
    if not messages:
        return [Diagnostic(unparsed_tail(output, repo_root))]
    return [Diagnostic(message) for message in messages]


def _target_from_failed(line: str) -> str:
    remainder = re.sub(r"^FAILED:\s*", "", line)
    remainder = re.sub(r"^\[code=\d+\]\s*", "", remainder)
    return remainder.split()[0] if remainder else "unknown target"


def reduce_build_failure(output: str, repo_root: Path) -> list[Diagnostic]:
    """Group compiler/linker diagnostics by text, so one bad header is reported once.

    Ninja repeats the same header error under every object that included it; the useful
    fact is the diagnostic plus which targets it broke, not N copies of the diagnostic.
    """
    lines = output.splitlines()
    order: list[str] = []
    targets: dict[str, list[str]] = {}
    index = 0
    while index < len(lines):
        if not lines[index].startswith("FAILED:"):
            index += 1
            continue
        target = normalize_line(_target_from_failed(lines[index]), repo_root)
        index += 1
        collected: list[str] = []
        while index < len(lines) and not lines[index].startswith("FAILED:"):
            candidate = lines[index]
            index += 1
            if re.match(r"^\[\d+/\d+\]", candidate) or candidate.startswith("ninja: build stopped"):
                continue
            linker = re.match(r"^.*[/\\](ld\.exe:.*)$", candidate)
            if linker:
                collected.append(linker.group(1))
            elif re.search(r":\s*(?:fatal error|error|warning|note):", candidate) or candidate.startswith(
                "collect2.exe:"
            ):
                collected.append(normalize_line(candidate, repo_root))
            elif collected and (re.match(r"^\s+\d+\s*\|", candidate) or re.match(r"^\s*\|", candidate)):
                collected.append(candidate.rstrip())
        for message in _split_diagnostics(collected):
            if message not in targets:
                targets[message] = []
                order.append(message)
            if target not in targets[message]:
                targets[message].append(target)
    if not order:
        return [Diagnostic(unparsed_tail(output, repo_root))]
    return [Diagnostic(message, tuple(targets[message])) for message in order]


def _split_diagnostics(lines: list[str]) -> list[str]:
    """One diagnostic is a `... error:`/`... warning:` line plus the source excerpt under it."""
    blocks: list[list[str]] = []
    for line in lines:
        starts_block = bool(re.search(r":\s*(?:fatal error|error|warning|note):", line)) or line.startswith(
            ("ld.exe:", "collect2.exe:")
        )
        if starts_block or not blocks:
            blocks.append([line])
        else:
            blocks[-1].append(line)
    return _unique(["\n".join(block).rstrip() for block in blocks if any(block)])


def reduce_test_failure(output: str, repo_root: Path) -> list[Failure]:
    """Text fallback for a runner that produced no machine-readable report."""
    return (
        parse_gtest_text(output, repo_root)
        or parse_qt_txt(output, repo_root)
        or [Failure(test="", message=unparsed_tail(output, repo_root))]
    )
