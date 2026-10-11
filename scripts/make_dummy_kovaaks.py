"""Generate a fake KovaaK's install directory for testing KSV on a machine without KovaaK's.

    uv run python scripts/make_dummy_kovaaks.py [OUT_DIR] [--days N] [--seed N]

Writes `<OUT_DIR>/FPSAimTrainer/performances/*Performance.perf` (hand-encoded protobuf,
schema in src/data/formats/protobuf/schema/perf.proto) and the matching
`<OUT_DIR>/FPSAimTrainer/stats/*Stats.csv`. Point KSV's Kovaaks Directory setting at OUT_DIR.
Output is deterministic for a given seed. Default OUT_DIR is `.temp/dummy-kovaaks`.
"""
from __future__ import annotations

import argparse
import hashlib
import random
import shutil
import struct
from datetime import datetime, timedelta
from pathlib import Path

SCENARIOS = [
    # name, map, length, base score, growth/run, shots/s, base accuracy
    ("1wall6targets TE", "cube_1wall_dense.map", 60.0, 120.0, 0.9, 3.2, 0.72),
    ("VT FlyTS Novice S5", "cube_fly.map", 60.0, 55.0, 0.5, 2.6, 0.58),
    ("Close Long Strafes Invincible", "cube_strafes.map", 60.0, 900.0, 4.0, 3.0, 0.66),
    ("Tile Frenzy Strafing", "cube_tile.map", 60.0, 70.0, 0.6, 2.8, 0.64),
    ("Smoothbot Goated", "cube_smooth.map", 60.0, 40.0, 0.35, 2.4, 0.51),
]


def varint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def key(field: int, wire: int) -> bytes:
    return varint(field << 3 | wire)


def f_varint(field: int, value: int) -> bytes:
    return key(field, 0) + varint(value)


def f_float(field: int, value: float) -> bytes:
    return key(field, 5) + struct.pack("<f", value)


def f_bytes(field: int, payload: bytes) -> bytes:
    return key(field, 2) + varint(len(payload)) + payload


def f_str(field: int, text: str) -> bytes:
    return f_bytes(field, text.encode())


def u32(field: int, value: int) -> bytes:
    return f_bytes(field, f_varint(1, value))


def f32(field: int, value: float) -> bytes:
    return f_bytes(field, f_float(1, value))


def encode_perf(name: str, scenario_hash: str, end_ms: int, length: float, map_name: str,
                entries: list[tuple[float, int, int, float, float, int]]) -> bytes:
    details = (f_float(1, length) + f_str(2, "Player") + f_str(3, "target.bot")
               + f_bytes(5, b"\0\0\0") + f_varint(6, 1) + f_bytes(7, b"\0\0\0")
               + f_str(8, map_name) + f_float(9, 4.0) + f_float(10, 1.0))
    meta = f_str(1, name) + f_str(2, scenario_hash) + f_varint(3, end_ms) + f_varint(4, 1) + f_bytes(5, details)
    out = f_bytes(1, meta)
    for time, shots, hits, dmg, score, kills in entries:
        entry = (f_float(1, time) + u32(2, shots) + u32(3, hits) + u32(4, shots - hits)
                 + f32(5, dmg) + f32(6, float(shots)) + f32(7, score) + u32(8, kills))  # all per-interval deltas
        out += f_bytes(2, entry)
    return out


def simulate_run(rng: random.Random, scenario, run_index: int):
    name, map_name, length, base, growth, sps, base_acc = scenario
    skill = base + growth * run_index
    acc = min(0.97, base_acc + 0.002 * run_index + rng.uniform(-0.04, 0.04))
    total_shots = int(length * sps * rng.uniform(0.9, 1.1))
    score = max(1.0, skill * rng.uniform(0.93, 1.07))
    steps = int(length)
    # Per-second score deltas are multiples of 1/64, so the decoder's float32 sum is exact
    # and matches the CSV's printed total.
    deltas = [round(score / steps * 64) / 64 for _ in range(steps)]
    score = sum(deltas)
    shots = hits = 0
    kills_at: list[float] = []
    entries = []
    for step in range(1, steps + 1):
        new = round(total_shots / steps + rng.uniform(-1, 1))
        new = max(0, new)
        got = sum(1 for _ in range(new) if rng.random() < acc)
        shots += new
        hits += got
        for _ in range(got):
            kills_at.append(step - rng.random())
        # The decoder sums entries, so each one is the delta for its one-second interval.
        entries.append((float(step), new, got, float(got), deltas[step - 1], got))
    return score, shots, hits, kills_at, entries


