from __future__ import annotations

import subprocess
import sys

import pytest

from conftest import CliResult
from scripts.arch_doc import build_parser


def assert_usage_error(result: CliResult) -> None:
    assert result.returncode == 2
    assert result.stdout == ""
    assert result.stderr.startswith("error: ")
    assert "Traceback" not in result.stderr


def test_no_arguments_prints_a_direct_usage_diagnostic(run_cli) -> None:
    assert_usage_error(run_cli())


def test_malformed_command_prints_a_direct_usage_diagnostic(run_cli) -> None:
    assert_usage_error(run_cli("view", "unknown"))


def test_inventory_uses_the_script_repository_instead_of_the_caller_directory(run_cli) -> None:
    first_view = run_cli.repo / "docs" / "architecture" / "views" / "first.md"
    first_view.parent.mkdir(parents=True)
    first_view.write_text("---\nid: first\nname: First\n---\n# First\n", encoding="utf-8")
    second_view = run_cli.repo / "docs" / "architecture" / "views" / "second.md"
    second_view.write_text("---\nid: second\nname: Second\n---\n# Second\n", encoding="utf-8")

    result = run_cli("inventory")

    assert result.returncode == 0
    assert result.stdout == (
        "Root: missing\n"
        "View: first docs/architecture/views/first.md\n"
        "View: second docs/architecture/views/second.md\n"
        "Warning: View 'first' is unreachable from document.\n"
        "Warning: View 'first' is not consumed outside its own document.\n"
        "Warning: View 'second' is unreachable from document.\n"
        "Warning: View 'second' is not consumed outside its own document.\n"
    )
    assert result.stderr == ""


def test_inventory_ignores_frontmatter_free_view_drafts(run_cli) -> None:
    draft = run_cli.repo / "docs" / "architecture" / "views" / "draft.md"
    draft.parent.mkdir(parents=True)
    draft.write_text("# Draft\n", encoding="utf-8")

    result = run_cli("inventory")

    assert result.returncode == 0
    assert result.stdout == "Root: missing\n"


@pytest.mark.parametrize(
    "arguments",
    (
        ("inventory",),
        ("validate", "document"),
        ("validate", "view", "view-id"),
        ("validate", "adr"),
        ("view", "resolve", "view-id"),
        ("view", "add", "draft.md", "--id", "view-id", "--name", "View", "--register-in", "document"),
        ("view", "migrate-id", "view-id", "new-id"),
        ("view", "retire", "view-id"),
        ("references", "add", "document", "view-id"),
        ("references", "remove", "document", "view-id"),
        ("references", "update"),
        ("fact", "consumers", "fact-profile"),
        ("adr", "record", "body.md", "--title", "Title", "--slug", "title", "--date", "2026-09-04"),
    ),
)
def test_every_public_command_form_routes_to_a_handler(arguments) -> None:
    """Catches a documented public command disappearing from or drifting within argparse."""
    parsed = build_parser().parse_args(arguments)

    assert callable(parsed.handler)


def test_unexpected_handler_failure_remains_an_uncaught_runtime_error(tmp_path, script_source) -> None:
    """Catches main collapsing an unexpected Python failure into an expected application exit code."""
    wrapper = tmp_path / "unexpected_failure.py"
    wrapper.write_text(
        "import sys\n"
        f"sys.path.insert(0, {str(script_source.parents[1])!r})\n"
        "from scripts import arch_doc\n"
        "def boom(args, root):\n"
        "    raise RuntimeError('boom')\n"
        "arch_doc.dispatch = boom\n"
        "raise SystemExit(arch_doc.main(['inventory']))\n",
        encoding="utf-8",
    )

    result = subprocess.run(
        [sys.executable, str(wrapper)],
        cwd=script_source.parents[1],
        text=True,
        capture_output=True,
        check=False,
    )

    assert result.returncode == 1
    assert result.stdout == ""
    assert "Traceback" in result.stderr
    assert result.stderr.rstrip().endswith("RuntimeError: boom")