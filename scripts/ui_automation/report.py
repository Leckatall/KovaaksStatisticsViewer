"""Contact sheets for driver screenshots: `ui_driver.py report` and `tour --report`.

Reads a shots directory and its index.jsonl (one line per shot, written by the
CLI) and writes one HTML page: an optional run summary, an optional coverage
table (coverage.json: [{"area", "state", "shot"}], where "shot" is a shot label),
then the shots grouped by their shots/ subdirectory, captioned with their labels
and any app warnings. Images are embedded as data URIs unless `embed=False`, so
the page works offline on its own.
"""

from __future__ import annotations

import base64
from dataclasses import dataclass, field
import html
import json
from pathlib import Path
import re
from typing import Any


@dataclass
class Shot:
    path: Path
    relative: str
    label: str
    group: str
    window: str = ""
    size: list[int] = field(default_factory=list)
    command: str = ""
    warnings: list[str] = field(default_factory=list)


_NUMBERED = re.compile(r"^(\d+)-(.*)\.png$")


def load_shots(directory: Path) -> list[Shot]:
    """Shots from index.jsonl, in the order taken; or every numbered PNG when there is no index."""
    index = directory / "index.jsonl"
    shots: list[Shot] = []
    if index.is_file():
        for line in index.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            entry = json.loads(line)
            path = directory / entry["path"]
            if not path.is_file():
                continue
            shots.append(Shot(path=path, relative=entry["path"], label=entry.get("label") or path.stem,
                              group=entry.get("dir") or Path(entry["path"]).parent.as_posix().strip("."),
                              window=entry.get("window") or "", size=entry.get("size") or [],
                              command=entry.get("command") or "", warnings=entry.get("warnings") or []))
        return shots
    for path in sorted(directory.rglob("*.png"), key=lambda item: (_number(item), item.as_posix())):
        relative = path.relative_to(directory).as_posix()
        match = _NUMBERED.match(path.name)
        shots.append(Shot(path=path, relative=relative, label=match.group(2) if match else path.stem,
                          group=path.parent.relative_to(directory).as_posix().strip(".")))
    return shots


def _number(path: Path) -> int:
    match = _NUMBERED.match(path.name)
    return int(match.group(1)) if match else 0


def load_coverage(path: Path | None) -> list[dict[str, str]]:
    if not path or not path.is_file():
        return []
    entries = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(entries, list):
        raise ValueError(f"{path}: expected a JSON array of {{area, state, shot}}")
    for number, entry in enumerate(entries):
        if not isinstance(entry, dict) or not {"area", "state", "shot"} <= set(entry):
            raise ValueError(f"{path}[{number}]: expected {{area, state, shot}}")
    return entries


def _caption(label: str) -> str:
    return label.replace("-", " ")


def _image_source(shot: Shot, output: Path, embed: bool) -> str:
    if embed:
        return "data:image/png;base64," + base64.b64encode(shot.path.read_bytes()).decode("ascii")
    try:
        return shot.path.resolve().relative_to(output.parent.resolve()).as_posix()
    except ValueError:
        return shot.path.resolve().as_uri()


_STYLE = """
:root{--bg:#16181b;--fg:#e6e6e6;--muted:#9aa0a6;--card:#202327;--ok:#5ec27a;--bad:#e8a33d;--bug:#ef6b6b}
body{background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,Segoe UI,sans-serif;margin:0;padding:24px 16px}
h1{margin:0 0 4px} h2{margin:28px 0 10px;font-size:16px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:14px}
figure{margin:0;background:var(--card);border-radius:8px;overflow:hidden} img{width:100%;display:block;cursor:zoom-in}
figcaption{padding:8px 10px;font-size:13px} .note{display:block;color:var(--muted);font-size:12px;margin-top:3px;word-break:break-word}
.note.warning{color:var(--bad)} .bad{color:var(--bug)}
table{border-collapse:collapse;width:100%;max-width:1100px;font-size:13px}
td,th{border-bottom:1px solid #2c3036;padding:6px 8px;text-align:left;vertical-align:top}
.captured{color:var(--ok)} .missing{color:var(--bug)} .wrap{overflow-x:auto} code{color:var(--muted)}
#zoom{display:none;position:fixed;inset:0;background:rgba(0,0,0,.85);align-items:center;justify-content:center;cursor:zoom-out}
#zoom img{max-width:96vw;max-height:96vh;width:auto;cursor:zoom-out}
"""

