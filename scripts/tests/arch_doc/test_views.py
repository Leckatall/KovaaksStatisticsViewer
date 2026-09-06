from __future__ import annotations

from pathlib import Path

import pytest

from scripts.arch_doc import _replace_managed_use_labels


def write_root(repo: Path, body: str = "# Architecture\n") -> Path:
    path = repo / "docs" / "architecture" / "README.md"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")
    return path


def write_view(repo: Path, view_id: str, *, path: str | None = None, body: str = "") -> Path:
    target = repo / "docs" / "architecture" / "views" / (path or f"{view_id}.md")
    target.parent.mkdir(parents=True, exist_ok=True)
    name = view_id.replace("-", " ").title()
    target.write_text(f"---\nid: {view_id}\nname: {name}\n---\n# {name}\n{body}", encoding="utf-8")
    return target


def write_draft(repo: Path, path: str, body: str = "# Candidate\n") -> Path:
    draft = repo / "docs" / "architecture" / "views" / path
    draft.parent.mkdir(parents=True, exist_ok=True)
    draft.write_text(body, encoding="utf-8")
    return draft


def registry(*definitions: tuple[str, str]) -> str:
    entries = "".join(f"[{label}]: {destination}\n" for label, destination in definitions)
    return f"\n<!-- arch-doc:references:start -->\n{entries}<!-- arch-doc:references:end -->\n"


def tree_bytes(repo: Path) -> dict[Path, bytes]:
    return {path.relative_to(repo): path.read_bytes() for path in repo.rglob("*") if path.is_file()}


@pytest.mark.parametrize(
    ("target", "expected"),
    [
        ("live-ingestion", "live-ingestion docs/architecture/views/runtime/live-ingestion.md\n"),
        ("docs/architecture/views/runtime/live-ingestion.md", "live-ingestion docs/architecture/views/runtime/live-ingestion.md\n"),
        ("runtime/live-ingestion.md", "live-ingestion docs/architecture/views/runtime/live-ingestion.md\n"),
        (r"runtime\live-ingestion.md", "live-ingestion docs/architecture/views/runtime/live-ingestion.md\n"),
    ],
    ids=("exact-id", "repository-path", "views-path", "backslash-path"),
)
def test_view_resolve_accepts_exact_ids_and_canonical_in_tree_paths(run_cli, target: str, expected: str) -> None:
    """Catches target lookup resolving IDs loosely or paths from the caller directory."""
    write_view(run_cli.repo, "live-ingestion", path="runtime/live-ingestion.md")

    result = run_cli("view", "resolve", target)

    assert result.returncode == 0
    assert result.stdout == expected
    assert result.stderr == ""


def test_view_resolve_is_case_sensitive_and_accepts_an_absolute_in_tree_path(run_cli) -> None:
    """Catches resolving a different stable ID or rejecting an absolute canonical view path."""
    view = write_view(run_cli.repo, "live-ingestion", path="runtime/live-ingestion.md")

    wrong_case = run_cli("view", "resolve", "Live-Ingestion")
    absolute_in_tree = run_cli("view", "resolve", str(view))

    assert wrong_case.returncode == 4
    assert absolute_in_tree.returncode == 0
    assert absolute_in_tree.stdout == "live-ingestion docs/architecture/views/runtime/live-ingestion.md\n"


def test_view_resolve_rejects_an_absolute_symlink_escape(run_cli) -> None:
    """Catches canonicalising an absolute symlink path outside the views directory."""
    escaped = run_cli.repo / "docs" / "architecture" / "views" / "escaped.md"
    try:
        escaped.symlink_to(run_cli.repo.parent / "outside-target.md")
    except OSError:
        pytest.skip("symlinks are unavailable in this test environment")

    absolute_escape = run_cli("view", "resolve", str(escaped))

    assert absolute_escape.returncode == 4


