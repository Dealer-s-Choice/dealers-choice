---
description: Build the gcc -Werror gate + clang ASan, then run the full meson test suite
allowed-tools: Bash
---
Verify the current working tree builds clean and passes tests. Run from the repo
root (or a worktree root). Wrap heavy builds in `nice -n 19 ionice -c 3`.

1. **gcc -Werror gate** — this is the CI gate; clang stays silent on warnings it
   emits (e.g. `-Wformat-truncation`), so a clang-clean branch can still red CI.
   - configure once if `_build_gcc` is missing:
     `CC=gcc CFLAGS=-Werror meson setup _build_gcc -Dgen_protobuf=true`
   - `meson compile -C _build_gcc`
2. **clang ASan/UBSan build:**
   - configure once if `_build_asan` is missing:
     `CC=clang CFLAGS="-fno-sanitize-recover=all" meson setup _build_asan -Db_sanitize=address,undefined -Db_lundef=false -Dgen_protobuf=true`
   - `meson compile -C _build_asan`
3. **tests:** `meson test -C _build_asan`
   (In a second worktree, pass a distinct base port: `DC_PORT=24900 meson test …`.)

If `dc_protocol.proto` changed, run `meson compile -C _build_<dir> gen-proto`
before the normal compile.

Report: gcc clean (any warning = gate failure), ASan clean, and the X/Y test
result. See `tests/README.md` for the full testing guide.
