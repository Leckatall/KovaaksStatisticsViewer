"""Snapshot the real app's settings (registry) and data (%APPDATA%) into a ui-driver fixture.

    uv run python tools/driver/snapshots/take_snapshot.py tools/driver/snapshots/<name>

Writes <name>/overlay/ (profile.pb, benchmarks/) and <name>/start.txt, a batch whose one `start` line
recreates the snapshot. The KovaaK's folder itself is not copied: the profile is the record of the runs.
"""
import json
import os
import shlex
import shutil
import struct
import sys
import winreg
from datetime import date
from pathlib import Path

APPDATA = Path(os.environ["APPDATA"]) / "Lecka" / "KovaaksStatsViewer"
REGISTRY_ROOT = r"Software\Lecka\KovaaksStatsViewer"
# file/ points at the real KovaaK's folder and profile; the driver sets the session's own.
# graph/seriesConfigs is a dead pre-document key holding a QByteArray, which --setting cannot express.
SKIPPED_PREFIXES = ("file/", "graph/seriesConfigQuarantine/", "graph/seriesConfigs")


class QVariantStream:
    """The QDataStream bytes inside a registry `@Variant(...)` value, which carry no null flag."""

    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def take(self, n: int) -> bytes:
        chunk = self.data[self.pos:self.pos + n]
        if len(chunk) != n:
            raise ValueError("truncated @Variant value")
        self.pos += n
        return chunk

    def u32(self) -> int:
        return struct.unpack(">I", self.take(4))[0]

    def string(self):
        n = self.u32()
        return None if n == 0xFFFFFFFF else self.take(n).decode("utf-16-be")

    def variant(self):
        type_id = self.u32()
        if type_id == 1: return self.take(1) != b"\0"
        if type_id == 2: return struct.unpack(">i", self.take(4))[0]
        if type_id == 8: return {self.string(): self.variant() for _ in range(self.u32())}
        if type_id == 9: return [self.variant() for _ in range(self.u32())]
        if type_id == 10: return self.string()
        if type_id == 11: return [self.string() for _ in range(self.u32())]
        if type_id == 17: return self.take(self.u32()).decode("utf-8")
        raise ValueError(f"unsupported QVariant type {type_id}")


def decode_variant(raw: bytes):
    text = raw.decode("utf-16-le")
    if not (text.startswith("@Variant(") and text.endswith(")")):
        raise ValueError(f"not a @Variant value: {text[:20]!r}")
    return QVariantStream(bytes(ord(c) for c in text[len("@Variant("):-1])).variant()


def convert(value, kind):
    if kind == winreg.REG_DWORD: return value
    if kind == winreg.REG_MULTI_SZ: return [v for v in value if v]
    if kind == winreg.REG_BINARY: return decode_variant(value)
    if value in ("true", "false"): return value == "true"
    try:
        document = json.loads(value)
    except ValueError:
        return value
    # A JSON document stored as a string must stay a string; compacting it keeps ksv_driver's argv short.
    return json.dumps(document, separators=(",", ":")) if isinstance(document, (dict, list)) else value


def registry_settings(path: str, prefix: str = ""):
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as key:
        subkeys, values, _ = winreg.QueryInfoKey(key)
        for i in range(values):
            name, value, kind = winreg.EnumValue(key, i)
            yield prefix + name, (value, kind)
        for i in range(subkeys):
            sub = winreg.EnumKey(key, i)
            yield from registry_settings(path + "\\" + sub, prefix + sub + "/")


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = Path(sys.argv[1])
    settings = sorted((key, convert(*raw)) for key, raw in registry_settings(REGISTRY_ROOT)
                      if not key.startswith(SKIPPED_PREFIXES))

    if out.exists():
        shutil.rmtree(out)
    overlay = out / "overlay"
    (overlay / "benchmarks").mkdir(parents=True)
    shutil.copy2(APPDATA / "profile.pb", overlay / "profile.pb")
    for benchmark in sorted((APPDATA / "benchmarks").glob("*.json")):
        shutil.copy2(benchmark, overlay / "benchmarks" / benchmark.name)

    start = ["start", "--dataset", "empty", "--overlay", "${BATCH_DIR}/overlay"]
    for key, value in settings:
        start += ["--setting", f"{key}={json.dumps(value, separators=(',', ':'))}"]
    (out / "start.txt").write_text(
        f"# Snapshot of the real app's data and settings, taken {date.today().isoformat()} "
        f"by tools/driver/snapshots/take_snapshot.py.\n{shlex.join(start)}\n",
        encoding="utf-8")
    print(f"{out}: {len(settings)} settings, {len(list((overlay / 'benchmarks').iterdir()))} benchmarks")


if __name__ == "__main__":
    main()
