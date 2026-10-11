"""Compact, agent-friendly text rendering of ksv_driver results."""

from __future__ import annotations

import json
from typing import Any


def describe(record: dict[str, Any]) -> str:
    """One line per item: Type#objectName id:x "text" @x,y wxh [flags]."""
    parts = [record.get("type", "?") + (f"#{record['name']}" if record.get("name") else "")]
    if record.get("id"):
        parts.append(f"id:{record['id']}")
    if record.get("text"):
        parts.append(json.dumps(record["text"], ensure_ascii=False))
    if record.get("placeholder"):
        parts.append(f"placeholder:{json.dumps(record['placeholder'], ensure_ascii=False)}")
    if record.get("acc"):
        parts.append(f"acc:{json.dumps(record['acc'], ensure_ascii=False)}")
    if record.get("kind") == "object":
        parts.append("(non-visual)")
    rect = record.get("rect")
    if rect and len(rect) == 4:
        parts.append(f"@{rect[0]},{rect[1]} {rect[2]}x{rect[3]}")
    flags = []
    if record.get("visible") is False:
        flags.append("hidden")
    if record.get("enabled") is False:
        flags.append("disabled")
    if record.get("focus"):
        flags.append("focus")
    if record.get("offscreen"):
        flags.append("offscreen")
    if "checked" in record:
        flags.append(f"checked={str(record['checked']).lower()}")
    if "currentIndex" in record:
        flags.append(f"index={record['currentIndex']}")
    if "value" in record:
        flags.append(f"value={_number(record['value'])}")
    if flags:
        parts.append("[" + " ".join(flags) + "]")
    if record.get("window") and record["window"] != "main":
        parts.append(f"(window {record['window']})")
    return " ".join(parts)


def _number(value: Any) -> str:
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return str(value)


def tree_lines(nodes: list[dict[str, Any]], indent: int = 0) -> list[str]:
    lines: list[str] = []
    for node in nodes:
        prefix = "  " * indent
        label = describe(node)
        if indent == 0 and node.get("window"):
            label = f"[window {node['window']}] {label}"
        lines.append(prefix + label)
        lines.extend(tree_lines(node.get("children", []), indent + 1))
    return lines


def window_line(window: dict[str, Any]) -> str:
    rect = window.get("rect", [0, 0, 0, 0])
    state = "visible" if window.get("visible") else "hidden"
    if window.get("active"):
        state += ", active"
    if window.get("kind"):
        state += f", {window['kind']}"
        if window.get("owner") and window["owner"] != window.get("key"):
            state += f" of {window['owner']}"
    if window.get("capturable") is False:
        state += ", not capturable"
    size = f" {rect[2]}x{rect[3]}" if window.get("rect") else ""
    return f"{window.get('key')}: {json.dumps(window.get('title', ''), ensure_ascii=False)}{size} ({state})"


def value_text(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False)


def error_lines(message: str, details: dict[str, Any]) -> list[str]:
    lines = [f"error: {message}"]
    for candidate in details.get("candidates", []):
        lines.append(f"  candidate: {describe(candidate)}")
    if details.get("similar"):
        lines.append("  similar names/ids/texts: " + ", ".join(details["similar"]))
    for window in details.get("windows", []):
        lines.append(f"  window {window_line(window)}")
    if details.get("properties"):
        lines.append("  properties: " + ", ".join(details["properties"]))
    if details.get("methods"):
        lines.append("  methods: " + ", ".join(details["methods"]))
    if "lastValue" in details:
        lines.append(f"  last value: {value_text(details['lastValue'])}")
    if details.get("lastError"):
        lines.append(f"  last error: {details['lastError']}")
    if details.get("commands"):
        lines.append("  commands: " + ", ".join(details["commands"]))
    return lines


def warning_lines(result: dict[str, Any]) -> list[str]:
    return [f"app {entry.get('type', 'warning')}: {entry.get('message', '')}" for entry in result.get("newWarnings", [])]


def log_line(entry: dict[str, Any]) -> str:
    source = f" ({entry['source']})" if entry.get("source") else ""
    return f"[{entry.get('type')}] {entry.get('message')}{source}"
