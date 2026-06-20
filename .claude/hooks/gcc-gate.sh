#!/bin/bash
# PostToolUse hook: after an Edit/Write touches a C source/header, run the
# incremental gcc -Werror gate (the CI gate; the usual ASan dev build is clang,
# which stays silent on warnings like -Wformat-truncation). Surfaces gate errors
# right away so they're fixed before a branch is called CI-ready.
#
# - No-op unless _build_gcc is configured, so it does nothing for contributors
#   who don't use that build dir. (Set it up with:
#     CC=gcc CFLAGS=-Werror meson setup _build_gcc -Dgen_protobuf=true)
# - Incremental: a no-op ninja is instant; a one-file recompile is ~1s.
# - It can transiently flag a half-finished multi-file edit (e.g. a call added
#   before its function); that clears once the edits are complete.
#
# Activate it per-user via .claude/settings.local.json (not committed):
#   { "hooks": { "PostToolUse": [ { "matcher": "Edit|Write",
#       "hooks": [ { "type": "command",
#         "command": "bash \"$CLAUDE_PROJECT_DIR/.claude/hooks/gcc-gate.sh\"" } ] } ] } }

root="${CLAUDE_PROJECT_DIR:-.}"
cd "$root" 2>/dev/null || exit 0
[ -f _build_gcc/build.ninja ] || exit 0

# Only react when a C source/header actually changed in the working tree.
git diff --name-only 2>/dev/null | grep -qE '\.(c|h)$' || exit 0

out=$(ninja -C _build_gcc 2>&1)
if printf '%s' "$out" | grep -qiE 'error:|FAILED'; then
  {
    echo "gcc -Werror gate failed (this is the CI gate). Fix before CI-ready:"
    printf '%s\n' "$out" | grep -iE 'error:|warning:|FAILED' | head -20
  } >&2
  exit 2
fi
exit 0
