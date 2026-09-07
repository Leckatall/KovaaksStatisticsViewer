from __future__ import annotations

from pathlib import Path
import re


def _normalize_line(line: str, repo_root: Path) -> str:
    normalized = line.replace("\\", "/")
    root = repo_root.resolve().as_posix().rstrip("/")
    normalized = re.sub(re.escape(root) + r"/", "", normalized, flags=re.IGNORECASE)
    normalized = re.sub(re.escape(root), ".", normalized, flags=re.IGNORECASE)
    return normalized.rstrip()


def _unique(lines: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for line in lines:
        if line not in seen:
            seen.add(line)
            result.append(line)
    return result


def _fallback(lines: list[str], repo_root: Path) -> str:
    useful = [
        _normalize_line(line, repo_root)
        for line in lines[-160:]
        if line and not line.startswith("> ") and not line.startswith("ENV ")
    ]
    return "\n".join(_unique(useful))


def reduce_configure_failure(output: str, repo_root: Path) -> str:
    lines = output.splitlines()
    reduced: list[str] = []
    capturing = False
    for line in lines:
        if re.match(r"^(CMake Error|CMake Warning at.*error)", line):
            capturing = True
        if capturing and line.startswith("-- Configuring incomplete"):
            capturing = False
        if capturing:
            reduced.append(_normalize_line(line, repo_root))
    return "\n".join(_unique(reduced)) if reduced else _fallback(lines, repo_root)


def _target_from_failed(line: str) -> str:
    remainder = re.sub(r"^FAILED:\s*", "", line)
    remainder = re.sub(r"^\[code=\d+\]\s*", "", remainder)
    return remainder.split()[0] if remainder else "unknown target"


def reduce_build_failure(output: str, repo_root: Path) -> str:
    lines = output.splitlines()
    reduced: list[str] = []
    index = 0
    while index < len(lines):
        line = lines[index]
        if not line.startswith("FAILED:"):
            index += 1
            continue
        reduced.append(f"Target: {_normalize_line(_target_from_failed(line), repo_root)}")
        index += 1
        while index < len(lines) and not lines[index].startswith("FAILED:"):
            candidate = lines[index]
            if re.match(r"^\[\d+/\d+\]", candidate) or candidate.startswith("ninja: build stopped"):
                index += 1
                continue
            linker = re.match(r"^.*[/\\](ld\.exe:.*)$", candidate)
            if linker:
                reduced.append(linker.group(1))
            elif re.search(r":\s*(?:fatal error|error|warning|note):", candidate) or candidate.startswith(
                "collect2.exe:"
            ):
                reduced.append(_normalize_line(candidate, repo_root))
            elif reduced and (re.match(r"^\s+\d+\s*\|", candidate) or re.match(r"^\s*\|", candidate)):
                reduced.append(candidate.rstrip())
            index += 1
    return "\n".join(_unique(reduced)) if reduced else _fallback(lines, repo_root)


def reduce_test_failure(output: str, repo_root: Path) -> str:
    lines = output.splitlines()
    reduced: list[str] = []
    capturing_gtest = False
    capturing_qml = False
    capturing_ctest = False
    for line in lines:
        normalized = _normalize_line(line, repo_root)
        if re.match(r"^\s*\d+/\d+\s+Test\s+#\d+:.*\*\*\*(?:Failed|Exception|Timeout|Not Run)", line):
            capturing_ctest = True
            reduced.append(normalized)
            continue
        if capturing_ctest:
            if (
                re.match(r"^\s*(?:Start\s+\d+:|\d+/\d+\s+Test\s+#\d+:)", line)
                or re.match(r"^\d+% tests passed", line)
                or line.startswith("The following tests FAILED:")
            ):
                capturing_ctest = False
            elif (
                normalized
                and not line.startswith("Running main() from ")
                and not line.startswith("[==========]")
                and not line.startswith("[  PASSED  ]")
            ):
                reduced.append(normalized)
            if capturing_ctest:
                continue
        if re.search(r":\d+: Failure$", normalized):
            capturing_gtest = True
        if line.startswith("FAIL!  :"):
            capturing_qml = True
        if capturing_gtest:
            if line.startswith("[==========]") or line.startswith("[  PASSED  ]"):
                capturing_gtest = False
            elif normalized:
                reduced.append(normalized)
                if line.startswith("[  FAILED  ]"):
                    capturing_gtest = False
        elif capturing_qml:
            if line.startswith("Totals:"):
                capturing_qml = False
            elif normalized:
                reduced.append(normalized)
    return "\n".join(_unique(reduced)) if reduced else _fallback(lines, repo_root)
