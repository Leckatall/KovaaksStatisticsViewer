from __future__ import annotations

from pathlib import Path
import re


def normalize_line(line: str, repo_root: Path) -> str:
    """Forward-slash the separators and strip the repository root, which the report states once."""
    normalized = line.replace("\\", "/")
    root = repo_root.resolve().as_posix().rstrip("/")
    normalized = re.sub(re.escape(root) + r"/", "", normalized, flags=re.IGNORECASE)
    normalized = re.sub(re.escape(root), ".", normalized, flags=re.IGNORECASE)
    return normalized.rstrip()


def normalize(text: str, repo_root: Path) -> str:
    return "\n".join(normalize_line(line, repo_root) for line in text.split("\n"))
