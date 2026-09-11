from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re


@dataclass(frozen=True)
class Failure:
    """One failing test. `location` is `path:line` when the framework reported one."""

    test: str
    message: str
    location: str = ""


@dataclass(frozen=True)
class Diagnostic:
    """One compiler/CMake diagnostic, with every build target that reproduced it."""

    message: str
    targets: tuple[str, ...] = ()


@dataclass
class Reason:
    key: str
    location: str
    message: str
    tests: list[str] = field(default_factory=list)


_GTEST_NAME = re.compile(r"^([A-Za-z_][\w/]*)\.(.+)$")
_QT_NAME = re.compile(r"^(.*)::([^:]+)$")
_DATA_TAG = re.compile(r"^(.*)\((.*)\)$")


def _split_name(name: str) -> tuple[str, str, str]:
    """Return (suite, separator, leaf); suite is empty when the name has no suite prefix."""
    qt = _QT_NAME.match(name)
    if qt:
        return qt.group(1), "::", qt.group(2)
    gtest = _GTEST_NAME.match(name)
    if gtest:
        return gtest.group(1), ".", gtest.group(2)
    return "", "", name


def _fold_data_tags(leaves: list[str]) -> list[str]:
    """`f(alpha)`, `f(beta)` -> `f(alpha|beta)`. Untagged leaves pass through unchanged."""
    order: list[str] = []
    tags: dict[str, list[str]] = {}
    for leaf in leaves:
        match = _DATA_TAG.match(leaf)
        base, tag = (match.group(1), match.group(2)) if match else (leaf, "")
        if base not in tags:
            tags[base] = []
            order.append(base)
        if tag and tag not in tags[base]:
            tags[base].append(tag)
    return [f"{base}({'|'.join(tags[base])})" if tags[base] else base for base in order]


def compress_names(names: list[str]) -> list[str]:
    """Factor the shared suite prefix and data tags out of a list of test names."""
    order: list[str] = []
    groups: dict[tuple[str, str], list[str]] = {}
    for name in names:
        suite, separator, leaf = _split_name(name)
        signature = (suite, separator)
        if signature not in groups:
            groups[signature] = []
            order.append(signature)
        groups[signature].append(leaf)
    rendered: list[str] = []
    for suite, separator in order:
        leaves = _fold_data_tags(groups[(suite, separator)])
        if not suite:
            rendered.extend(leaves)
        elif len(leaves) == 1:
            rendered.append(f"{suite}{separator}{leaves[0]}")
        else:
            rendered.append(f"{suite}{separator}{{{', '.join(leaves)}}}")
    return rendered


def group(failures: list[Failure]) -> list[Reason]:
    """Collapse failures sharing a location and message into one keyed reason, first-seen order."""
    order: list[tuple[str, str]] = []
    reasons: dict[tuple[str, str], Reason] = {}
    for failure in failures:
        signature = (failure.location, failure.message)
        if signature not in reasons:
            reasons[signature] = Reason("", failure.location, failure.message)
            order.append(signature)
        tests = reasons[signature].tests
        if failure.test and failure.test not in tests:
            tests.append(failure.test)
    grouped = [reasons[signature] for signature in order]
    for index, reason in enumerate(grouped, start=1):
        reason.key = f"R{index}"
    return grouped


def _indent(text: str, prefix: str = "  ") -> list[str]:
    return [prefix + line if line else "" for line in text.splitlines()]


def _header(kind: str, scope: str, detail: str, elapsed: float) -> str:
    return f"FAILED ({kind}) | scope {scope} | {detail} | {elapsed:.1f}s"


def _log_pointer(log_paths: list[Path]) -> list[str]:
    if not log_paths:
        return []
    joined = ", ".join(path.as_posix() for path in log_paths)
    return ["", f"log (read only if the above is insufficient): {joined}"]


def _root_note(repo_root: Path) -> str:
    return f"paths relative to {repo_root.resolve().as_posix()}"


def render_test_report(
    failures: list[Failure],
    *,
    scope: str,
    total: int,
    elapsed: float,
    repo_root: Path,
    log_paths: list[Path],
    notes: list[str] | None = None,
) -> list[str]:
    reasons = group(failures)
    # A test that tripped several assertions is still one failing test.
    named = {failure.test for failure in failures if failure.test}
    failed = len(named) + sum(1 for failure in failures if not failure.test)
    detail = f"{failed} of {total} failed" if total else f"{failed} failed"
    lines = ["", _header("test", scope, detail, elapsed), _root_note(repo_root)]
    lines.extend(notes or [])
    for reason in reasons:
        lines.append("")
        names = compress_names(reason.tests)
        count = f" x{len(reason.tests)}" if len(reason.tests) > 1 else ""
        head = " ".join(part for part in (f"{reason.key}{count}", reason.location) if part)
        if len(reason.tests) == 1:
            head = f"{head} {reason.tests[0]}".strip()
        lines.append(head)
        lines.extend(_indent(reason.message))
        if len(reason.tests) > 1:
            lines.extend(f"  -> {name}" for name in names)
    lines.extend(_log_pointer(log_paths))
    return lines


def render_diagnostic_report(
    diagnostics: list[Diagnostic],
    *,
    kind: str,
    scope: str,
    elapsed: float,
    repo_root: Path,
    log_paths: list[Path],
) -> list[str]:
    prefix = kind[0].upper()
    targets: list[str] = []
    for diagnostic in diagnostics:
        targets.extend(target for target in diagnostic.targets if target not in targets)
    keys = {target: f"T{index}" for index, target in enumerate(targets, start=1)}

    noun = "diagnostic" if len(diagnostics) == 1 else "diagnostics"
    detail = f"{len(diagnostics)} {noun}"
    if len(targets) > 1:
        detail += f" in {len(targets)} targets"
    lines = ["", _header(kind, scope, detail, elapsed), _root_note(repo_root)]
    if len(targets) == 1:
        lines.append(f"target {targets[0]}")
    else:
        # The same object path repeats under every diagnostic it produced; name it once.
        lines.extend(f"{keys[target]} = {target}" for target in targets)

    # A broken declaration makes the compiler report every later use of it in the same
    # translation unit. Those are consequences of the first diagnostic, not separate causes,
    # so only the first one per target is worth a source excerpt; the rest keep their location.
    blocks: list[str] = []
    excerpted: set[tuple[str, ...]] = set()
    densified = False
    for index, diagnostic in enumerate(diagnostics, start=1):
        head = f"{prefix}{index}"
        if len(targets) > 1:
            head = f"{head} {','.join(keys[target] for target in diagnostic.targets)}"
        headline = diagnostic.message.splitlines()[0]
        if diagnostic.targets in excerpted:
            blocks.append(f"{head} {headline}")
            densified = True
            continue
        excerpted.add(diagnostic.targets)
        blocks.extend(["", head, *_indent(diagnostic.message)])
    if densified:
        lines.append("(source excerpt shown once per target; later errors there are listed by location)")
    lines.extend(blocks)
    lines.extend(_log_pointer(log_paths))
    return lines