def clock(moment: datetime) -> str:
    return moment.strftime("%H:%M:%S.") + f"{moment.microsecond // 1000:03d}"


def write_csv(path: Path, name: str, scenario_hash: str, start: datetime, score: float, shots: int, hits: int,
              kills_at: list[float], length: float) -> None:
    rows = ["Kill #,Timestamp,Bot,Weapon,TTK,Shots,Hits,Accuracy,Damage Done,Damage Possible,Efficiency,Cheated,OverShots"]
    for number, offset in enumerate(sorted(kills_at), 1):
        rows.append(f"{number},{clock(start + timedelta(seconds=offset))},target,BB Gun,0.000000s,1,1,"
                    "1.000000,1.000000,1.000000,1.000000,0,0")
    misses = shots - hits
    text = "\n".join(rows) + f"""

Weapon,Shots,Hits,Damage Done,Damage Possible,,Sens Scale,Horiz Sens,Vert Sens,FOV,Hide Gun,Crosshair,Crosshair Scale,Crosshair Color,ADS Sens,ADS Zoom Scale,Avg Target Scale,Avg Time Dilation
BB Gun,{shots},{hits},{float(hits)},{float(shots)},

Kills:,{hits}
Deaths:,0
Fight Time:,{length:.3f}
Time Remaining:,0.0
Avg TTK:,0.361058
Damage Done:,{float(hits)}
Total Overshots:,0
Damage Taken:,0.0
Hit Count:,{hits}
Miss Count:,{misses}
Midairs:,0
Midaired:,0
Directs:,0
Directed:,0
Reloads:,0
Distance Traveled:,0.0
MBS Points:,0.0
Score:,{score:.6f}
Scenario:,{name}
Hash:,{scenario_hash}
Game Version:,3.9.5.2026-07-06-16-10-18-f12d73508953
Challenge Start:,{clock(start)}
Pause Count:,0
Pause Duration:,0
Avg Target Scale:,1.0
Avg Time Dilation:,1.0

Input Lag:,0
Max FPS (config):,300.0
Sens Scale:,Valorant
Sens Increment:,0.159909
Horiz Sens:,0.16
Vert Sens:,0.16
DPI:,1600
FOV:,103.0
FOVScale:,Valorant
Hide Gun:,false
Crosshair:,PopDot.png
Crosshair Scale:,1.0
Crosshair Color:,000100FF
Resolution:,1920x1080
Avg FPS:,207.939728
Resolution Scale:,100.0
"""
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", nargs="?", default=".temp/dummy-kovaaks")
    parser.add_argument("--days", type=int, default=30, help="how many days of history to span")
    parser.add_argument("--seed", type=int, default=1234)
    args = parser.parse_args()

    out = Path(args.out_dir)
    if out.exists():
        shutil.rmtree(out)
    perf_dir = out / "FPSAimTrainer" / "performances"
    stats_dir = out / "FPSAimTrainer" / "stats"
    perf_dir.mkdir(parents=True)
    stats_dir.mkdir(parents=True)

    rng = random.Random(args.seed)
    today = datetime.now().replace(microsecond=0)
    count = 0
    for scenario in SCENARIOS:
        name, map_name, length = scenario[0], scenario[1], scenario[2]
        scenario_hash = hashlib.md5(name.encode()).hexdigest()
        runs = rng.randint(14, 24)
        for run_index in range(runs):
            day = args.days - int(run_index * args.days / runs)
            end = (today - timedelta(days=day)).replace(hour=rng.randint(17, 23), minute=rng.randint(0, 59),
                                                         second=rng.randint(0, 59))
            start = end - timedelta(seconds=length)
            score, shots, hits, kills_at, entries = simulate_run(rng, scenario, run_index)
            stem = f"{name} - Challenge - {end:%Y.%m.%d-%H.%M.%S}"
            perf = encode_perf(name, scenario_hash, int(end.timestamp() * 1000), length, map_name, entries)
            (perf_dir / f"{stem} Performance.perf").write_bytes(perf)
            write_csv(stats_dir / f"{stem} Stats.csv", name, scenario_hash, start, score, shots, hits,
                      kills_at, length)
            count += 1
    print(f"Wrote {count} runs across {len(SCENARIOS)} scenarios to {out.resolve()}")


if __name__ == "__main__":
    main()
