from __future__ import annotations

from pathlib import Path

from scripts import arch_doc


def write_root(repo: Path, text: str = "# Architecture\n") -> Path:
    path = repo / "docs" / "architecture" / "README.md"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="")
    return path


def write_view(repo: Path, view_id: str, *, path: str | None = None, body: str = "") -> Path:
    target = repo / "docs" / "architecture" / "views" / (path or f"{view_id}.md")
    target.parent.mkdir(parents=True, exist_ok=True)
    name = view_id.replace("-", " ").title()
    target.write_text(
        f"---\nid: {view_id}\nname: {name}\n---\n# {name}\n{body}", encoding="utf-8", newline=""
    )
    return target


def registry(*definitions: tuple[str, str]) -> str:
    entries = "".join(f"[{label}]: {destination}\n" for label, destination in definitions)
    return f"\n<!-- arch-doc:references:start -->\n{entries}<!-- arch-doc:references:end -->\n"


def tree_bytes(repo: Path) -> dict[Path, bytes]:
    return {path.relative_to(repo): path.read_bytes() for path in repo.rglob("*") if path.is_file()}


def test_destination_and_registry_helpers_use_relative_posix_paths_and_exact_delimiters(tmp_path: Path) -> None:
    """Catches platform paths or registry order leaking into generated Markdown."""
    document = tmp_path / "docs" / "architecture" / "views" / "runtime" / "live.md"
    target = tmp_path / "docs" / "architecture" / "README.md"

    assert hasattr(arch_doc, "destination_for")
    assert hasattr(arch_doc, "render_registry")
    assert arch_doc.destination_for(document, target, "fact-profile-authority") == "../../README.md#fact-profile-authority"
    assert arch_doc.render_registry({"zebra": "z.md", "alpha": "a.md"}) == (
        "<!-- arch-doc:references:start -->\n"
        "[alpha]: a.md\n"
        "[zebra]: z.md\n"
        "<!-- arch-doc:references:end -->"
    )


def test_references_add_creates_a_final_sorted_registry_and_keeps_a_deliberately_unused_registration(run_cli) -> None:
    """Catches add inserting a registry before prose or requiring an existing managed use."""
    root = write_root(run_cli.repo, "# Architecture\n\nClosing prose.\n")
    write_view(run_cli.repo, "zebra", path="nested/zebra.md")
    write_view(run_cli.repo, "alpha", path="alpha.md")

    first = run_cli("references", "add", "document", "zebra")
    second = run_cli("references", "add", "document", "alpha")

    assert first.returncode == second.returncode == 0
    assert first.stdout == "Added reference 'zebra' to document.\n"
    assert second.stdout == "Added reference 'alpha' to document.\n"
    assert root.read_text(encoding="utf-8", newline="") == (
        "# Architecture\n\nClosing prose.\n\n"
        "<!-- arch-doc:references:start -->\n"
        "[alpha]: views/alpha.md\n"
        "[zebra]: views/nested/zebra.md\n"
        "<!-- arch-doc:references:end -->\n"
    )


def test_references_add_to_a_recursive_view_rebases_the_destination(run_cli) -> None:
    """Catches calculating generated paths from the repository instead of the containing document."""
    write_root(run_cli.repo)
    consumer = write_view(run_cli.repo, "consumer", path="nested/deeper/consumer.md")
    write_view(run_cli.repo, "target", path="other/target.md")

    result = run_cli("references", "add", "consumer", "target")

    assert result.returncode == 0
    assert "[target]: ../../other/target.md" in consumer.read_text(encoding="utf-8")


def test_references_update_adds_missing_use_definitions_corrects_destinations_and_preserves_unused_entries(run_cli) -> None:
    """Catches update rewriting prose, deleting an intentional registration, or retaining stale paths."""
    write_root(run_cli.repo)
    consumer = write_view(
        run_cli.repo,
        "consumer",
        path="nested/consumer.md",
        body="\n[Actual use][target]\n\nSummary prose.\n"
        + registry(("stale", "wrong.md"), ("unused", "../../unused.md")),
    )
    write_view(run_cli.repo, "target", path="target.md")
    write_view(run_cli.repo, "stale", path="deeper/stale.md")
    write_view(run_cli.repo, "unused", path="unused.md")
    before = consumer.read_text(encoding="utf-8")

    result = run_cli("references", "update", "nested/consumer.md")

    assert result.returncode == 0
    assert result.stdout == "Updated references in consumer.\n"
    after = consumer.read_text(encoding="utf-8")
    assert after.startswith(before[: before.index("\n<!-- arch-doc:references:start -->")])
    assert "[stale]: ../deeper/stale.md" in after
    assert "[target]: ../target.md" in after
    assert "[unused]: ../unused.md" in after
    rendered_registry = after[after.index("<!-- arch-doc:references:start -->") :]
    assert rendered_registry.index("[stale]") < rendered_registry.index("[target]") < rendered_registry.index("[unused]")


