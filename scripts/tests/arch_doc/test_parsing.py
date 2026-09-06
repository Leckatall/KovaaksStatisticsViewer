from __future__ import annotations

from pathlib import Path

import pytest

from scripts.arch_doc import (
    ArchDocError,
    ArchitectureDocument,
    ExitCode,
    ReferenceUse,
    managed_reference_uses,
    parse_document,
    parse_registry,
)


def write_view(repo: Path, text: str, name: str = "candidate.md") -> Path:
    path = repo / "docs" / "architecture" / "views" / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


@pytest.mark.parametrize(
    ("frontmatter", "expected_id", "expected_name"),
    [
        ("", None, None),
        (
            "---\nid: live-ingestion\nname: Live ingestion\n---\n",
            "live-ingestion",
            "Live ingestion",
        ),
    ],
    ids=("frontmatter-free-draft", "canonical-managed-view"),
)
def test_parse_document_classifies_only_canonical_frontmatter_as_a_view(
    tmp_path: Path,
    frontmatter: str,
    expected_id: str | None,
    expected_name: str | None,
) -> None:
    """Catches a draft being promoted without canonical managed frontmatter."""
    path = write_view(tmp_path, f"{frontmatter}# Live ingestion\n")

    document = parse_document(path, tmp_path)

    assert document.kind == ("view" if expected_id else "draft")
    assert document.view_id == expected_id
    assert document.name == expected_name


@pytest.mark.parametrize(
    ("frontmatter", "heading", "reason"),
    [
        ("name: Live ingestion\nid: live-ingestion", "Live ingestion", "key order"),
        ("id: live-ingestion\nname: Live ingestion\nsummary: Extra", "Live ingestion", "key order"),
        ("id: live-ingestion\nid: second\nname: Live ingestion", "Live ingestion", "key order"),
        ("id:\nname: Live ingestion", "Live ingestion", "id"),
        ("id: live-ingestion\nname:", "Live ingestion", "name"),
        ("id: 42\nname: Live ingestion", "Live ingestion", "id"),
        ("id: live-ingestion\nname: true", "Live ingestion", "name"),
        ("id: live_ingestion\nname: Live ingestion", "Live ingestion", "ID"),
        ("id: fact-profile-authority\nname: Profile authority", "Profile authority", "ID"),
        ("id: live-ingestion\nname: Live ingestion", "Different heading", "heading"),
    ],
    ids=(
        "reordered",
        "extra",
        "duplicate",
        "empty-id",
        "empty-name",
        "non-string-id",
        "non-string-name",
        "non-kebab-id",
        "reserved-fact-id",
        "heading-mismatch",
    ),
)
def test_parse_document_rejects_every_noncanonical_managed_view_shape(
    tmp_path: Path,
    frontmatter: str,
    heading: str,
    reason: str,
) -> None:
    """Catches accepting malformed metadata that makes a view identity ambiguous."""
    path = write_view(tmp_path, f"---\n{frontmatter}\n---\n# {heading}\n")

    with pytest.raises(ArchDocError, match=reason) as error:
        parse_document(path, tmp_path)

    assert error.value.code == ExitCode.VALIDATION


def test_parse_document_recognises_the_fixed_root_without_view_frontmatter(tmp_path: Path) -> None:
    """Catches treating the root document as an unmanaged draft."""
    path = tmp_path / "docs" / "architecture" / "README.md"
    path.parent.mkdir(parents=True)
    path.write_text("# Architecture\n", encoding="utf-8")

    document = parse_document(path, tmp_path)

    assert document.kind == "root"
    assert document.view_id is None
    assert document.name is None


def test_parse_registry_returns_definitions_and_utf8_byte_bounds_for_the_final_block() -> None:
    """Catches registry rewrites targeting character positions or surrounding prose."""
    text = (
        "# Architecture\n\n"
        "π before the registry\n\n"
        "<!-- arch-doc:references:start -->\n"
        "[fact-profile-authority]: #fact-profile-authority\n"
        "[live-ingestion]: runtime/live-ingestion.md\n"
        "<!-- arch-doc:references:end -->\n\n"
    )

    registry = parse_registry(text)

    assert registry is not None
    assert dict(registry.definitions) == {
        "fact-profile-authority": "#fact-profile-authority",
        "live-ingestion": "runtime/live-ingestion.md",
    }
    assert registry.start_offset == len(text[: text.index("<!-- arch-doc:references:start -->")].encode())
    assert registry.end_offset == len(
        text[: text.index("<!-- arch-doc:references:end -->") + len("<!-- arch-doc:references:end -->")].encode()
    )