def test_view_resolve_rejects_duplicate_ids_and_non_markdown_paths(run_cli) -> None:
    """Catches picking an arbitrary duplicate view or accepting a non-view path."""
    write_view(run_cli.repo, "duplicate", path="one.md")
    write_view(run_cli.repo, "duplicate", path="two.md")

    duplicate = run_cli("view", "resolve", "duplicate")
    non_markdown = run_cli("view", "resolve", "runtime/not-a-view.txt")

    assert duplicate.returncode == 3
    assert "Duplicate managed view ID 'duplicate'." in duplicate.stderr
    assert non_markdown.returncode == 4


def test_view_add_prefixes_a_draft_without_reformatting_its_body_and_registers_it(run_cli) -> None:
    """Catches view addition rewriting confirmed prose or failing to register the new view."""
    root = write_root(run_cli.repo, "# Architecture\n")
    draft = write_draft(run_cli.repo, "runtime/live-ingestion.md", "# Live ingestion\n\nBody with  two spaces.\n")

    result = run_cli(
        "view", "add", "runtime/live-ingestion.md", "--id", "live-ingestion", "--name", "Live ingestion", "--register-in", "document"
    )

    assert result.returncode == 0
    assert result.stdout == "Added view 'live-ingestion' at docs/architecture/views/runtime/live-ingestion.md.\n"
    assert result.stderr == ""
    assert draft.read_text(encoding="utf-8") == (
        "---\nid: live-ingestion\nname: Live ingestion\n---\n\n# Live ingestion\n\nBody with  two spaces.\n"
    )
    assert root.read_text(encoding="utf-8") == (
        "# Architecture\n\n<!-- arch-doc:references:start -->\n"
        "[live-ingestion]: views/runtime/live-ingestion.md\n<!-- arch-doc:references:end -->\n"
    )


@pytest.mark.parametrize("contents", ["---\nid: managed\nname: Managed\n---\n# Managed\n", "---\nname: Bad\nid: bad\n---\n# Bad\n"])
def test_view_add_refuses_to_replace_a_managed_or_malformed_target_without_writing(run_cli, contents: str) -> None:
    """Catches add overwriting a document that is not the explicit frontmatter-free draft operand."""
    write_root(run_cli.repo)
    target = write_draft(run_cli.repo, "candidate.md", contents)
    before = tree_bytes(run_cli.repo)

    result = run_cli("view", "add", "candidate.md", "--id", "candidate", "--name", "Candidate", "--register-in", "document")

    assert target.exists()
    assert result.returncode == 5
    assert tree_bytes(run_cli.repo) == before


def test_view_add_requires_an_explicit_managed_registration_target_without_writing(run_cli) -> None:
    """Catches add silently selecting the root or another arbitrary registration target."""
    write_root(run_cli.repo)
    write_draft(run_cli.repo, "candidate.md")
    before = tree_bytes(run_cli.repo)

    result = run_cli("view", "add", "candidate.md", "--id", "candidate", "--name", "Candidate", "--register-in", "missing")

    assert result.returncode == 4
    assert tree_bytes(run_cli.repo) == before


def test_view_add_registers_in_an_existing_view_without_a_root_document(run_cli) -> None:
    """Catches requiring the root when the requested registration target is already a managed view."""
    registration = write_view(run_cli.repo, "parent")
    draft = write_draft(run_cli.repo, "candidate.md")

    result = run_cli("view", "add", "candidate.md", "--id", "candidate", "--name", "Candidate", "--register-in", "parent")

    assert result.returncode == 0
    assert draft.read_text(encoding="utf-8").startswith("---\nid: candidate\nname: Candidate\n---\n\n")
    assert "[candidate]: candidate.md" in registration.read_text(encoding="utf-8")