def test_references_update_all_documents_rebases_recursively_and_preflights_every_payload(run_cli) -> None:
    """Catches all-document update writing an early registry before a later document is invalid."""
    root = write_root(run_cli.repo, "# Architecture\n\n[One][one]" + registry(("one", "wrong.md")))
    first = write_view(run_cli.repo, "one", path="nested/one.md", body="\n[Two][two]" + registry(("two", "wrong.md")))
    second = write_view(run_cli.repo, "two", path="deep/two.md", body="\n[Missing][missing]\n")
    before = tree_bytes(run_cli.repo)

    refused = run_cli("references", "update")

    assert refused.returncode == 5
    assert tree_bytes(run_cli.repo) == before

    second.write_text("---\nid: two\nname: Two\n---\n# Two\n", encoding="utf-8")
    updated = run_cli("references", "update")

    assert updated.returncode == 0
    assert updated.stdout == "Updated references in 3 documents.\n"
    assert "[one]: views/nested/one.md" in root.read_text(encoding="utf-8")
    assert "[two]: ../deep/two.md" in first.read_text(encoding="utf-8")


def test_references_remove_refuses_a_current_use_then_removes_the_last_registry_block(run_cli) -> None:
    """Catches removing a still-required definition or leaving empty delimiters behind."""
    root = write_root(run_cli.repo, "# Architecture\n\n[Used][used]" + registry(("used", "views/used.md")))
    write_view(run_cli.repo, "used")
    before = root.read_bytes()

    refused = run_cli("references", "remove", "document", "used")

    assert refused.returncode == 5
    assert root.read_bytes() == before

    root.write_text("# Architecture\n" + registry(("used", "views/used.md")), encoding="utf-8")
    removed = run_cli("references", "remove", "document", "used")

    assert removed.returncode == 0
    assert removed.stdout == "Removed reference 'used' from document.\n"
    assert root.read_text(encoding="utf-8") == "# Architecture\n"


def test_fact_consumers_are_sorted_and_fact_references_rebase_to_root(run_cli) -> None:
    """Catches facts being treated as views or consumer output depending on discovery order."""
    root = write_root(
        run_cli.repo,
        "# Architecture\n\n<a id=\"fact-profile-authority\"></a>\n## Profile authority\n"
        + registry(("fact-profile-authority", "#fact-profile-authority")),
    )
    alpha = write_view(run_cli.repo, "alpha", path="alpha.md", body="\n[Authority][fact-profile-authority]\n")
    zulu = write_view(run_cli.repo, "zulu", path="nested/zulu.md")

    add = run_cli("references", "add", "zulu", "fact-profile-authority")
    consumers = run_cli("fact", "consumers", "fact-profile-authority")

    assert root.exists()
    assert add.returncode == 0
    assert "[fact-profile-authority]: ../../README.md#fact-profile-authority" in zulu.read_text(encoding="utf-8")
    assert consumers.returncode == 0
    assert consumers.stdout == "docs/architecture/views/alpha.md\n"
    assert alpha.exists()


def test_fact_registration_replacement_waits_for_all_consumers_and_never_edits_fact_prose(run_cli) -> None:
    """Catches removing an old fact identity before every consumer has moved to its replacement."""
    root = write_root(
        run_cli.repo,
        "# Architecture\n\n<a id=\"fact-old\"></a>\n## Old\n\n"
        "<a id=\"fact-new\"></a>\n## New\n"
        + registry(("fact-new", "#fact-new"), ("fact-old", "#fact-old")),
    )
    consumer = write_view(
        run_cli.repo,
        "consumer",
        body="\n[Old fact][fact-old]" + registry(("fact-old", "../../README.md#fact-old")),
    )

    refused = run_cli("references", "remove", "document", "fact-old")

    assert refused.returncode == 5
    assert "fact-old" in root.read_text(encoding="utf-8")

    consumer.write_text(
        consumer.read_text(encoding="utf-8").replace("fact-old", "fact-new"), encoding="utf-8"
    )
    root.write_text(
        root.read_text(encoding="utf-8").replace('<a id="fact-old"></a>\n## Old\n\n', ""), encoding="utf-8"
    )
    removed = run_cli("references", "remove", "document", "fact-old")

    assert removed.returncode == 0
    assert "<a id=\"fact-new\"></a>\n## New" in root.read_text(encoding="utf-8")
    assert "[fact-old]:" not in root.read_text(encoding="utf-8")


