from __future__ import annotations

from pathlib import Path

from scripts.arch_doc import RepositorySnapshot, load_snapshot


def write_root(repo: Path, text: str) -> Path:
    path = repo / "docs" / "architecture" / "README.md"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def write_view(repo: Path, view_id: str, *, path: str | None = None, body: str = "") -> Path:
    target = repo / "docs" / "architecture" / "views" / (path or f"{view_id}.md")
    target.parent.mkdir(parents=True, exist_ok=True)
    name = view_id.replace("-", " ").title()
    target.write_text(
        f"---\nid: {view_id}\nname: {name}\n---\n# {name}\n{body}", encoding="utf-8"
    )
    return target


def registry(*definitions: tuple[str, str]) -> str:
    entries = "".join(f"[{label}]: {destination}\n" for label, destination in definitions)
    return f"\n<!-- arch-doc:references:start -->\n{entries}<!-- arch-doc:references:end -->\n"


def valid_adr_text(identifier: str, title: str) -> str:
    return (
        "---\nstatus: accepted\ndate: 2026-09-04\n---\n\n"
        f"# {identifier}: {title}\n\n"
        "## Context\n\nContext.\n\n"
        "## Decision\n\nDecision.\n\n"
        "## Consequences\n\nConsequences.\n\n"
        "## Alternatives considered\n\nNo alternatives are preserved.\n"
    )


def test_snapshot_indexes_recursive_managed_views_but_ignores_drafts(run_cli) -> None:
    root = write_root(run_cli.repo, "# Architecture\n")
    nested = write_view(run_cli.repo, "nested-view", path="runtime/nested.md")
    draft = run_cli.repo / "docs" / "architecture" / "views" / "draft.md"
    draft.write_text("# Not managed\n", encoding="utf-8")

    snapshot = load_snapshot(run_cli.repo)

    assert isinstance(snapshot, RepositorySnapshot)
    assert snapshot.root is not None and snapshot.root.path == root.resolve()
    assert snapshot.views_by_id["nested-view"].path == nested.resolve()
    assert snapshot.views_by_path[nested.resolve()].view_id == "nested-view"
    assert snapshot.drafts == (draft.resolve(),)


def test_inventory_reports_only_canonical_artifacts_and_managed_edges(run_cli) -> None:
    root = write_root(
        run_cli.repo,
        "# Architecture\n\n[Runtime][runtime]"
        + registry(("fact-authority", "#fact-authority"), ("runtime", "views/runtime.md")),
    )
    write_view(run_cli.repo, "runtime", body=registry(("fact-authority", "../../README.md#fact-authority")))
    root.write_text(
        root.read_text(encoding="utf-8").replace(
            "# Architecture", "# Architecture\n\n<a id=\"fact-authority\"></a>\n## Authority"
        ),
        encoding="utf-8",
    )
    draft = run_cli.repo / "docs" / "architecture" / "views" / "draft.md"
    draft.write_text("# Draft\n", encoding="utf-8")

    result = run_cli("inventory")

    assert result.returncode == 0
    assert result.stderr == ""
    assert result.stdout == (
        "Root: docs/architecture/README.md\n"
        "View: runtime docs/architecture/views/runtime.md\n"
        "Fact: fact-authority #fact-authority\n"
        "Edge: document -> runtime\n"
        "Warning: Fact 'fact-authority' is not consumed outside its own document.\n"
        "Warning: Registry entry 'fact-authority' in 'runtime' is unused.\n"
    )


def test_validate_document_blocks_missing_local_managed_definition(run_cli) -> None:
    write_root(run_cli.repo, "# Architecture\n\n[Missing][missing]\n")

    result = run_cli("validate", "document")

    assert result.returncode == 3
    assert "Managed reference 'missing' has no local definition." in result.stderr
    assert result.stdout == ""


