#!/usr/bin/env bash
# Collect runtime DLLs needed by dealers-choice.exe from the MSYS2 installation.
# Run this from the repository root inside an MSYS2 shell after building.
# Usage: bash packaging/windows/collect-dlls.sh <exe> <dest-dir>

set -euo pipefail

EXE="${1:?Usage: $0 <exe> <dest-dir>}"
DEST="${2:?Usage: $0 <exe> <dest-dir>}"

mkdir -p "$DEST"
DEST_ABS=$(cd "$DEST" && pwd -P)

# ldd prints lines like:
#   libSDL2-2.0-0.dll => /ucrt64/bin/libSDL2-2.0-0.dll (0x...)
# We keep only DLLs that live inside the MSYS2 tree (not C:/Windows/...).
# Read from a process substitution rather than piping into the loop: with
# pipefail, a pipeline whose grep matches nothing exits 1, so the old form would
# fail the staging step the moment a build stopped needing any DLL (e.g. after
# static-linking). Zero DLLs is a valid result.
# Skip anything ldd resolved out of $DEST itself. Windows searches the exe's own
# directory first, so once one exe's DLLs are staged, ldd on the next exe in that
# same directory reports the staged copies -- and cp would be asked to copy a
# file onto itself, which is an error under `set -e`. Skipping also makes the
# script idempotent, so re-running it on an already-staged tree is a no-op.
while read -r dll; do
  if [ -f "$dll" ]; then
    if [ "$(cd "$(dirname "$dll")" && pwd -P)" = "$DEST_ABS" ]; then
      echo "Skipping (already staged): $dll"
      continue
    fi
    echo "Copying: $dll"
    cp "$dll" "$DEST/"
  fi
done < <(ldd "$EXE" | grep -i '=> /' | grep -v '/c/[Ww]indows' | awk '{print $3}')
