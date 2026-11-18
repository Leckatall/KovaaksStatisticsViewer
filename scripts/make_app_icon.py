"""Regenerate resources/app-icon/app-icon.ico from resources/app-icon/app-icon.svg.

    uv run --with resvg-py --with pillow python scripts/make_app_icon.py

The .ico is committed so the build never needs Python. Rerun this after editing the SVG and commit both.
Each size is rendered from the SVG at that size rather than downscaled from one large render. Windows
picks the entry matching the slot it is drawing (16 px title bar at 100% scaling, 24 px taskbar, 20/30/40
at higher scalings), so a hand-tuned entry for any one size can be swapped in here without touching
the others.
"""
from __future__ import annotations

from io import BytesIO
from pathlib import Path

import resvg_py
from PIL import Image

ICON_DIR = Path(__file__).resolve().parents[1] / "resources" / "app-icon"
SOURCE = ICON_DIR / "app-icon.svg"
OUTPUT = ICON_DIR / "app-icon.ico"
SIZES = (16, 20, 24, 30, 32, 36, 40, 48, 64, 96, 256)


def render(size: int) -> Image.Image:
    data = resvg_py.svg_to_bytes(svg_path=str(SOURCE), width=size, height=size, skip_system_fonts=True)
    return Image.open(BytesIO(bytes(data))).convert("RGBA")


def main() -> None:
    frames = [render(size) for size in SIZES]
    largest = frames[-1]
    largest.save(OUTPUT, format="ICO", sizes=[(s, s) for s in SIZES], append_images=frames[:-1])
    print(f"Wrote {OUTPUT} ({', '.join(str(s) for s in SIZES)} px)")


if __name__ == "__main__":
    main()
