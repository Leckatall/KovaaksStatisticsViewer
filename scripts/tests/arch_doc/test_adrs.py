from __future__ import annotations

from pathlib import Path

import pytest


def adr_body(newline: str = "\n") -> str:
    return newline.join(
        (
            "## Context",
            "",
            "A durable constraint exists.",
            "",
            "## Decision",
            "",
            "Use the accepted design.",
            "",
            "## Consequences",
            "",
            "The constraint becomes explicit.",
            "",
            "## Alternatives considered",
            "",
            "No other alternative is preserved by the evidence.",
            "",
        )
    )


def write_body(repo: Path, text: str | None = None) -> Path:
    path = repo / "proposal.md"
    path.write_text(text if text is not None else adr_body(), encoding="utf-8", newline="")
    return path


def write_adr(
    repo: Path,
    identifier: str,
    slug: str,
    *,
    status: str = "accepted",
    extra: str = "",
    newline: str = "\n",
) -> Path:
    path = repo / "docs" / "architecture" / "decisions" / f"{identifier}-{slug}.md"
    path.parent.mkdir(parents=True, exist_ok=True)
    metadata = f"status: {status}{newline}date: 2026-09-01{newline}{extra}"
    payload = (
        f"---{newline}{metadata}---{newline}{newline}"
        f"# {identifier}: Existing choice{newline}{newline}{adr_body(newline)}"
    )
    path.write_text(payload, encoding="utf-8", newline="")
    return path


def tree_bytes(repo: Path) -> dict[str, bytes]:
    return {
        path.relative_to(repo).as_posix(): path.read_bytes()
        for path in sorted(repo.rglob("*"))
        if path.is_file()
    }


def test_adr_record_allocates_0001_and_writes_canonical_accepted_record(run_cli) -> None:
    """Catches numbering from anything except filenames and noncanonical initial metadata/body layout."""
    body = write_body(run_cli.repo)

    result = run_cli(
        "adr",
        "record",
        str(body),
        "--title",
        "Choose the profile store",
        "--slug",
        "choose-profile-store",
        "--date",
        "2026-09-04",
    )

    expected_path = run_cli.repo / "docs" / "architecture" / "decisions" / "0001-choose-profile-store.md"
    assert result.returncode == 0
    assert result.stderr == ""
    assert result.stdout == "docs/architecture/decisions/0001-choose-profile-store.md\n"
    assert expected_path.read_text(encoding="utf-8") == (
        "---\n"
        "status: accepted\n"
        "date: 2026-09-04\n"
        "---\n\n"
        "# 0001: Choose the profile store\n\n"
        + adr_body()
    )
    assert body.read_text(encoding="utf-8") == adr_body()


def test_adr_record_uses_highest_numeric_prefix_plus_one_and_ignores_other_files(run_cli) -> None:
    """Catches counting files or filling gaps instead of allocating from the highest numeric prefix."""
    write_adr(run_cli.repo, "0002", "older")
    write_adr(run_cli.repo, "0007", "latest")
    decisions = run_cli.repo / "docs" / "architecture" / "decisions"
    (decisions / "notes.md").write_text("not an ADR", encoding="utf-8")
    body = write_body(run_cli.repo)

    result = run_cli(
        "adr", "record", str(body), "--title", "Next", "--slug", "next", "--date", "2026-09-04"
    )

    assert result.returncode == 0
    assert result.stdout == "docs/architecture/decisions/0008-next.md\n"


@pytest.mark.parametrize(
    ("arguments", "body_text", "diagnostic"),
    (
        (("--title", "", "--slug", "valid", "--date", "2026-09-04"), None, "Refused: invalid ADR title\n"),
        (("--title", "Title", "--slug", "Not-Valid", "--date", "2026-09-04"), None, "Refused: invalid ADR slug: Not-Valid\n"),
        (("--title", "Title", "--slug", "valid", "--date", "2026-02-30"), None, "Refused: invalid ADR date: 2026-02-30\n"),
        (("--title", "Title", "--slug", "valid", "--date", "2026-09-04"), "---\nstatus: draft\n---\n" + adr_body(), "Refused: ADR body must be frontmatter-free\n"),
        (("--title", "Title", "--slug", "valid", "--date", "2026-09-04"), adr_body().replace("## Decision", "## Alternatives considered", 1), "Refused: ADR body must contain Context, Decision, Consequences, Alternatives considered, and optional final Links headings in order\n"),
    ),
)
def test_adr_record_refuses_invalid_inputs_without_writing(run_cli, arguments, body_text, diagnostic) -> None:
    """Catches accepting malformed confirmed inputs or partially creating an ADR before validation finishes."""
    body = write_body(run_cli.repo, body_text)
    before = tree_bytes(run_cli.repo)

    result = run_cli("adr", "record", str(body), *arguments)

    assert result.returncode == 5
    assert result.stdout == ""
    assert result.stderr == diagnostic
    assert tree_bytes(run_cli.repo) == before


