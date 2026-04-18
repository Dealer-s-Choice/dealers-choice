#!/bin/sh
#
# 2026
#
# Removes the packaging/ directory from the meson dist tarball and
# regenerates the .sha256sum file.
#
# Run from the project root or from within a build directory.

set -e

if [ -d "meson-dist" ]; then
  dist_dir="meson-dist"
else
  build_dir=$(ls -d _build* 2>/dev/null | head -1)
  if [ -z "$build_dir" ]; then
    echo "error: run from the project root or a build directory" >&2
    exit 1
  fi
  dist_dir="$build_dir/meson-dist"
fi

tarball=$(ls "$dist_dir"/*.tar.xz 2>/dev/null | head -1)
if [ -z "$tarball" ]; then
  echo "error: no .tar.xz found in $dist_dir (run 'meson dist' first)" >&2
  exit 1
fi

tarball_name=$(basename "$tarball")
echo "Processing $tarball_name ..."

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

tar -C "$tmp" -xf "$tarball"

inner=$(ls "$tmp")
strip_list="$(dirname "$0")/dist-strip-list.txt"
if [ ! -f "$strip_list" ]; then
  echo "error: $strip_list not found" >&2
  exit 1
fi

grep -v '^\s*#' "$strip_list" | grep -v '^\s*$' | while IFS= read -r entry; do
  target="$tmp/$inner/$entry"
  if [ ! -e "$target" ]; then
    echo "warning: $entry not found in tarball, skipping"
  else
    rm -rf "$target"
    echo "Removed $entry from $tarball_name"
  fi
done

tar -C "$tmp" -cJf "$tarball" "$inner"

(cd "$dist_dir" && sha256sum -b "$tarball_name" > "$tarball_name.sha256sum")
echo "Wrote $dist_dir/$tarball_name.sha256sum"
