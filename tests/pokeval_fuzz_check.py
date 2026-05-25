#!/usr/bin/env python3
"""Meson test wrapper: run the pokeval fuzzer, then run analyze_hands.py
against its output to confirm pokeval's declared winners match the
independent Python evaluator.

Exits non-zero on any mismatch.  Tuned to stay well under a 5-second
test budget on commodity hardware (1000 hands × 13 variants, with the
analyzer cross-check, runs in <1s on a development machine).
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

DEFAULT_N_HANDS = 1000
DEFAULT_SEED = 1


def main() -> int:
    build_root = os.environ.get("MESON_BUILD_ROOT")
    test_root = os.environ.get("MESON_BUILD_TEST_ROOT")
    source_root = os.environ.get("MESON_SOURCE_ROOT")
    if not (build_root and test_root and source_root):
        print("error: MESON_BUILD_ROOT/MESON_BUILD_TEST_ROOT/MESON_SOURCE_ROOT must be set",
              file=sys.stderr)
        return 2

    # Windows builds (MSVC, MSYS2) emit `.exe`; POSIX builds have no suffix.
    fuzz_bin = Path(test_root) / "test_pokeval_fuzz"
    if not fuzz_bin.exists():
        fuzz_bin = fuzz_bin.with_suffix(".exe")
    analyzer = Path(source_root) / "scripts" / "analyze_hands.py"
    if not fuzz_bin.exists():
        print(f"error: {fuzz_bin} not found", file=sys.stderr)
        return 2
    if not analyzer.exists():
        print(f"error: {analyzer} not found", file=sys.stderr)
        return 2

    n_hands = int(os.environ.get("DC_FUZZ_N", DEFAULT_N_HANDS))
    seed = int(os.environ.get("DC_FUZZ_SEED", DEFAULT_SEED))

    jsonl_path = Path(test_root) / "pokeval_fuzz.jsonl"
    with jsonl_path.open("w") as fp:
        rc = subprocess.run([str(fuzz_bin), str(n_hands), str(seed)],
                            stdout=fp, stderr=sys.stderr).returncode
    if rc != 0:
        print(f"error: fuzz binary exited {rc}", file=sys.stderr)
        return rc

    result = subprocess.run([sys.executable, str(analyzer), str(jsonl_path)],
                            stdout=sys.stdout, stderr=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
