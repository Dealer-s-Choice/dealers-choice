---
description: Pre-merge gate — gcc -Werror + clang ASan + full tests + soak, with one go/no-go verdict
allowed-tools: Bash
---
Run the full pre-merge gate on the current branch/worktree and return a single
**PASS / FAIL** verdict. Use this before merging a branch to trunk (the human
still does the actual merge/push). Run from the repo root or a worktree root.
Wrap heavy builds in `nice -n 19 ionice -c 3`.

The gate is fail-fast: stop at the first failing stage and report FAIL with the
relevant output — don't run later stages on a broken build.

**Stage 1 — build + tests.** Perform the entire `/dc-verify` procedure:
gcc `-Werror` gate, clang ASan/UBSan build, then `meson test -C _build_asan`.
(That file holds the canonical setup/compile flags — follow it, don't restate
them here.) Any gcc warning, ASan/UBSan failure, or test failure = **FAIL**, stop.

**Stage 2 — soak.** Perform the `/dc-soak` procedure. Judge it **by the log, not
the exit code**: a pass ends with `DONE: all phases passed` in the soak log, and
`grep -ic 'runtime error\|AddressSanitizer'` on the server log must be `0`.
Anything else = **FAIL**.

- Skip Stage 2 only when the branch is clearly **non-server** (docs-only, or
  GUI/`src/ui` with no change under `src/{server,net,game}`, `src/tcpme`,
  `src/pokeval`, or the protocol). When you skip it, **say so and say why** —
  don't silently drop it. When unsure, run it.

**Verdict.** Report `PASS` only if: gcc clean (zero warnings), ASan/UBSan clean,
all meson tests green (give the X/Y), and the soak passed (or was justifiably
skipped). Otherwise `FAIL` with the first failing stage and its evidence. Finish
with the exact build dirs and the soak log path so the result is reproducible.

In a second worktree, pass a distinct base port to avoid collisions
(`DC_PORT=24900 meson test …`, and a different `DC_PORT` for the soak).
See `tests/README.md` for the full testing guide.