def test_validate_document_reports_integration_gaps_without_blocking(run_cli) -> None:
    write_root(run_cli.repo, "# Architecture\n")
    write_view(run_cli.repo, "orphan")

    result = run_cli("validate", "document")

    assert result.returncode == 0
    assert result.stderr == ""
    assert result.stdout == (
        "Valid.\n"
        "Warning: View 'orphan' is unreachable from document.\n"
        "Warning: View 'orphan' is not consumed outside its own document.\n"
    )


def test_validate_document_allows_a_registered_but_unsummarised_view_as_a_warning(run_cli) -> None:
    write_root(run_cli.repo, "# Architecture\n" + registry(("pending", "views/pending.md")))
    write_view(run_cli.repo, "pending")

    result = run_cli("validate", "document")

    assert result.returncode == 0
    assert result.stderr == ""
    assert "Warning: Registry entry 'pending' in 'document' is unused." in result.stdout


def test_validation_reports_duplicate_ids_malformed_attempts_and_broken_destinations_in_path_order(run_cli) -> None:
    write_root(
        run_cli.repo,
        "# Architecture\n\n[Existing][existing]" + registry(("existing", "views/not-existing.md")),
    )
    write_view(run_cli.repo, "existing", path="a.md")
    write_view(run_cli.repo, "existing", path="z.md")
    malformed = run_cli.repo / "docs" / "architecture" / "views" / "broken.md"
    malformed.write_text("---\nname: Broken\nid: broken\n---\n# Broken\n", encoding="utf-8")

    result = run_cli("validate", "document")

    assert result.returncode == 3
    assert result.stdout == ""
    assert result.stderr.index("broken.md") < result.stderr.index("Duplicate managed view ID 'existing'.")
    assert "Registry destination for 'existing' does not target its managed artifact." in result.stderr


def test_validate_view_reports_duplicate_ids_for_both_id_and_path_targets(run_cli) -> None:
    write_root(run_cli.repo, "# Architecture\n")
    first = write_view(run_cli.repo, "duplicate", path="first.md")
    second = write_view(run_cli.repo, "duplicate", path="nested/second.md")

    by_id = run_cli("validate", "view", "duplicate")
    by_path = run_cli("validate", "view", str(second.relative_to(run_cli.repo)))

    assert first.exists()
    assert by_id.returncode == 3
    assert by_path.returncode == 3
    assert "nested/second.md:2: Duplicate managed view ID 'duplicate'." in by_id.stderr
    assert "nested/second.md:2: Duplicate managed view ID 'duplicate'." in by_path.stderr


def test_validation_diagnostics_identify_the_source_line_of_parse_registry_and_adr_errors(run_cli) -> None:
    write_root(
        run_cli.repo,
        "# Architecture\n\n[Existing][existing]" + registry(("existing", "views/not-existing.md")),
    )
    write_view(run_cli.repo, "existing")
    malformed = run_cli.repo / "docs" / "architecture" / "views" / "broken.md"
    malformed.parent.mkdir(parents=True, exist_ok=True)
    malformed.write_text("---\nname: Broken\nid: broken\n---\n# Broken\n", encoding="utf-8")
    decisions = run_cli.repo / "docs" / "architecture" / "decisions"
    decisions.mkdir(parents=True)
    (decisions / "0001-decide.md").write_text(
        "---\nstatus: proposed\ndate: 2026-09-04\n---\n# 0001: Decide\n", encoding="utf-8"
    )

    document = run_cli("validate", "document")
    adr = run_cli("validate", "adr")

    assert document.returncode == 3
    assert "views/broken.md:2: Invalid view frontmatter key order: expected id then name." in document.stderr
    assert "README.md:5: Registry destination for 'existing' does not target its managed artifact." in document.stderr
    assert adr.returncode == 3
    assert "decisions/0001-decide.md:2: Invalid ADR metadata." in adr.stderr


