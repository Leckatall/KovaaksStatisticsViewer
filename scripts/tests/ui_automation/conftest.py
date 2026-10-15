from __future__ import annotations

from pathlib import Path
import sys


# ui_automation imports build_runner as a top-level package, the way
# scripts/ui_driver.py runs it (scripts/ is the script directory there).
SCRIPTS_DIR = Path(__file__).resolve().parents[2]
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))