def test_fact_commands_reject_missing_or_non_heading_anchors_without_writing(run_cli) -> None:
    """Catches treating arbitrary HTML anchors as stable root facts."""
    root = write_root(
        run_cli.repo,
        "# Architecture\n\n<a id=\"fact-not-a-heading\"></a>\nordinary prose\n"
        + registry(("fact-not-a-heading", "#fact-not-a-heading")),
    )
    write_view(run_cli.repo, "consumer")
    before = tree_bytes(run_cli.repo)

    missing = run_cli("fact", "consumers", "fact-missing")
    malformed = run_cli("references", "add", "consumer", "fact-not-a-heading")

    assert missing.returncode == 4
    assert malformed.returncode == 4
    assert tree_bytes(run_cli.repo) == before
    assert root.exists()


def test_fact_consumers_rejects_duplicate_anchors(run_cli) -> None:
    """Catches selecting the first of two incompatible root fact identities."""
    write_root(
        run_cli.repo,
        "# Architecture\n\n<a id=\"fact-shared\"></a>\n## First\n\n"
        "<a id=\"fact-shared\"></a>\n## Second\n"
        + registry(("fact-shared", "#fact-shared")),
    )
    before = tree_bytes(run_cli.repo)

    result = run_cli("fact", "consumers", "fact-shared")

    assert result.returncode == 3
    assert "Duplicate fact anchor: fact-shared." in result.stderr
    assert tree_bytes(run_cli.repo) == before


def test_validate_document_rejects_a_non_lexical_registry_at_the_first_out_of_order_definition(run_cli) -> None:
    """Catches silently accepting a hand-edited registry whose stable IDs are not canonical."""
    write_root(
        run_cli.repo,
        "# Architecture\n"
        + registry(("zebra", "views/zebra.md"), ("alpha", "views/alpha.md")),
    )
    write_view(run_cli.repo, "zebra")
    write_view(run_cli.repo, "alpha")

    result = run_cli("validate", "document")

    assert result.returncode == 3
    assert result.stdout == ""
    assert result.stderr == (
        "docs/architecture/README.md:5: Registry definitions must be lexically ordered.\n"
    )


def test_references_add_rejects_an_anchored_but_unregistered_root_fact_without_writing(run_cli) -> None:
    """Catches add bootstrapping a root fact identity that the semantic skill must author directly."""
    root = write_root(
        run_cli.repo,
        "# Architecture\n\n<a id=\"fact-profile-authority\"></a>\n## Profile authority\n",
    )
    before = tree_bytes(run_cli.repo)

    result = run_cli("references", "add", "document", "fact-profile-authority")

    assert result.returncode == 4
    assert result.stdout == ""
    assert result.stderr == "Fact target not found: fact-profile-authority\n"
    assert tree_bytes(run_cli.repo) == before
    assert root.exists()


def test_crlf_reference_mutations_preserve_unrelated_bytes_and_crlf_line_endings(run_cli) -> None:
    """Catches registry rewrites normalizing CRLF or touching prose outside the generated final block."""
    root = run_cli.repo / "docs" / "architecture" / "README.md"
    root.parent.mkdir(parents=True, exist_ok=True)
    root.write_bytes("# Architecture\r\n\r\nπ stays byte-for-byte.".encode("utf-8"))
    write_view(run_cli.repo, "target")
    prose = "# Architecture\r\n\r\nπ stays byte-for-byte.".encode("utf-8")

    added = run_cli("references", "add", "document", "target")

    assert added.returncode == 0
    add_bytes = root.read_bytes()
    assert add_bytes.startswith(prose)
    assert b"\n" not in add_bytes.replace(b"\r\n", b"")

    stale_bytes = add_bytes.replace(b"[target]: views/target.md", b"[target]: wrong.md")
    root.write_bytes(stale_bytes)
    before_update_prefix = stale_bytes[: stale_bytes.index(b"<!-- arch-doc:references:start -->")]
    updated = run_cli("references", "update", "document")

    assert updated.returncode == 0
    update_bytes = root.read_bytes()
    assert update_bytes.startswith(before_update_prefix)
    assert b"[target]: views/target.md" in update_bytes
    assert b"\n" not in update_bytes.replace(b"\r\n", b"")

    removed = run_cli("references", "remove", "document", "target")

    assert removed.returncode == 0
    assert root.read_bytes().startswith(prose)
    assert b"<!-- arch-doc:references:" not in root.read_bytes()
    assert b"\n" not in root.read_bytes().replace(b"\r\n", b"")