def test_validate_adr_rejects_duplicate_numeric_prefixes_even_when_slugs_differ(run_cli) -> None:
    """Catches ambiguous ADR identity being hidden by a filename-keyed snapshot."""
    write_adr(run_cli.repo, "0001", "first")
    write_adr(run_cli.repo, "0001", "second")

    result = run_cli("validate", "adr")

    assert result.returncode == 3
    assert result.stdout == ""
    assert result.stderr == (
        "docs/architecture/decisions/0001-second.md:1: Duplicate ADR identifier '0001' from filename.\n"
    )


def test_adr_record_writes_retrospective_metadata_in_canonical_order(run_cli) -> None:
    """Catches retrospective status being confused with the recording date or accepted lifecycle."""
    body = write_body(run_cli.repo)

    result = run_cli(
        "adr", "record", str(body), "--title", "Existing choice", "--slug", "existing-choice",
        "--date", "2026-09-04", "--retrospective"
    )

    assert result.returncode == 0
    created = run_cli.repo / "docs" / "architecture" / "decisions" / "0001-existing-choice.md"
    assert created.read_bytes().startswith(
        b"---\nstatus: accepted\ndate: 2026-09-04\nretrospective: true\n---\n"
    )


def test_adr_record_supersedes_multiple_records_reciprocally_and_preserves_old_bodies(run_cli) -> None:
    """Catches one-sided supersession or rewriting historical ADR body bytes."""
    first = write_adr(run_cli.repo, "0001", "first", newline="\r\n")
    second = write_adr(run_cli.repo, "0002", "second", extra="retrospective: true\n")
    first_before = first.read_bytes()
    second_before = second.read_bytes()
    first_body = first_before[first_before.index(b"# 0001:") :]
    second_body = second_before[second_before.index(b"# 0002:") :]
    proposal = write_body(run_cli.repo)

    result = run_cli(
        "adr", "record", str(proposal), "--title", "Replacement", "--slug", "replacement",
        "--date", "2026-09-04", "--supersedes", "0001", "--supersedes", "0002"
    )

    assert result.returncode == 0
    created = run_cli.repo / "docs" / "architecture" / "decisions" / "0003-replacement.md"
    assert created.read_bytes().startswith(
        b"---\nstatus: accepted\ndate: 2026-09-04\nsupersedes:\n  - '0001'\n  - '0002'\n---\n"
    )
    assert first.read_bytes().startswith(
        b"---\r\nstatus: superseded\r\ndate: 2026-09-01\r\nsuperseded-by: '0003'\r\n---\r\n"
    )
    assert second.read_bytes().startswith(
        b"---\nstatus: superseded\ndate: 2026-09-01\nretrospective: true\nsuperseded-by: '0003'\n---\n"
    )
    assert first.read_bytes().endswith(first_body)
    assert second.read_bytes().endswith(second_body)


@pytest.mark.parametrize(
    ("target", "existing_status", "extra", "returncode", "diagnostic"),
    (
        ("9999", None, "", 4, "ADR target not found: 9999\n"),
        ("0001", "superseded", "superseded-by: '0002'\n", 5, "Refused: ADR '0001' is already superseded\n"),
    ),
)
def test_adr_record_rejects_missing_or_already_superseded_targets_without_writing(
    run_cli, target, existing_status, extra, returncode, diagnostic
) -> None:
    """Catches partial writes when the requested supersession set is not eligible."""
    if existing_status is not None:
        write_adr(run_cli.repo, "0001", "old", status=existing_status, extra=extra)
        write_adr(run_cli.repo, "0002", "replacement", extra="supersedes:\n  - '0001'\n")
    body = write_body(run_cli.repo)
    before = tree_bytes(run_cli.repo)

    result = run_cli(
        "adr", "record", str(body), "--title", "New", "--slug", "new", "--date", "2026-09-04",
        "--supersedes", target
    )

    assert result.returncode == returncode
    assert result.stdout == ""
    assert result.stderr == diagnostic
    assert tree_bytes(run_cli.repo) == before


def test_validate_adr_rejects_nonreciprocal_relationships_and_unknown_metadata(run_cli) -> None:
    """Catches treating individually parseable relationship fields as a coherent decision log."""
    write_adr(run_cli.repo, "0001", "old", status="superseded", extra="superseded-by: '0002'\n")
    write_adr(run_cli.repo, "0002", "new", extra="owner: nobody\n")

    result = run_cli("validate", "adr")

    assert result.returncode == 3
    assert result.stdout == ""
    assert "Invalid ADR metadata key: owner." in result.stderr


