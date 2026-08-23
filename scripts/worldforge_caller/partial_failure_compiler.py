#!/usr/bin/env python3
"""Test-only compiler fixture that leaves a staged partial output and fails."""

from __future__ import annotations

import sys
from pathlib import Path


output_root = Path(sys.argv[sys.argv.index("--output-root") + 1])
(output_root / "manifest.json").write_text("partial", encoding="utf-8")
raise SystemExit(7)
