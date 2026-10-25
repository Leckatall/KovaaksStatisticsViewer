from __future__ import annotations

from dataclasses import dataclass, field, replace
import json
from pathlib import Path

from .failures import _header, _indent, _log_pointer, _root_note
from .normalize import normalize, normalize_line


QMLLINT_TARGET = "all_qmllint_json"

# Every `<module>_qmllint_json` target writes `<build dir>/<module>_qmllint.json`.
REPORT_GLOB = "*_qmllint.json"

# qmllint reports these but does not mean them as defects; they never fail the scope.
NON_FAILING_TYPES = frozenset({"info", "debug"})

# A malformed report surfaces as a decode error or as an entry missing the keys read below.
UNUSABLE_LINT_REPORT = (OSError, ValueError, KeyError, TypeError)


@dataclass(frozen=True)
class Note:
    path: str
    line: int
    column: int
    message: str


@dataclass(frozen=True)
class Finding:
    """One qmllint diagnostic. `kind` is qmllint's `type`, `category` its `id`."""

    path: str
    line: int
    column: int
    kind: str
    category: str
    message: str
    fix: str = ""
    notes: tuple[Note, ...] = ()

    @property
    def fails(self) -> bool:
        return self.kind not in NON_FAILING_TYPES


def _fix(warning: dict) -> str:
    """The suggested edits, which carry the per-occurrence detail (e.g. which id to qualify with)."""
    parts: list[str] = []
    for suggestion in warning.get("suggestions") or []:
        edits = suggestion.get("documentEdits") or []
        for edit in edits:
            replacement = edit.get("replacement", "")
            verb = "insert" if not edit.get("location", {}).get("length") else "replace with"
            parts.append(f"{verb} {json.dumps(replacement)}")
        if not edits and suggestion.get("message"):
            parts.append(" ".join(suggestion["message"].split()))
    return "; ".join(parts)


def parse_qmllint_json(path: Path, repo_root: Path) -> list[Finding]:
    """Read the report written by `qmllint --json`.

    qmllint emits a finding's supporting notes ("Note: Id defined here") as separate entries
    directly after it, so each one is folded into the finding it follows.
    """
    document = json.loads(path.read_text(encoding="utf-8"))
    findings: list[Finding] = []
    for entry in document["files"]:
        file = normalize_line(entry["filename"], repo_root)
        for warning in entry.get("warnings") or []:
            message = normalize(warning["message"], repo_root).strip()
            line = int(warning.get("line") or 0)
            column = int(warning.get("column") or 0)
            if message.startswith("Note:") and findings:
                note = Note(file, line, column, message[len("Note:"):].strip())
                findings[-1] = replace(findings[-1], notes=(*findings[-1].notes, note))
                continue
            findings.append(
                Finding(
                    path=file,
                    line=line,
                    column=column,
                    kind=warning["type"],
                    category=warning.get("id", ""),
                    message=message,
                    fix=_fix(warning),
                )
            )
    return findings


@dataclass
class _Reason:
    kind: str
    category: str
    message: str
    findings: list[Finding] = field(default_factory=list)


def render_qmllint_report(
    findings: list[Finding],
    *,
    scope: str,
    elapsed: float,
    repo_root: Path,
    log_paths: list[Path],
) -> list[str]:
    failing = [finding for finding in findings if finding.fails]
    files: list[str] = []
    for finding in failing:
        for path in (finding.path, *(note.path for note in finding.notes)):
            if path not in files:
                files.append(path)
    keys = {path: f"F{index}" for index, path in enumerate(files, start=1)}

    def where(path: str, line: int, column: int) -> str:
        position = f"{line}:{column}" if line else ""
        if len(files) == 1:
            return position or path
        return ":".join(part for part in (keys[path], position) if part)

    order: list[tuple[str, str, str]] = []
    reasons: dict[tuple[str, str, str], _Reason] = {}
    for finding in failing:
        signature = (finding.kind, finding.category, finding.message)
        if signature not in reasons:
            reasons[signature] = _Reason(*signature)
            order.append(signature)
        reasons[signature].findings.append(finding)

    noun = "finding" if len(failing) == 1 else "findings"
    detail = f"{len(failing)} {noun}"
    if len(files) > 1:
        detail += f" in {len(files)} files"
    lines = ["", _header("qmllint", scope, detail, elapsed), _root_note(repo_root)]
    passing = len(findings) - len(failing)
    if passing:
        plural = "finding" if passing == 1 else "findings"
        lines.append(f"({passing} info-level {plural} not shown; they do not fail the scope)")
    if len(files) == 1:
        lines.append(f"file {files[0]}")
    else:
        lines.extend(f"{keys[path]} = {path}" for path in files)

    for index, signature in enumerate(order, start=1):
        reason = reasons[signature]
        count = f" x{len(reason.findings)}" if len(reason.findings) > 1 else ""
        category = f" [{reason.category}]" if reason.category else ""
        headline, *rest = reason.message.splitlines() or [""]
        lines.extend(["", f"R{index}{count} {reason.kind}{category} {headline}".rstrip()])
        lines.extend(_indent("\n".join(rest)))
        for finding in reason.findings:
            suffixes = [finding.fix] if finding.fix else []
            suffixes.extend(
                f"note {where(note.path, note.line, note.column)} {note.message}" for note in finding.notes
            )
            location = where(finding.path, finding.line, finding.column)
            lines.append("  " + "  ".join([location, *suffixes]))
    lines.extend(_log_pointer(log_paths))
    return lines