def test_parse_document_preserves_crlf_source_and_registry_byte_bounds(tmp_path: Path) -> None:
    """Catches a registry rewrite using normalized instead of on-disk byte offsets."""
    text = (
        "---\r\n"
        "id: live-ingestion\r\n"
        "name: Live ingestion\r\n"
        "---\r\n"
        "# Live ingestion\r\n\r\n"
        "<!-- arch-doc:references:start -->\r\n"
        "[live-ingestion]: runtime/live-ingestion.md\r\n"
        "<!-- arch-doc:references:end -->\r\n"
    )
    path = tmp_path / "docs" / "architecture" / "views" / "candidate.md"
    path.parent.mkdir(parents=True)
    path.write_bytes(text.encode("utf-8"))

    document = parse_document(path, tmp_path)

    assert document.text == text
    assert document.registry is not None
    assert path.read_bytes()[document.registry.start_offset : document.registry.end_offset] == (
        "<!-- arch-doc:references:start -->\r\n"
        "[live-ingestion]: runtime/live-ingestion.md\r\n"
        "<!-- arch-doc:references:end -->"
    ).encode("utf-8")


@pytest.mark.parametrize(
    ("text", "reason"),
    [
        (
            "<!-- arch-doc:references:start -->\n[one]: one.md\n"
            "<!-- arch-doc:references:end -->\n\n"
            "<!-- arch-doc:references:start -->\n[two]: two.md\n"
            "<!-- arch-doc:references:end -->\n",
            "registry",
        ),
        (
            "<!-- arch-doc:references:start -->\n[one]: one.md\n"
            "[one]: another.md\n<!-- arch-doc:references:end -->\n",
            "Duplicate registry definition",
        ),
        (
            "<!-- arch-doc:references:start -->\n[one]: one.md\n"
            "<!-- arch-doc:references:end -->\n\nAfter the registry.\n",
            "final nonblank block",
        ),
    ],
    ids=("duplicate-registries", "duplicate-definition", "registry-not-final"),
)
def test_parse_registry_rejects_ambiguous_or_misplaced_registry_blocks(text: str, reason: str) -> None:
    """Catches an unsafe registry replacement when delimiters or definitions are ambiguous."""
    with pytest.raises(ArchDocError, match=reason) as error:
        parse_registry(text)

    assert error.value.code == ExitCode.VALIDATION


@pytest.mark.parametrize("closing_fence", ("```\n", ""), ids=("closed", "unclosed"))
def test_parse_document_ignores_registry_markers_documented_in_fenced_code(
    tmp_path: Path, closing_fence: str
) -> None:
    """Catches fenced syntax examples being mistaken for a live registry."""
    path = write_view(
        tmp_path,
        "# Draft\n\n```markdown\n"
        "<!-- arch-doc:references:start -->\n"
        "[live-ingestion]: runtime/live-ingestion.md\n"
        "<!-- arch-doc:references:end -->\n"
        f"{closing_fence}",
    )

    document = parse_document(path, tmp_path)

    assert document.kind == "draft"
    assert document.registry is None


def test_parse_document_does_not_count_a_managed_view_as_its_own_consumer(tmp_path: Path) -> None:
    """Catches a self-reference making an otherwise unconsumed view look integrated."""
    path = write_view(
        tmp_path,
        "---\nid: live-ingestion\nname: Live ingestion\n---\n"
        "# Live ingestion\n\n"
        "[This view][live-ingestion]\n\n"
        "<!-- arch-doc:references:start -->\n"
        "[live-ingestion]: runtime/live-ingestion.md\n"
        "<!-- arch-doc:references:end -->\n",
    )

    document = parse_document(path, tmp_path)

    assert document.uses == ()


def test_managed_reference_uses_keeps_only_explicit_full_reference_links_in_prose(tmp_path: Path) -> None:
    """Catches shortcut, inline, image, code, comment, or fenced text entering the managed graph."""
    text = """[Managed details][live-ingestion]
[shortcut]
[collapsed][]
[inline](runtime/live-ingestion.md)
![image][live-ingestion]
`[code][live-ingestion]`
<!-- [comment][live-ingestion] -->
```text
[fenced][live-ingestion]
```

<!-- arch-doc:references:start -->
[live-ingestion]: runtime/live-ingestion.md
<!-- arch-doc:references:end -->
"""
    path = write_view(tmp_path, text)

    document = parse_document(path, tmp_path)

    assert managed_reference_uses(text) == (ReferenceUse("live-ingestion", 1),)
    assert document.uses == (ReferenceUse("live-ingestion", 1),)
    assert "[inline](runtime/live-ingestion.md)" in document.text
    assert isinstance(document, ArchitectureDocument)