def test_view_add_refuses_a_malformed_document_registration_target_without_writing(run_cli) -> None:
    """Catches a malformed root reaching an assertion instead of a controlled mutation refusal."""
    write_root(run_cli.repo, "# Architecture\n\n<!-- arch-doc:references:start -->\n")
    write_draft(run_cli.repo, "candidate.md")
    before = tree_bytes(run_cli.repo)

    result = run_cli("view", "add", "candidate.md", "--id", "candidate", "--name", "Candidate", "--register-in", "document")

    assert result.returncode == 5
    assert "Refused:" in result.stderr
    assert "Traceback" not in result.stderr
    assert tree_bytes(run_cli.repo) == before


def test_view_migrate_id_changes_only_exact_managed_labels_registry_keys_and_selected_frontmatter(run_cli) -> None:
    """Catches migration changing visible prose, unrelated text, or only some incoming managed references."""
    root = write_root(
        run_cli.repo,
        "# Architecture\n\n[Old visible text][old-id]\nThe old-id prose remains."
        + registry(("old-id", "views/old.md")),
    )
    selected = write_view(run_cli.repo, "old-id", path="old.md")
    consumer = write_view(
        run_cli.repo,
        "consumer",
        body="\n[Visible old-id text][old-id]\n`[code][old-id]`\n" + registry(("old-id", "old.md")),
    )

    result = run_cli("view", "migrate-id", "old-id", "new-id")

    assert result.returncode == 0
    assert result.stdout == "Migrated view ID 'old-id' to 'new-id'.\n"
    assert result.stderr == ""
    assert "id: new-id" in selected.read_text(encoding="utf-8")
    assert "[Old visible text][new-id]" in root.read_text(encoding="utf-8")
    assert "The old-id prose remains." in root.read_text(encoding="utf-8")
    consumer_text = consumer.read_text(encoding="utf-8")
    assert "[Visible old-id text][new-id]" in consumer_text
    assert "`[code][old-id]`" in consumer_text
    assert "[new-id]: old.md" in consumer_text


def test_view_migrate_id_preserves_reference_shaped_frontmatter_values() -> None:
    """Catches migration treating frontmatter prose as an exact managed reference use."""
    source = (
        "---\nid: consumer\nname: '[Frontmatter prose][old-id]'\n---\n"
        "[Managed body use][old-id]\n"
    )

    migrated = _replace_managed_use_labels(source, "old-id", "new-id")

    assert "name: '[Frontmatter prose][old-id]'" in migrated
    assert "[Managed body use][new-id]" in migrated


def test_view_migrate_id_preflights_all_documents_before_writing(run_cli) -> None:
    """Catches migration partially updating files when the proposed ID is already managed."""
    write_root(run_cli.repo, "# Architecture\n\n[Old][old-id]" + registry(("old-id", "views/old.md")))
    write_view(run_cli.repo, "old-id", path="old.md")
    write_view(run_cli.repo, "new-id", path="existing.md")
    before = tree_bytes(run_cli.repo)

    result = run_cli("view", "migrate-id", "old-id", "new-id")

    assert result.returncode == 5
    assert tree_bytes(run_cli.repo) == before


def test_view_retire_refuses_while_any_external_use_or_registration_remains(run_cli) -> None:
    """Catches retirement deleting a view still referenced by an architecture document."""
    write_root(run_cli.repo, "# Architecture\n\n[Still used][still-used]" + registry(("still-used", "views/still-used.md")))
    view = write_view(run_cli.repo, "still-used")
    before = tree_bytes(run_cli.repo)

    result = run_cli("view", "retire", "still-used")

    assert result.returncode == 5
    assert view.exists()
    assert tree_bytes(run_cli.repo) == before


def test_view_retire_unlinks_an_unreferenced_view(run_cli) -> None:
    """Catches retirement retaining a managed view after all external relationships are removed."""
    write_root(run_cli.repo)
    view = write_view(run_cli.repo, "retired")

    result = run_cli("view", "retire", "retired")

    assert result.returncode == 0
    assert result.stdout == "Retired view 'retired' at docs/architecture/views/retired.md.\n"
    assert result.stderr == ""
    assert not view.exists()