_SCRIPT = """
const zoom=document.getElementById('zoom'),big=zoom.querySelector('img');
document.querySelectorAll('figure img').forEach(i=>i.onclick=()=>{big.src=i.src;zoom.style.display='flex'});
zoom.onclick=()=>{zoom.style.display='none';big.src=''};
document.onkeydown=e=>{if(e.key==='Escape')zoom.onclick()};
"""


def build_report(shots: list[Shot], *, output: Path, title: str, coverage: list[dict[str, str]],
                 summary: dict[str, Any] | None = None, embed: bool = True) -> str:
    esc = html.escape
    labels = {shot.label for shot in shots}
    warned = sum(1 for shot in shots if shot.warnings)
    parts = [
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">",
        f"<title>{esc(title)}</title><style>{_STYLE}</style></head><body>",
        f"<h1>{esc(title)}</h1>",
    ]
    facts = [f"{len(shots)} screenshot(s)"]
    if coverage:
        captured = sum(1 for entry in coverage if entry["shot"] in labels)
        facts.append(f"coverage {captured}/{len(coverage)} captured")
    if warned:
        facts.append(f"{warned} shot(s) with app warnings")
    if summary:
        facts.append(f"{summary.get('steps', 0)} step(s), {summary.get('failed', 0)} failed")
        if "seconds" in summary:
            facts.append(f"{summary['seconds']}s")
    parts.append(f"<p style=\"color:var(--muted)\">{esc(', '.join(facts))}</p>")

    if summary and summary.get("failures"):
        parts.append("<h2>Failed steps</h2><div class=\"wrap\"><table><tr><th>Line</th><th>Step</th><th>Error</th></tr>")
        for failure in summary["failures"]:
            parts.append(f"<tr><td>{failure.get('line', '')}</td><td><code>{esc(str(failure.get('step', '')))}</code></td>"
                         f"<td class=\"bad\">{esc(str(failure.get('error', '')))}</td></tr>")
        parts.append("</table></div>")

    if coverage:
        parts.append("<h2>Coverage</h2><div class=\"wrap\"><table><tr><th>Area</th><th>State</th><th>Status</th>"
                     "<th>Shot</th></tr>")
        for entry in coverage:
            status = "captured" if entry["shot"] in labels else "missing"
            parts.append(f"<tr><td>{esc(entry['area'])}</td><td>{esc(entry['state'])}</td>"
                         f"<td class=\"{status}\">{status}</td><td><code>{esc(entry['shot'])}</code></td></tr>")
        parts.append("</table></div>")

    groups: dict[str, list[Shot]] = {}
    for shot in shots:
        groups.setdefault(shot.group, []).append(shot)
    for group, members in groups.items():
        parts.append(f"<h2>{esc(group or 'shots')}</h2><div class=\"grid\">")
        for shot in members:
            notes = []
            details = " ".join(item for item in (
                f"window {shot.window}" if shot.window and shot.window != "main" else "",
                f"{shot.size[0]}x{shot.size[1]}" if len(shot.size) == 2 else "") if item)
            if details:
                notes.append(f"<span class=\"note\">{esc(details)}</span>")
            if shot.command:
                notes.append(f"<span class=\"note\"><code>{esc(shot.command)}</code></span>")
            notes += [f"<span class=\"note warning\">{esc(warning)}</span>" for warning in shot.warnings]
            parts.append(f"<figure><img src=\"{_image_source(shot, output, embed)}\" loading=\"lazy\" "
                         f"alt=\"{esc(shot.label)}\"><figcaption>{esc(_caption(shot.label))}{''.join(notes)}"
                         "</figcaption></figure>")
        parts.append("</div>")
    parts.append(f"<div id=\"zoom\"><img alt=\"\"></div><script>{_SCRIPT}</script></body></html>")
    return "".join(parts)


def write_report(directory: Path, output: Path, *, title: str, coverage: Path | None = None,
                 summary: dict[str, Any] | None = None, embed: bool = True) -> Path:
    page = build_report(load_shots(directory), output=output, title=title, coverage=load_coverage(coverage),
                        summary=summary, embed=embed)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(page, encoding="utf-8")
    return output.resolve()