def test_validation_diagnostics_locate_heading_and_root_fact_registry_problems(run_cli) -> None:
    write_root(
        run_cli.repo,
        "# Architecture\n\n<a id=\"fact-missing\"></a>\n## Missing\n"
        + registry(("fact-orphan", "#fact-orphan")),
    )
    path = run_cli.repo / "docs" / "architecture" / "views" / "heading.md"
    path.parent.mkdir(parents=True)
    path.write_text(
        "---\nid: heading\nname: Expected heading\n---\n# Different heading\n", encoding="utf-8"
    )

    result = run_cli("validate", "document")

    assert result.returncode == 3
    assert "views/heading.md:5: View name and first level-one heading must match." in result.stderr
    assert "README.md:3: Fact anchor 'fact-missing' has no matching root registry definition." in result.stderr
    assert "README.md:7: Fact registry definition 'fact-orphan' has no matching anchor." in result.stderr


def test_validate_adr_marks_duplicate_filename_identifiers_at_the_filename_location(run_cli) -> None:
    decisions = run_cli.repo / "docs" / "architecture" / "decisions"
    decisions.mkdir(parents=True)
    for name in ("0001-first.md", "0001-second.md"):
        (decisions / name).write_text(
            valid_adr_text("0001", "Decision"), encoding="utf-8"
        )

    result = run_cli("validate", "adr")

    assert result.returncode == 3
    assert "decisions/0001-second.md:1: Duplicate ADR identifier '0001' from filename." in result.stderr


def test_view_links_reachability_and_external_consumption_ignore_cycles_and_self_links(run_cli) -> None:
    write_root(run_cli.repo, "# Architecture\n\n[A][a]" + registry(("a", "views/a.md")))
    write_view(run_cli.repo, "a", body="\n[B][b]" + registry(("b", "b.md")))
    write_view(run_cli.repo, "b", body="\n[A][a]" + registry(("a", "a.md")))
    write_view(run_cli.repo, "cycle-one", body="\n[Two][cycle-two]" + registry(("cycle-two", "cycle-two.md")))
    write_view(run_cli.repo, "cycle-two", body="\n[One][cycle-one]" + registry(("cycle-one", "cycle-one.md")))
    write_view(run_cli.repo, "self", body="\n[Self][self]" + registry(("self", "self.md")))

    result = run_cli("validate", "document")

    assert result.returncode == 0
    assert "Warning: View 'a' is unreachable" not in result.stdout
    assert "Warning: View 'b' is unreachable" not in result.stdout
    assert "Warning: View 'cycle-one' is unreachable from document." in result.stdout
    assert "Warning: View 'cycle-two' is unreachable from document." in result.stdout
    assert "Warning: View 'self' is unreachable from document." in result.stdout
    assert "Warning: View 'self' is not consumed outside its own document." in result.stdout


def test_validate_document_rejects_duplicate_fact_anchors_and_fact_namespace_view_ids(run_cli) -> None:
    write_root(
        run_cli.repo,
        "# Architecture\n\n<a id=\"fact-shared\"></a>\n## Shared\n"
        "<a id=\"fact-shared\"></a>\n## Again\n"
        + registry(("fact-shared", "#fact-shared")),
    )
    path = run_cli.repo / "docs" / "architecture" / "views" / "bad.md"
    path.parent.mkdir(parents=True)
    path.write_text("---\nid: fact-bad\nname: Bad\n---\n# Bad\n", encoding="utf-8")

    result = run_cli("validate", "document")

    assert result.returncode == 3
    assert "Duplicate fact anchor: fact-shared." in result.stderr
    assert "Invalid view ID." in result.stderr


def test_validate_view_only_requires_its_target_and_validate_adr_checks_numeric_records(run_cli) -> None:
    write_root(run_cli.repo, "# Architecture\n")
    write_view(run_cli.repo, "target")
    decisions = run_cli.repo / "docs" / "architecture" / "decisions"
    decisions.mkdir(parents=True)
    (decisions / "0001-decide.md").write_text(
        valid_adr_text("0001", "Decide"), encoding="utf-8"
    )

    assert run_cli("validate", "view", "target").stdout.startswith("Valid.\n")
    adr_result = run_cli("validate", "adr")
    assert adr_result.returncode == 0
    assert adr_result.stdout == "Valid.\n"