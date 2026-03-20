#!/usr/bin/env bash
# Collect runtime DLLs needed by dealers-choice.exe from the MSYS2 installation.
# Run this from the repository root inside an MSYS2 shell after building.
# Usage: bash packaging/windows/collect-dlls.sh <exe> <dest-dir>

set -euo pipefail

EXE="${1:?Usage: $0 <exe> <dest-dir>}"
DEST="${2:?Usage: $0 <exe> <dest-dir>}"

mkdir -p "$DEST"

# ldd prints lines like:
#   libSDL2-2.0-0.dll => /ucrt64/bin/libSDL2-2.0-0.dll (0x...)
# We keep only DLLs that live inside the MSYS2 tree (not C:/Windows/...).
ldd "$EXE" \
  | grep -i '=> /' \
  | grep -v '/c/[Ww]indows' \
  | awk '{print $3}' \
  | while read -r dll; do
      if [ -f "$dll" ]; then
        echo "Copying: $dll"
        cp "$dll" "$DEST/"
      fi
    done
