from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import date as calendar_date
from enum import IntEnum
import os
from pathlib import Path
import re
import sys
from types import MappingProxyType
from typing import Literal, Mapping, Sequence

from markdown_it import MarkdownIt
import yaml


class ExitCode(IntEnum):
    OK = 0
    USAGE = 2
    VALIDATION = 3
    UNRESOLVED = 4
    REFUSED = 5


class ArchDocError(Exception):
    def __init__(self, message: str, code: ExitCode, line: int | None = None) -> None:
        super().__init__(message)
        self.code = code
        self.line = line


@dataclass(frozen=True)
class ReferenceUse:
    label: str
    line: int


@dataclass(frozen=True)
class Registry:
    definitions: Mapping[str, str]
    start_offset: int
    end_offset: int


@dataclass(frozen=True)
class ArchitectureDocument:
    path: Path
    text: str
    kind: Literal["root", "view", "draft"]
    view_id: str | None
    name: str | None
    registry: Registry | None
    uses: tuple[ReferenceUse, ...]


@dataclass(frozen=True)
class AdrRecord:
    identifier: str
    path: Path
    status: str
    date: str
    retrospective: bool
    supersedes: tuple[str, ...]
    superseded_by: str | None
    body: bytes
    newline: bytes


@dataclass(frozen=True)
class StructuralProblem:
    path: Path
    line: int
    message: str


@dataclass(frozen=True)
class RepositorySnapshot:
    repo: Path
    root: ArchitectureDocument | None
    views_by_id: Mapping[str, ArchitectureDocument]
    views_by_path: Mapping[Path, ArchitectureDocument]
    drafts: tuple[Path, ...]
    facts: Mapping[str, str]
    adrs: Mapping[str, AdrRecord]
    problems: tuple[StructuralProblem, ...] = ()


