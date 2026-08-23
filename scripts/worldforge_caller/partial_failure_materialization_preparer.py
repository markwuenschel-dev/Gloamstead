#!/usr/bin/env python3
"""Test fixture: write a partial request, then fail before publication."""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    output = Path(sys.argv[sys.argv.index("--output") + 1])
    output.write_text('{"partial":true}\n', encoding="utf-8")
    return 9


if __name__ == "__main__":
    raise SystemExit(main())