_VIEW_ID = re.compile(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*\Z")
_REFERENCE_DEFINITION = re.compile(
    r"\[([a-z][a-z0-9]*(?:-[a-z0-9]+)*)\]:[ \t]+(\S(?:.*\S)?)\Z"
)
_REGISTRY_DELIMITER = re.compile(r"(?m)^<!-- arch-doc:references:(start|end) -->\r?$")
_FACT_ANCHOR = re.compile(r'^<a id="(fact-[a-z][a-z0-9]*(?:-[a-z0-9]+)*)"></a>\r?$')
_ADR_FILENAME = re.compile(r"^(\d{4})-[a-z][a-z0-9]*(?:-[a-z0-9]+)*\.md$")
_ADR_NUMERIC_PREFIX = re.compile(r"^\d{4}")
_ADR_NUMBERED_FILE = re.compile(r"^(\d{4})-.*\.md$")
_ADR_ID = re.compile(r"\d{4}\Z")
_ADR_SECTIONS = ("Context", "Decision", "Consequences", "Alternatives considered")


def _validation_error(message: str, line: int | None = None) -> ArchDocError:
    return ArchDocError(message, ExitCode.VALIDATION, line)


def _line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _frontmatter_value_line(raw: str, key: str) -> int:
    for line_number, line in enumerate(raw.splitlines(), start=2):
        if line.startswith(f"{key}:"):
            return line_number
    return 1


def _frontmatter(text: str) -> tuple[str, str] | None:
    lines = text.splitlines(keepends=True)
    if not lines or lines[0].rstrip("\r\n") != "---":
        return None

    for index, line in enumerate(lines[1:], start=1):
        if line.rstrip("\r\n") == "---":
            return "".join(lines[1:index]), "".join(lines[index + 1 :])
    raise _validation_error("Invalid view frontmatter: missing closing delimiter.", 1)


def _frontmatter_end_line(text: str) -> int | None:
    lines = text.splitlines(keepends=True)
    if not lines or lines[0].rstrip("\r\n") != "---":
        return None
    for index, line in enumerate(lines[1:], start=1):
        if line.rstrip("\r\n") == "---":
            return index
    return None


def _parse_view_frontmatter(text: str) -> tuple[str, str, str] | None:
    extracted = _frontmatter(text)
    if extracted is None:
        return None
    raw, body = extracted

    raw_keys = re.findall(r"(?m)^([A-Za-z][A-Za-z0-9_-]*):", raw)
    if raw_keys != ["id", "name"]:
        expected_keys = ("id", "name")
        for index, line in enumerate(raw.splitlines(), start=2):
            key = re.match(r"([A-Za-z][A-Za-z0-9_-]*):", line)
            if key is None or index - 2 >= len(expected_keys) or key.group(1) != expected_keys[index - 2]:
                raise _validation_error("Invalid view frontmatter key order: expected id then name.", index)
        raise _validation_error("Invalid view frontmatter key order: expected id then name.", 1)
    try:
        values = yaml.safe_load(raw)
    except yaml.YAMLError as error:
        mark = getattr(error, "problem_mark", None)
        raise _validation_error(f"Invalid view frontmatter: {error}", getattr(mark, "line", -1) + 2) from error
    if not isinstance(values, dict) or list(values) != ["id", "name"]:
        raise _validation_error("Invalid view frontmatter key order: expected id then name.", 1)

    view_id = values["id"]
    name = values["name"]
    if not isinstance(view_id, str) or not view_id:
        raise _validation_error("Invalid view id.", _frontmatter_value_line(raw, "id"))
    if not isinstance(name, str) or not name.strip():
        raise _validation_error("Invalid view name.", _frontmatter_value_line(raw, "name"))
    if not _VIEW_ID.fullmatch(view_id) or view_id.startswith("fact-"):
        raise _validation_error("Invalid view ID.", _frontmatter_value_line(raw, "id"))
    return view_id, name, body


def _first_level_one_heading(text: str) -> str | None:
    tokens = MarkdownIt("commonmark").parse(text)
    for index, token in enumerate(tokens):
        if token.type == "heading_open" and token.tag == "h1":
            return tokens[index + 1].content
    return None


def _first_level_one_heading_line(text: str) -> int | None:
    tokens = MarkdownIt("commonmark").parse(text)
    for token in tokens:
        if token.type == "heading_open" and token.tag == "h1" and token.map is not None:
            return token.map[0] + 1
    return None


def _is_escaped(source: str, index: int) -> bool:
    backslashes = 0
    for previous in range(index - 1, -1, -1):
        if source[previous] != "\\":
            break
        backslashes += 1
    return backslashes % 2 == 1


def _reference_uses_in_inline(source: str, start_line: int) -> list[ReferenceUse]:
    uses: list[ReferenceUse] = []
    index = 0
    while index < len(source):
        if source.startswith("<!--", index):
            end = source.find("-->", index + 4)
            index = len(source) if end == -1 else end + 3
            continue
        if source[index] == "`":
            delimiter_end = index
            while delimiter_end < len(source) and source[delimiter_end] == "`":
                delimiter_end += 1
            delimiter = source[index:delimiter_end]
            closing = source.find(delimiter, delimiter_end)
            index = len(source) if closing == -1 else closing + len(delimiter)
            continue
        if source[index] != "[" or _is_escaped(source, index):
            index += 1
            continue
        if index > 0 and source[index - 1] == "!":
            index += 1
            continue

        visible_end = source.find("]", index + 1)
        if visible_end == -1 or visible_end == index + 1 or visible_end + 1 >= len(source):
            index += 1
            continue
        if source[visible_end + 1] != "[":
            index += 1
            continue
        label_end = source.find("]", visible_end + 2)
        if label_end == -1:
            index += 1
            continue
        label = source[visible_end + 2 : label_end]
        if _VIEW_ID.fullmatch(label):
            uses.append(ReferenceUse(label, start_line + source.count("\n", 0, index)))
        index = label_end + 1
    return uses


def managed_reference_uses(text: str) -> tuple[ReferenceUse, ...]:
    frontmatter_end_line = _frontmatter_end_line(text)
    uses: list[ReferenceUse] = []
    for token in MarkdownIt("commonmark").parse(text):
        if token.type != "inline" or token.map is None:
            continue
        start_line, end_line = token.map
        if frontmatter_end_line is not None and start_line <= frontmatter_end_line < end_line:
            continue
        uses.extend(_reference_uses_in_inline(token.content, start_line + 1))
    return tuple(uses)


def _registry_delimiters(text: str) -> list[re.Match[str]]:
    fenced_lines = [
        range(token.map[0], token.map[1])
        for token in MarkdownIt("commonmark").parse(text)
        if token.type in {"fence", "code_block"} and token.map is not None
    ]
    return [
        delimiter
        for delimiter in _REGISTRY_DELIMITER.finditer(text)
        if not any(text.count("\n", 0, delimiter.start()) in fenced for fenced in fenced_lines)
    ]


def parse_registry(text: str) -> Registry | None:
    delimiters = _registry_delimiters(text)
    if not delimiters:
        return None
    if len(delimiters) != 2 or [delimiter.group(1) for delimiter in delimiters] != ["start", "end"]:
        raise _validation_error("Invalid registry delimiters.", _line_number(text, delimiters[-1].start()))

    start, end = delimiters
    if text[end.end() :].strip():
        trailing = text[end.end() :]
        first_content = next(index for index, character in enumerate(trailing) if not character.isspace())
        raise _validation_error("Registry must be the final nonblank block.", _line_number(text, end.end() + first_content))

    definitions: dict[str, str] = {}
    previous_label: str | None = None
    registry_contents = text[start.end() : end.start()]
    offset = start.end()
    for raw_line in registry_contents.splitlines(keepends=True):
        line_number = _line_number(text, offset)
        line = raw_line.rstrip("\r\n")
        offset += len(raw_line)
        if not line:
            continue
        definition = _REFERENCE_DEFINITION.fullmatch(line)
        if definition is None:
            raise _validation_error("Invalid registry definition.", line_number)
        label, destination = definition.groups()
        if label in definitions:
            raise _validation_error(f"Duplicate registry definition: {label}.", line_number)
        if previous_label is not None and label < previous_label:
            raise _validation_error("Registry definitions must be lexically ordered.", line_number)
        definitions[label] = destination
        previous_label = label

    return Registry(
        definitions=MappingProxyType(definitions),
        start_offset=len(text[: start.start()].encode("utf-8")),
        end_offset=len(text[: end.start() + len("<!-- arch-doc:references:end -->")].encode("utf-8")),
    )


def parse_document(path: Path, repo: Path) -> ArchitectureDocument:
    resolved_path = path.resolve()
    resolved_repo = repo.resolve()
    try:
        relative_path = resolved_path.relative_to(resolved_repo)
    except ValueError as error:
        raise _validation_error(f"Architecture document is outside the repository: {path}") from error
    text = resolved_path.read_bytes().decode("utf-8")
    registry = parse_registry(text)
    uses = managed_reference_uses(text)

    if relative_path == Path("docs/architecture/README.md"):
        return ArchitectureDocument(resolved_path, text, "root", None, None, registry, uses)

    views_directory = Path("docs/architecture/views")
    if views_directory not in (relative_path, *relative_path.parents):
        raise _validation_error(f"Not an architecture document: {path}")
    frontmatter = _parse_view_frontmatter(text)
    if frontmatter is None:
        return ArchitectureDocument(resolved_path, text, "draft", None, None, registry, uses)

    view_id, name, body = frontmatter
    if _first_level_one_heading(body) != name:
        body_heading_line = _first_level_one_heading_line(body)
        closing_delimiter_line = (_frontmatter_end_line(text) or 0) + 1
        raise _validation_error(
            "View name and first level-one heading must match.",
            closing_delimiter_line + (body_heading_line or 1),
        )
    return ArchitectureDocument(
        resolved_path,
        text,
        "view",
        view_id,
        name,
        registry,
        tuple(use for use in uses if use.label != view_id),
    )


def _relative_path(path: Path, repo: Path) -> str:
    return path.resolve().relative_to(repo.resolve()).as_posix()


def _problem(path: Path, line: int, message: str) -> StructuralProblem:
    return StructuralProblem(path.resolve(), line, message)


def _parse_root_facts(document: ArchitectureDocument) -> tuple[Mapping[str, str], tuple[StructuralProblem, ...]]:
    facts: dict[str, str] = {}
    fact_lines: dict[str, int] = {}
    problems: list[StructuralProblem] = []
    lines = document.text.splitlines()
    fenced_lines = {
        line
        for token in MarkdownIt("commonmark").parse(document.text)
        if token.type in {"fence", "code_block"} and token.map is not None
        for line in range(token.map[0], token.map[1])
    }
    for index, line in enumerate(lines):
        if index in fenced_lines:
            continue
        anchor = _FACT_ANCHOR.fullmatch(line)
        if anchor is None:
            continue
        fact_id = anchor.group(1)
        if fact_id in facts:
            problems.append(_problem(document.path, index + 1, f"Duplicate fact anchor: {fact_id}."))
            continue
        if index + 1 >= len(lines) or not re.fullmatch(r"#{1,6}[ \t]+.*", lines[index + 1]):
            problems.append(
                _problem(document.path, index + 1, f"Fact anchor '{fact_id}' must immediately precede a heading.")
            )
            continue
        facts[fact_id] = f"#{fact_id}"
        fact_lines[fact_id] = index + 1

    definitions = document.registry.definitions if document.registry is not None else {}
    for fact_id in facts:
        if definitions.get(fact_id) != f"#{fact_id}":
            problems.append(
                _problem(
                    document.path,
                    fact_lines[fact_id],
                    f"Fact anchor '{fact_id}' has no matching root registry definition.",
                )
            )
    for label, destination in definitions.items():
        if label.startswith("fact-") and (label not in facts or destination != f"#{label}"):
            problems.append(
                _problem(
                    document.path,
                    _definition_line(document, label),
                    f"Fact registry definition '{label}' has no matching anchor.",
                )
            )
    return MappingProxyType(facts), tuple(problems)


def _adr_relationship_ids(value: object, key: str, raw: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not value:
        raise _validation_error("Invalid ADR metadata.", _frontmatter_value_line(raw, key))
    identifiers: list[str] = []
    for identifier in value:
        if not isinstance(identifier, str) or not _ADR_ID.fullmatch(identifier):
            raise _validation_error("Invalid ADR relationship identifier.", _frontmatter_value_line(raw, key))
        if identifier in identifiers:
            raise _validation_error("Duplicate ADR relationship identifier.", _frontmatter_value_line(raw, key))
        identifiers.append(identifier)
    return tuple(identifiers)


def _adr_section_headings(text: str) -> tuple[str, ...]:
    tokens = MarkdownIt("commonmark").parse(text)
    return tuple(
        tokens[index + 1].content
        for index, token in enumerate(tokens)
        if token.type == "heading_open" and token.tag == "h2"
    )


def _valid_adr_sections(text: str) -> bool:
    headings = _adr_section_headings(text)
    return headings == _ADR_SECTIONS or headings == (*_ADR_SECTIONS, "Links")


def _parse_adr_payload(path: Path, payload: bytes) -> AdrRecord:
    filename = _ADR_FILENAME.fullmatch(path.name)
    if filename is None:
        raise _validation_error("Invalid ADR filename.", 1)
    text = payload.decode("utf-8")
    try:
        extracted = _frontmatter(text)
    except ArchDocError as error:
        raise _validation_error("Invalid ADR frontmatter.", error.line or 1) from error
    if extracted is None:
        raise _validation_error("Invalid ADR frontmatter.", 1)
    raw, body = extracted
    try:
        values = yaml.safe_load(raw)
    except yaml.YAMLError as error:
        mark = getattr(error, "problem_mark", None)
        raise _validation_error(f"Invalid ADR frontmatter: {error}", getattr(mark, "line", -1) + 2) from error
    if not isinstance(values, dict):
        raise _validation_error("Invalid ADR frontmatter.", 1)
    allowed_keys = {"status", "date", "retrospective", "supersedes", "superseded-by"}
    for key in values:
        if key not in allowed_keys:
            raise _validation_error(f"Invalid ADR metadata key: {key}.", _frontmatter_value_line(raw, str(key)))
    status = values.get("status")
    recorded_date = str(values.get("date", ""))
    if status not in {"accepted", "superseded"} or not re.fullmatch(r"\d{4}-\d{2}-\d{2}", recorded_date):
        invalid_key = "status" if status not in {"accepted", "superseded"} else "date"
        raise _validation_error("Invalid ADR metadata.", _frontmatter_value_line(raw, invalid_key))
    try:
        calendar_date.fromisoformat(recorded_date)
    except ValueError as error:
        raise _validation_error("Invalid ADR metadata.", _frontmatter_value_line(raw, "date")) from error

    retrospective = values.get("retrospective", False)
    if retrospective not in {False, True} or ("retrospective" in values and retrospective is not True):
        raise _validation_error("Invalid ADR metadata.", _frontmatter_value_line(raw, "retrospective"))
    supersedes = (
        _adr_relationship_ids(values["supersedes"], "supersedes", raw)
        if "supersedes" in values
        else ()
    )
    superseded_by = values.get("superseded-by")
    if superseded_by is not None and (not isinstance(superseded_by, str) or not _ADR_ID.fullmatch(superseded_by)):
        raise _validation_error("Invalid ADR relationship identifier.", _frontmatter_value_line(raw, "superseded-by"))
    if status == "accepted" and superseded_by is not None:
        raise _validation_error("Accepted ADR cannot declare superseded-by.", _frontmatter_value_line(raw, "superseded-by"))
    if status == "superseded" and superseded_by is None:
        raise _validation_error("Superseded ADR must declare superseded-by.", _frontmatter_value_line(raw, "status"))
    identifier = filename.group(1)
    heading = _first_level_one_heading(body)
    if heading is None or not heading.startswith(f"{identifier}: ") or not heading.removeprefix(f"{identifier}: ").strip():
        raise _validation_error("ADR heading must match its filename identifier.", (_frontmatter_end_line(text) or 0) + 2)
    if not _valid_adr_sections(body):
        raise _validation_error("Invalid ADR body section order.", (_frontmatter_end_line(text) or 0) + 2)
    newline = b"\r\n" if b"\r\n" in payload[: payload.find(b"---", 3) + 3] else b"\n"
    return AdrRecord(
        identifier,
        path.resolve(),
        status,
        recorded_date,
        bool(retrospective),
        supersedes,
        superseded_by,
        body.encode("utf-8"),
        newline,
    )


def _parse_adr(path: Path) -> AdrRecord:
    return _parse_adr_payload(path, path.read_bytes())


def _adr_relationship_problems(
    adrs: Mapping[str, AdrRecord], repo: Path
) -> tuple[StructuralProblem, ...]:
    problems: list[StructuralProblem] = []
    for identifier, record in adrs.items():
        for replaced_id in record.supersedes:
            replaced = adrs.get(replaced_id)
            if replaced is None:
                problems.append(_problem(record.path, 1, f"ADR '{identifier}' supersedes missing ADR '{replaced_id}'."))
            elif replaced.status != "superseded" or replaced.superseded_by != identifier:
                problems.append(_problem(record.path, 1, f"ADR '{identifier}' has a nonreciprocal supersedes relationship with '{replaced_id}'."))
        if record.superseded_by is None:
            continue
        replacement = adrs.get(record.superseded_by)
        if replacement is None:
            problems.append(
                _problem(record.path, 1, f"ADR '{identifier}' names missing replacement '{record.superseded_by}'.")
            )
        elif replacement.status != "accepted" or identifier not in replacement.supersedes:
            problems.append(
                _problem(
                    record.path,
                    1,
                    f"ADR '{identifier}' has a nonreciprocal superseded-by relationship with '{record.superseded_by}'.",
                )
            )
    return tuple(sorted(problems, key=lambda problem: (_relative_path(problem.path, repo), problem.line, problem.message)))


def load_snapshot(repo: Path) -> RepositorySnapshot:
    resolved_repo = repo.resolve()
    problems: list[StructuralProblem] = []
    root_path = root_document(resolved_repo)
    root: ArchitectureDocument | None = None
    if root_path.is_file():
        try:
            root = parse_document(root_path, resolved_repo)
        except ArchDocError as error:
            problems.append(_problem(root_path, error.line or 1, str(error)))

    views_by_id: dict[str, ArchitectureDocument] = {}
    views_by_path: dict[Path, ArchitectureDocument] = {}
    drafts: list[Path] = []
    views_directory = resolved_repo / "docs" / "architecture" / "views"
    if views_directory.is_dir():
        for path in sorted((candidate for candidate in views_directory.rglob("*.md") if candidate.is_file()), key=lambda item: item.as_posix()):
            try:
                document = parse_document(path, resolved_repo)
            except ArchDocError as error:
                problems.append(_problem(path, error.line or 1, str(error)))
                continue
            if document.kind == "draft":
                drafts.append(document.path)
                continue
            if document.view_id in views_by_id:
                views_by_path[document.path] = document
                problems.append(
                    _problem(
                        document.path,
                        _frontmatter_value_line(_frontmatter(document.text)[0], "id"),
                        f"Duplicate managed view ID '{document.view_id}'.",
                    )
                )
                continue
            views_by_id[document.view_id] = document
            views_by_path[document.path] = document

    facts: Mapping[str, str] = MappingProxyType({})
    if root is not None:
        facts, fact_problems = _parse_root_facts(root)
        problems.extend(fact_problems)

    adrs: dict[str, AdrRecord] = {}
    decisions_directory = resolved_repo / "docs" / "architecture" / "decisions"
    if decisions_directory.is_dir():
        for path in sorted((candidate for candidate in decisions_directory.rglob("*.md") if candidate.is_file()), key=lambda item: item.as_posix()):
            if not _ADR_NUMERIC_PREFIX.match(path.name):
                continue
            try:
                record = _parse_adr(path)
            except ArchDocError as error:
                problems.append(_problem(path, error.line or 1, str(error)))
                continue
            if record.identifier in adrs:
                problems.append(_problem(path, 1, f"Duplicate ADR identifier '{record.identifier}' from filename."))
                continue
            adrs[record.identifier] = record
    problems.extend(_adr_relationship_problems(adrs, resolved_repo))

    return RepositorySnapshot(
        resolved_repo,
        root,
        MappingProxyType(views_by_id),
        MappingProxyType(views_by_path),
        tuple(drafts),
        facts,
        MappingProxyType(adrs),
        tuple(problems),
    )


class ArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise ArchDocError(f"error: {message}", ExitCode.USAGE)


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def root_document(root: Path) -> Path:
    return root / "docs" / "architecture" / "README.md"


def require_root(root: Path) -> Path:
    document = root_document(root)
    if not document.is_file():
        raise ArchDocError(f"Architecture document not found: {document}", ExitCode.UNRESOLVED)
    return document


def require_file(path: Path, description: str) -> Path:
    if not path.is_file():
        raise ArchDocError(f"{description} not found: {path}", ExitCode.UNRESOLVED)
    return path


def is_managed_view(path: Path) -> bool:
    lines = path.read_text(encoding="utf-8").splitlines()
    return (
        len(lines) >= 4
        and lines[0] == "---"
        and lines[1].startswith("id: ")
        and lines[2].startswith("name: ")
        and lines[3] == "---"
    )


def destination_for(document: Path, target: Path, fragment: str | None = None) -> str:
    relative = Path(os.path.relpath(target, start=document.parent)).as_posix()
    return f"{relative}#{fragment}" if fragment else relative


def render_registry(definitions: Mapping[str, str]) -> str:
    entries = "\n".join(
        f"[{label}]: {definitions[label]}" for label in sorted(definitions)
    )
    return (
        "<!-- arch-doc:references:start -->\n"
        f"{entries}\n"
        "<!-- arch-doc:references:end -->"
    )


def replace_registry(
    document: ArchitectureDocument,
    definitions: Mapping[str, str],
) -> str:
    if document.registry is None:
        prefix = document.text.rstrip("\r\n")
    else:
        prefix = document.text[:document.registry.start_offset].rstrip("\r\n")
    if not definitions:
        return f"{prefix}\n"
    return f"{prefix}\n\n{render_registry(definitions)}\n"


def _document_label(document: ArchitectureDocument, snapshot: RepositorySnapshot) -> str:
    return "document" if snapshot.root is not None and document.path == snapshot.root.path else document.view_id or "document"


def _expected_destination(
    document: ArchitectureDocument, label: str, snapshot: RepositorySnapshot
) -> str | None:
    if label in snapshot.views_by_id:
        target = snapshot.views_by_id[label].path
        return destination_for(document.path, target)
    if label in snapshot.facts:
        root = snapshot.root
        if root is None:
            return None
        if document.path == root.path:
            return snapshot.facts[label]
        return destination_for(document.path, root.path, label)
    return None


def _definition_line(document: ArchitectureDocument, label: str) -> int:
    for line_number, raw_line in enumerate(document.text.splitlines(), start=1):
        definition = _REFERENCE_DEFINITION.fullmatch(raw_line.rstrip("\r"))
        if definition is not None and definition.group(1) == label:
            return line_number
    return 1


def _selected_documents(
    snapshot: RepositorySnapshot, target: ArchitectureDocument | None = None
) -> tuple[ArchitectureDocument, ...]:
    if target is not None:
        return (target,)
    documents: list[ArchitectureDocument] = []
    if snapshot.root is not None:
        documents.append(snapshot.root)
    documents.extend(snapshot.views_by_id.values())
    return tuple(documents)


def _structural_problems(
    snapshot: RepositorySnapshot, target: ArchitectureDocument | None = None, *, adrs_only: bool = False
) -> tuple[StructuralProblem, ...]:
    if adrs_only:
        problems = [
            problem
            for problem in snapshot.problems
            if problem.path.is_relative_to(snapshot.repo / "docs" / "architecture" / "decisions")
        ]
        return tuple(sorted(problems, key=lambda problem: (_relative_path(problem.path, snapshot.repo), problem.line, problem.message)))

    documents = _selected_documents(snapshot, target)
    selected_paths = {document.path for document in documents}
    if target is None:
        problems = [
            problem
            for problem in snapshot.problems
            if not problem.path.is_relative_to(snapshot.repo / "docs" / "architecture" / "decisions")
        ]
    else:
        problems = [problem for problem in snapshot.problems if problem.path in selected_paths]
        if target.view_id is not None:
            duplicate_message = f"Duplicate managed view ID '{target.view_id}'."
            problems.extend(problem for problem in snapshot.problems if problem.message == duplicate_message)
    for document in documents:
        definitions = document.registry.definitions if document.registry is not None else {}
        for use in managed_reference_uses(document.text):
            if use.label not in definitions:
                problems.append(
                    _problem(document.path, use.line, f"Managed reference '{use.label}' has no local definition.")
                )
                continue
            expected = _expected_destination(document, use.label, snapshot)
            if expected is None:
                problems.append(
                    _problem(document.path, use.line, f"Managed reference '{use.label}' has no managed target.")
                )
        for label, destination in definitions.items():
            expected = _expected_destination(document, label, snapshot)
            if expected is None:
                problems.append(
                    _problem(
                        document.path,
                        _definition_line(document, label),
                        f"Registry definition '{label}' has no managed target.",
                    )
                )
            elif destination != expected:
                problems.append(
                    _problem(
                        document.path,
                        _definition_line(document, label),
                        f"Registry destination for '{label}' does not target its managed artifact.",
                    )
                )
    return tuple(
        sorted(set(problems), key=lambda problem: (_relative_path(problem.path, snapshot.repo), problem.line, problem.message))
    )


def _managed_edges(snapshot: RepositorySnapshot) -> tuple[tuple[str, str], ...]:
    edges: list[tuple[str, str]] = []
    for document in _selected_documents(snapshot):
        definitions = document.registry.definitions if document.registry is not None else {}
        source = _document_label(document, snapshot)
        for use in managed_reference_uses(document.text):
            if use.label not in definitions or _expected_destination(document, use.label, snapshot) is None:
                continue
            if document.view_id == use.label:
                continue
            edges.append((source, use.label))
    return tuple(sorted(set(edges)))


def _integration_warnings(snapshot: RepositorySnapshot) -> tuple[tuple[str, int, str], ...]:
    edges = _managed_edges(snapshot)
    outgoing: dict[str, set[str]] = {}
    for source, target in edges:
        outgoing.setdefault(source, set()).add(target)

    reachable: set[str] = set()
    pending = list(outgoing.get("document", set()))
    while pending:
        current = pending.pop()
        if current in reachable or current not in snapshot.views_by_id:
            continue
        reachable.add(current)
        pending.extend(outgoing.get(current, set()))

    consumers: dict[str, set[str]] = {}
    for source, target in edges:
        consumers.setdefault(target, set()).add(source)

    warnings: list[tuple[str, int, str]] = []
    for view_id in snapshot.views_by_id:
        if view_id not in reachable:
            warnings.append((view_id, 0, f"Warning: View '{view_id}' is unreachable from document."))
        if not consumers.get(view_id):
            warnings.append((view_id, 1, f"Warning: View '{view_id}' is not consumed outside its own document."))
    for fact_id in snapshot.facts:
        if not consumers.get(fact_id):
            warnings.append((fact_id, 1, f"Warning: Fact '{fact_id}' is not consumed outside its own document."))

    for document in _selected_documents(snapshot):
        if document.registry is None:
            continue
        raw_uses = {use.label for use in managed_reference_uses(document.text)}
        for label in document.registry.definitions:
            if label in raw_uses:
                continue
            if document.kind == "root" and label in snapshot.facts:
                continue
            warnings.append(
                (
                    label,
                    2,
                    f"Warning: Registry entry '{label}' in '{_document_label(document, snapshot)}' is unused.",
                )
            )
    return tuple(sorted(warnings, key=lambda warning: (warning[0], warning[1], warning[2])))


def _print_problems(problems: Sequence[StructuralProblem], repo: Path) -> None:
    for problem in problems:
        print(f"{_relative_path(problem.path, repo)}:{problem.line}: {problem.message}", file=sys.stderr)


def _print_valid(snapshot: RepositorySnapshot) -> None:
    print("Valid.")
    for _identifier, _category, warning in _integration_warnings(snapshot):
        print(warning)


def _resolve_view(snapshot: RepositorySnapshot, target: str, *, reject_duplicates: bool = True) -> ArchitectureDocument:
    if "/" not in target and "\\" not in target and not target.endswith(".md"):
        document = snapshot.views_by_id.get(target)
        if document is None:
            unresolved_view(target)
        if reject_duplicates:
            _reject_duplicate_view_id(snapshot, document.view_id or target)
        return document
    candidate = _resolve_view_path(snapshot.repo, target)
    document = snapshot.views_by_path.get(candidate)
    if document is None:
        unresolved_view(target)
    if reject_duplicates:
        _reject_duplicate_view_id(snapshot, document.view_id or target)
    return document


def _views_directory(repo: Path) -> Path:
    return (repo / "docs" / "architecture" / "views").resolve()


def _resolve_view_path(repo: Path, target: str) -> Path:
    if not target.endswith(".md"):
        unresolved_view(target)
    raw_path = Path(target)
    base = repo if raw_path.parts and raw_path.parts[0] == "docs" else _views_directory(repo)
    candidate = raw_path.resolve() if raw_path.is_absolute() else (base / raw_path).resolve()
    try:
        candidate.relative_to(_views_directory(repo))
    except ValueError:
        unresolved_view(target)
    return candidate


def _reject_duplicate_view_id(snapshot: RepositorySnapshot, view_id: str) -> None:
    duplicate = f"Duplicate managed view ID '{view_id}'."
    if any(problem.message == duplicate for problem in snapshot.problems):
        raise ArchDocError(duplicate, ExitCode.VALIDATION)


def _registry_payload(definitions: Mapping[str, str], newline: str) -> str:
    return render_registry(definitions).replace("\n", newline)


def _rewrite_registry(document: ArchitectureDocument, text: str, definitions: Mapping[str, str]) -> str:
    newline = "\r\n" if "\r\n" in text else "\n"
    newline_bytes = newline.encode("utf-8")
    payload = text.encode("utf-8")
    if document.registry is None:
        prefix = payload.rstrip(b"\r\n")
    else:
        prefix = payload[: document.registry.start_offset].rstrip(b"\r\n")
    if not definitions:
        return (prefix + newline_bytes).decode("utf-8")
    block = _registry_payload(definitions, newline).encode("utf-8")
    return (prefix + newline_bytes * 2 + block + newline_bytes).decode("utf-8")


def _replace_managed_use_labels(text: str, old_id: str, new_id: str) -> str:
    frontmatter_end_line = _frontmatter_end_line(text)
    frontmatter_end_offset = (
        sum(len(line) for line in text.splitlines(keepends=True)[: frontmatter_end_line + 1])
        if frontmatter_end_line is not None
        else 0
    )
    fenced_ranges = [
        (token.map[0], token.map[1])
        for token in MarkdownIt("commonmark").parse(text)
        if token.type in {"fence", "code_block"} and token.map is not None
    ]
    replacements: list[tuple[int, int]] = []
    index = 0
    while index < len(text):
        if index < frontmatter_end_offset:
            index = frontmatter_end_offset
            continue
        line = text.count("\n", 0, index)
        if any(start <= line < end for start, end in fenced_ranges):
            next_line = text.find("\n", index)
            index = len(text) if next_line == -1 else next_line + 1
            continue
        if text.startswith("<!--", index):
            end = text.find("-->", index + 4)
            index = len(text) if end == -1 else end + 3
            continue
        if text[index] == "`":
            delimiter_end = index
            while delimiter_end < len(text) and text[delimiter_end] == "`":
                delimiter_end += 1
            delimiter = text[index:delimiter_end]
            closing = text.find(delimiter, delimiter_end)
            index = len(text) if closing == -1 else closing + len(delimiter)
            continue
        if text[index] != "[" or _is_escaped(text, index) or (index > 0 and text[index - 1] == "!"):
            index += 1
            continue
        visible_end = text.find("]", index + 1)
        if visible_end == -1 or visible_end == index + 1 or visible_end + 1 >= len(text) or text[visible_end + 1] != "[":
            index += 1
            continue
        label_end = text.find("]", visible_end + 2)
        if label_end == -1:
            index += 1
            continue
        if text[visible_end + 2 : label_end] == old_id:
            replacements.append((visible_end + 2, label_end))
        index = label_end + 1
    for start, end in reversed(replacements):
        text = f"{text[:start]}{new_id}{text[end:]}"
    return text


def _validate_payload(path: Path, text: str, repo: Path) -> None:
    registry = parse_registry(text)
    definitions = registry.definitions if registry is not None else {}
    for use in managed_reference_uses(text):
        if use.label not in definitions:
            raise _validation_error(f"Managed reference '{use.label}' has no local definition.", use.line)
    relative_path = path.resolve().relative_to(repo.resolve())
    if relative_path == Path("docs/architecture/README.md"):
        return
    frontmatter = _parse_view_frontmatter(text)
    if frontmatter is None:
        return
    _view_id, name, body = frontmatter
    if _first_level_one_heading(body) != name:
        raise _validation_error("View name and first level-one heading must match.")


def _mutation_refused(message: str) -> ArchDocError:
    return ArchDocError(f"Refused: {message}", ExitCode.REFUSED)


def unresolved_view(target: str) -> None:
    raise ArchDocError(f"View target not found: {target}", ExitCode.UNRESOLVED)


def handle_inventory(args: argparse.Namespace, root: Path) -> int:
    del args
    snapshot = load_snapshot(root)
    if snapshot.root is None:
        print("Root: missing")
    else:
        print(f"Root: {_relative_path(snapshot.root.path, snapshot.repo)}")
    for view_id, document in sorted(snapshot.views_by_id.items()):
        print(f"View: {view_id} {_relative_path(document.path, snapshot.repo)}")
    for fact_id, destination in sorted(snapshot.facts.items()):
        print(f"Fact: {fact_id} {destination}")
    for identifier, record in sorted(snapshot.adrs.items()):
        print(f"ADR: {identifier} {_relative_path(record.path, snapshot.repo)}")
    for source, target in _managed_edges(snapshot):
        print(f"Edge: {source} -> {target}")
    for _identifier, _category, warning in _integration_warnings(snapshot):
        print(warning)
    return int(ExitCode.OK)


def handle_validate_document(args: argparse.Namespace, root: Path) -> int:
    del args
    require_root(root)
    snapshot = load_snapshot(root)
    problems = _structural_problems(snapshot)
    if problems:
        _print_problems(problems, snapshot.repo)
        return int(ExitCode.VALIDATION)
    _print_valid(snapshot)
    return int(ExitCode.OK)


def handle_validate_view(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    document = _resolve_view(snapshot, args.target, reject_duplicates=False)
    problems = _structural_problems(snapshot, document)
    if problems:
        _print_problems(problems, snapshot.repo)
        return int(ExitCode.VALIDATION)
    _print_valid(snapshot)
    return int(ExitCode.OK)


def handle_validate_adr(args: argparse.Namespace, root: Path) -> int:
    del args
    snapshot = load_snapshot(root)
    problems = _structural_problems(snapshot, adrs_only=True)
    if problems:
        _print_problems(problems, snapshot.repo)
        return int(ExitCode.VALIDATION)
    print("Valid.")
    return int(ExitCode.OK)


def handle_view_resolve(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    document = _resolve_view(snapshot, args.target)
    print(f"{document.view_id} {_relative_path(document.path, snapshot.repo)}")
    return int(ExitCode.OK)


def handle_view_add(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    draft_path = _resolve_view_path(snapshot.repo, args.draft_path)
    if not draft_path.is_file():
        raise _mutation_refused(f"draft view not found: {args.draft_path}")
    try:
        draft = parse_document(draft_path, snapshot.repo)
    except ArchDocError as error:
        raise _mutation_refused(f"draft view is malformed: {args.draft_path}") from error
    if draft.kind != "draft":
        raise _mutation_refused(f"draft view is already managed: {args.draft_path}")
    if not _VIEW_ID.fullmatch(args.id) or args.id.startswith("fact-"):
        raise _mutation_refused(f"invalid view ID: {args.id}")
    if not args.name.strip() or "\n" in args.name or "\r" in args.name:
        raise _mutation_refused("invalid view name")
    if args.id in snapshot.views_by_id or any(
        problem.message == f"Duplicate managed view ID '{args.id}'." for problem in snapshot.problems
    ):
        raise _mutation_refused(f"view ID already exists: {args.id}")

    if args.register_in == "document":
        registration = snapshot.root
        if registration is None:
            if root_document(root).is_file():
                raise _mutation_refused("architecture document is malformed")
            require_root(root)
    else:
        registration = _resolve_view(snapshot, args.register_in)
    assert registration is not None

    frontmatter = f"---\nid: {args.id}\nname: {args.name}\n---\n\n"
    draft_payload = f"{frontmatter}{draft.text}"
    definitions = dict(registration.registry.definitions) if registration.registry is not None else {}
    definitions[args.id] = Path(os.path.relpath(draft_path, registration.path.parent)).as_posix()
    registration_payload = _rewrite_registry(registration, registration.text, definitions)
    try:
        _validate_payload(draft_path, draft_payload, snapshot.repo)
        _validate_payload(registration.path, registration_payload, snapshot.repo)
    except ArchDocError as error:
        raise _mutation_refused(str(error)) from error

    draft_path.write_bytes(draft_payload.encode("utf-8"))
    registration.path.write_bytes(registration_payload.encode("utf-8"))
    print(f"Added view '{args.id}' at {_relative_path(draft_path, snapshot.repo)}.")
    return int(ExitCode.OK)


def handle_view_migrate_id(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    selected = _resolve_view(snapshot, args.target)
    old_id = selected.view_id
    assert old_id is not None
    if not _VIEW_ID.fullmatch(args.new_id) or args.new_id.startswith("fact-"):
        raise _mutation_refused(f"invalid view ID: {args.new_id}")
    if args.new_id == old_id:
        raise _mutation_refused("new view ID must differ from the existing ID")
    if args.new_id in snapshot.views_by_id or any(
        problem.message == f"Duplicate managed view ID '{args.new_id}'." for problem in snapshot.problems
    ):
        raise _mutation_refused(f"view ID already exists: {args.new_id}")

    payloads: dict[Path, str] = {}
    for document in _selected_documents(snapshot):
        definitions = dict(document.registry.definitions) if document.registry is not None else {}
        if old_id in definitions:
            definitions[args.new_id] = definitions.pop(old_id)
        payload = _rewrite_registry(document, document.text, definitions) if old_id in (
            document.registry.definitions if document.registry is not None else {}
        ) else document.text
        payload = _replace_managed_use_labels(payload, old_id, args.new_id)
        if document.path == selected.path:
            payload = re.sub(
                rf"(?m)^id: {re.escape(old_id)}(?=\r?$)", f"id: {args.new_id}", payload, count=1
            )
        if payload != document.text:
            payloads[document.path] = payload
    try:
        for path, payload in payloads.items():
            _validate_payload(path, payload, snapshot.repo)
    except ArchDocError as error:
        raise _mutation_refused(str(error)) from error

    for path, payload in payloads.items():
        path.write_bytes(payload.encode("utf-8"))
    print(f"Migrated view ID '{old_id}' to '{args.new_id}'.")
    return int(ExitCode.OK)


def handle_view_retire(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    selected = _resolve_view(snapshot, args.target)
    view_id = selected.view_id
    assert view_id is not None
    for document in _selected_documents(snapshot):
        if document.path == selected.path:
            continue
        definitions = document.registry.definitions if document.registry is not None else {}
        if view_id in definitions or any(use.label == view_id for use in managed_reference_uses(document.text)):
            raise _mutation_refused(f"view '{view_id}' is still referenced by {_document_label(document, snapshot)}")
    selected.path.unlink()
    print(f"Retired view '{view_id}' at {_relative_path(selected.path, snapshot.repo)}.")
    return int(ExitCode.OK)


def _resolve_reference_document(snapshot: RepositorySnapshot, target: str) -> ArchitectureDocument:
    if target == "document":
        require_root(snapshot.repo)
        if snapshot.root is None:
            raise _mutation_refused("architecture document is malformed")
        return snapshot.root
    return _resolve_view(snapshot, target)


def _is_registered_root_fact(snapshot: RepositorySnapshot, label: str) -> bool:
    return (
        snapshot.root is not None
        and snapshot.root.registry is not None
        and snapshot.root.registry.definitions.get(label) == f"#{label}"
    )


def _fact_structural_problems(snapshot: RepositorySnapshot, label: str) -> tuple[StructuralProblem, ...]:
    message = f"Duplicate fact anchor: {label}."
    return tuple(problem for problem in snapshot.problems if problem.message == message)


def _resolve_reference_label(
    snapshot: RepositorySnapshot,
    label: str,
    source: ArchitectureDocument,
    *,
    removing: bool = False,
) -> None:
    if label in snapshot.views_by_id:
        _reject_duplicate_view_id(snapshot, label)
        return
    if not label.startswith("fact-"):
        unresolved_view(label)
    problems = _fact_structural_problems(snapshot, label)
    if problems:
        raise ArchDocError(problems[0].message, ExitCode.VALIDATION)
    if label not in snapshot.facts:
        if removing and source.kind == "root" and _is_registered_root_fact(snapshot, label):
            return
        raise ArchDocError(f"Fact target not found: {label}", ExitCode.UNRESOLVED)
    if _is_registered_root_fact(snapshot, label):
        return
    raise ArchDocError(f"Fact target not found: {label}", ExitCode.UNRESOLVED)


def _preflight_reference_payload(
    snapshot: RepositorySnapshot,
    document: ArchitectureDocument,
    payload: str,
) -> None:
    _validate_payload(document.path, payload, snapshot.repo)
    registry = parse_registry(payload)
    if registry is None:
        return
    for label, destination in registry.definitions.items():
        expected = _expected_destination(document, label, snapshot)
        if expected is None:
            raise _validation_error(f"Registry definition '{label}' has no managed target.")
        if destination != expected:
            raise _validation_error(f"Registry destination for '{label}' does not target its managed artifact.")


def _updated_registry_definitions(
    snapshot: RepositorySnapshot,
    document: ArchitectureDocument,
) -> dict[str, str]:
    definitions = dict(document.registry.definitions) if document.registry is not None else {}
    for use in managed_reference_uses(document.text):
        expected = _expected_destination(document, use.label, snapshot)
        if expected is None:
            raise _validation_error(f"Managed reference '{use.label}' has no managed target.", use.line)
        definitions.setdefault(use.label, expected)
    for label in definitions:
        expected = _expected_destination(document, label, snapshot)
        if expected is None:
            raise _validation_error(f"Registry definition '{label}' has no managed target.")
        definitions[label] = expected
    return definitions


def handle_references_add(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    document = _resolve_reference_document(snapshot, args.document_or_view)
    _resolve_reference_label(snapshot, args.view_or_fact_id, document)
    definitions = dict(document.registry.definitions) if document.registry is not None else {}
    expected = _expected_destination(document, args.view_or_fact_id, snapshot)
    assert expected is not None
    definitions[args.view_or_fact_id] = expected
    payload = _rewrite_registry(document, document.text, definitions)
    try:
        _preflight_reference_payload(snapshot, document, payload)
    except ArchDocError as error:
        raise _mutation_refused(str(error)) from error
    document.path.write_bytes(payload.encode("utf-8"))
    print(f"Added reference '{args.view_or_fact_id}' to {_document_label(document, snapshot)}.")
    return int(ExitCode.OK)


def handle_references_remove(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    document = _resolve_reference_document(snapshot, args.document_or_view)
    _resolve_reference_label(snapshot, args.view_or_fact_id, document, removing=True)
    definitions = dict(document.registry.definitions) if document.registry is not None else {}
    if args.view_or_fact_id not in definitions:
        raise _mutation_refused(
            f"reference '{args.view_or_fact_id}' is not registered by {_document_label(document, snapshot)}"
        )
    consumers = _selected_documents(snapshot) if document.kind == "root" and args.view_or_fact_id.startswith("fact-") else (document,)
    for consumer in consumers:
        if any(use.label == args.view_or_fact_id for use in managed_reference_uses(consumer.text)):
            raise _mutation_refused(
                f"reference '{args.view_or_fact_id}' is still used by {_document_label(consumer, snapshot)}"
            )
    definitions.pop(args.view_or_fact_id)
    payload = _rewrite_registry(document, document.text, definitions)
    try:
        _preflight_reference_payload(snapshot, document, payload)
    except ArchDocError as error:
        raise _mutation_refused(str(error)) from error
    document.path.write_bytes(payload.encode("utf-8"))
    print(f"Removed reference '{args.view_or_fact_id}' from {_document_label(document, snapshot)}.")
    return int(ExitCode.OK)


def handle_references_update(args: argparse.Namespace, root: Path) -> int:
    snapshot = load_snapshot(root)
    if args.document_or_view is None:
        require_root(root)
        if snapshot.root is None:
            raise _mutation_refused("architecture document is malformed")
        documents = _selected_documents(snapshot)
    else:
        documents = (_resolve_reference_document(snapshot, args.document_or_view),)
    payloads: dict[Path, str] = {}
    try:
        for document in documents:
            definitions = _updated_registry_definitions(snapshot, document)
            payload = _rewrite_registry(document, document.text, definitions)
            _preflight_reference_payload(snapshot, document, payload)
            if payload != document.text:
                payloads[document.path] = payload
    except ArchDocError as error:
        raise _mutation_refused(str(error)) from error
    for path, payload in payloads.items():
        path.write_bytes(payload.encode("utf-8"))
    if len(documents) == 1:
        print(f"Updated references in {_document_label(documents[0], snapshot)}.")
    else:
        print(f"Updated references in {len(documents)} documents.")
    return int(ExitCode.OK)


def handle_fact_consumers(args: argparse.Namespace, root: Path) -> int:
    if not _VIEW_ID.fullmatch(args.fact_id) or not args.fact_id.startswith("fact-"):
        raise ArchDocError(f"Invalid fact ID: {args.fact_id}", ExitCode.USAGE)
    snapshot = load_snapshot(root)
    require_root(root)
    problems = _fact_structural_problems(snapshot, args.fact_id)
    if problems:
        _print_problems(problems, snapshot.repo)
        return int(ExitCode.VALIDATION)
    if args.fact_id not in snapshot.facts or not _is_registered_root_fact(snapshot, args.fact_id):
        raise ArchDocError(f"Fact target not found: {args.fact_id}", ExitCode.UNRESOLVED)
    consumers = sorted(
        _relative_path(document.path, snapshot.repo)
        for document in snapshot.views_by_id.values()
        if any(use.label == args.fact_id for use in managed_reference_uses(document.text))
    )
    for consumer in consumers:
        print(consumer)
    return int(ExitCode.OK)


def _render_adr_frontmatter(
    *,
    status: str,
    recorded_date: str,
    retrospective: bool,
    supersedes: Sequence[str],
    superseded_by: str | None,
    newline: bytes,
) -> bytes:
    lines = [b"---", f"status: {status}".encode(), f"date: {recorded_date}".encode()]
    if retrospective:
        lines.append(b"retrospective: true")
    if supersedes:
        lines.append(b"supersedes:")
        lines.extend(f"  - '{identifier}'".encode() for identifier in supersedes)
    if superseded_by is not None:
        lines.append(f"superseded-by: '{superseded_by}'".encode())
    lines.append(b"---")
    return newline.join(lines) + newline


def _validated_adr_date(value: str) -> str:
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", value):
        raise _mutation_refused(f"invalid ADR date: {value}")
    try:
        calendar_date.fromisoformat(value)
    except ValueError as error:
        raise _mutation_refused(f"invalid ADR date: {value}") from error
    return value


def _validate_adr_body_input(payload: bytes) -> str:
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError as error:
        raise _mutation_refused("ADR body must be UTF-8") from error
    if text.splitlines()[:1] == ["---"]:
        raise _mutation_refused("ADR body must be frontmatter-free")
    tokens = MarkdownIt("commonmark").parse(text)
    if any(token.type == "heading_open" and token.tag == "h1" for token in tokens) or not _valid_adr_sections(text):
        raise _mutation_refused(
            "ADR body must contain Context, Decision, Consequences, Alternatives considered, and optional final Links headings in order"
        )
    return text


def handle_adr_record(args: argparse.Namespace, root: Path) -> int:
    body_path = require_file(Path(args.body_path), "ADR body")
    if not args.title.strip() or "\n" in args.title or "\r" in args.title:
        raise _mutation_refused("invalid ADR title")
    if not _VIEW_ID.fullmatch(args.slug):
        raise _mutation_refused(f"invalid ADR slug: {args.slug}")
    recorded_date = _validated_adr_date(args.date)
    if len(set(args.supersedes)) != len(args.supersedes):
        raise _mutation_refused("duplicate supersedes identifier")
    if any(not _ADR_ID.fullmatch(identifier) for identifier in args.supersedes):
        raise _mutation_refused("invalid supersedes identifier")

    body_payload = body_path.read_bytes()
    _validate_adr_body_input(body_payload)
    snapshot = load_snapshot(root)
    existing_problems = _structural_problems(snapshot, adrs_only=True)
    if existing_problems:
        _print_problems(existing_problems, snapshot.repo)
        return int(ExitCode.VALIDATION)

    decisions = root / "docs" / "architecture" / "decisions"
    identifiers = [
        int(match.group(1))
        for path in decisions.rglob("*.md") if decisions.is_dir()
        if (match := _ADR_NUMBERED_FILE.fullmatch(path.name)) is not None
    ]
    next_number = max(identifiers, default=0) + 1
    if next_number > 9999:
        raise _mutation_refused("ADR identifier space is exhausted")
    identifier = f"{next_number:04d}"

    selected: list[AdrRecord] = []
    for replaced_id in args.supersedes:
        record = snapshot.adrs.get(replaced_id)
        if record is None:
            raise ArchDocError(f"ADR target not found: {replaced_id}", ExitCode.UNRESOLVED)
        if record.status == "superseded":
            raise _mutation_refused(f"ADR '{replaced_id}' is already superseded")
        selected.append(record)

    newline = b"\r\n" if b"\r\n" in body_payload else b"\n"
    created_path = decisions / f"{identifier}-{args.slug}.md"
    created_payload = (
        _render_adr_frontmatter(
            status="accepted",
            recorded_date=recorded_date,
            retrospective=args.retrospective,
            supersedes=args.supersedes,
            superseded_by=None,
            newline=newline,
        )
        + newline
        + f"# {identifier}: {args.title.strip()}".encode("utf-8")
        + newline * 2
        + body_payload
    )
    payloads: dict[Path, bytes] = {created_path: created_payload}
    for record in selected:
        payloads[record.path] = (
            _render_adr_frontmatter(
                status="superseded",
                recorded_date=record.date,
                retrospective=record.retrospective,
                supersedes=record.supersedes,
                superseded_by=identifier,
                newline=record.newline,
            )
            + record.body
        )

    preflight = dict(snapshot.adrs)
    try:
        for path, payload in payloads.items():
            record = _parse_adr_payload(path, payload)
            preflight[record.identifier] = record
    except ArchDocError as error:
        raise _mutation_refused(str(error)) from error
    relationship_problems = _adr_relationship_problems(preflight, snapshot.repo)
    if relationship_problems:
        raise _mutation_refused(relationship_problems[0].message)

    decisions.mkdir(parents=True, exist_ok=True)
    for path, payload in payloads.items():
        path.write_bytes(payload)
    print(_relative_path(created_path, snapshot.repo))
    return int(ExitCode.OK)


def add_handler(parser: argparse.ArgumentParser, handler) -> argparse.ArgumentParser:
    parser.set_defaults(handler=handler)
    return parser


def build_parser() -> ArgumentParser:
    parser = ArgumentParser(prog="arch_doc.py")
    commands = parser.add_subparsers(dest="command", required=True)

    add_handler(commands.add_parser("inventory"), handle_inventory)

    validate = commands.add_parser("validate")
    validate_commands = validate.add_subparsers(dest="validate_command", required=True)
    add_handler(validate_commands.add_parser("document"), handle_validate_document)
    validate_view = add_handler(validate_commands.add_parser("view"), handle_validate_view)
    validate_view.add_argument("target")
    add_handler(validate_commands.add_parser("adr"), handle_validate_adr)

    view = commands.add_parser("view")
    view_commands = view.add_subparsers(dest="view_command", required=True)
    resolve = add_handler(view_commands.add_parser("resolve"), handle_view_resolve)
    resolve.add_argument("target")
    add = add_handler(view_commands.add_parser("add"), handle_view_add)
    add.add_argument("draft_path")
    add.add_argument("--id", required=True)
    add.add_argument("--name", required=True)
    add.add_argument("--register-in", required=True)
    migrate = add_handler(view_commands.add_parser("migrate-id"), handle_view_migrate_id)
    migrate.add_argument("target")
    migrate.add_argument("new_id")
    retire = add_handler(view_commands.add_parser("retire"), handle_view_retire)
    retire.add_argument("target")

    references = commands.add_parser("references")
    reference_commands = references.add_subparsers(dest="references_command", required=True)
    for name, handler in (("add", handle_references_add), ("remove", handle_references_remove)):
        reference = add_handler(reference_commands.add_parser(name), handler)
        reference.add_argument("document_or_view")
        reference.add_argument("view_or_fact_id")
    update = add_handler(reference_commands.add_parser("update"), handle_references_update)
    update.add_argument("document_or_view", nargs="?")

    fact = commands.add_parser("fact")
    fact_commands = fact.add_subparsers(dest="fact_command", required=True)
    consumers = add_handler(fact_commands.add_parser("consumers"), handle_fact_consumers)
    consumers.add_argument("fact_id")

    adr = commands.add_parser("adr")
    adr_commands = adr.add_subparsers(dest="adr_command", required=True)
    record = add_handler(adr_commands.add_parser("record"), handle_adr_record)
    record.add_argument("body_path")
    record.add_argument("--title", required=True)
    record.add_argument("--slug", required=True)
    record.add_argument("--date", required=True)
    record.add_argument("--retrospective", action="store_true")
    record.add_argument("--supersedes", action="append", default=[])

    return parser


def dispatch(args: argparse.Namespace, root: Path) -> int:
    return args.handler(args, root)


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    try:
        args = parser.parse_args(argv)
        return dispatch(args, repository_root())
    except ArchDocError as error:
        print(error, file=sys.stderr)
        return int(error.code)


if __name__ == "__main__":
    raise SystemExit(main())