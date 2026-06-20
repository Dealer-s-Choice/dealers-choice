#!/bin/bash
# Phased sanitizer soak of the Dealer's Choice server: verifies the server
# survives sustained play, bot churn, and the 0-player showdown path, with
# ASan/UBSan armed to abort on the first violation.
#
# Phase 1: P1_BOTS bots for P1_MIN minutes.
# Phase 2: P2_BOTS bots for P2_MIN minutes.
# Phase 3: force the 0-player path P3_ROUNDS times (kill all bots mid-hand).
# Phase 4: stop.  Exits 1 with a stack-trace report if the server ever dies
# or a sanitizer fires; exits 0 if every phase passes.
#
# Build the binaries first, e.g.:
#   CC=clang CFLAGS=-fno-sanitize-recover=all meson setup _build_asan \
#       -Db_sanitize=address,undefined -Db_lundef=false -Dgen_protobuf=true
#   meson compile -C _build_asan
#
# Overridable via env (defaults shown). DC_REPO defaults to this repo (resolved
# from the script's own location); set it to a worktree to soak that build.
DC_REPO="${DC_REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DC_BUILD="${DC_BUILD:-$DC_REPO/_build_asan}"
DC_PORT="${DC_PORT:-22777}"
DC_PASSWORD="${DC_PASSWORD:-blippy}"
P1_BOTS="${P1_BOTS:-2}"; P1_MIN="${P1_MIN:-15}"
P2_BOTS="${P2_BOTS:-3}"; P2_MIN="${P2_MIN:-10}"
P3_ROUNDS="${P3_ROUNDS:-5}"
SRVLOG="${SRVLOG:-/tmp/dc_soak_server.log}"
LOG="${LOG:-/tmp/dc_soak.log}"

export DC_PASSWORD
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:abort_on_error=1:print_stacktrace=1:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:abort_on_error=1:print_stacktrace=1}"
: > "$LOG"
say(){ echo "$(date '+%H:%M:%S') $*" >> "$LOG"; }
BOTS=()
launch_bots(){
  for n in $(seq 1 "$1"); do
    DEALERSCHOICE_DATADIR="$DC_REPO/data" stdbuf -oL "$DC_BUILD/dealers-choice-bot" \
      --host 127.0.0.1 --port "$DC_PORT" --nick "Bot$n" --verbose > "/tmp/dc_bot$n.log" 2>&1 &
    BOTS+=($!)
  done
  say "launched $1 bots: ${BOTS[*]}"
}
kill_bots(){ for p in "${BOTS[@]}"; do kill "$p" 2>/dev/null; done; say "killed bots"; BOTS=(); }
cleanup(){ kill_bots 2>/dev/null; [ -n "${SRV:-}" ] && kill "$SRV" 2>/dev/null; }
trap cleanup EXIT
trap 'exit 130' INT TERM
check_server(){
  if ! kill -0 "$SRV" 2>/dev/null; then
    say "ANOMALY: server ($SRV) DIED"
    grep -iE "AddressSanitizer|Undefined|runtime error:|SUMMARY:|assertion|abort" "$SRVLOG" | tail -25 >> "$LOG"
    tail -30 "$SRVLOG" >> "$LOG"; return 1
  fi
  if grep -qiE "AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|heap-use-after|stack-buffer|assertion failed" "$SRVLOG" 2>/dev/null; then
    say "ANOMALY: sanitizer/assert report"
    grep -iE "AddressSanitizer|Undefined|runtime error:|SUMMARY:|assertion" "$SRVLOG" | tail -20 >> "$LOG"; return 1
  fi
  return 0
}
abort(){ say "ABORTED"; exit 1; }
soak(){ local secs=$(( $1 * 60 )) t=0; while [ $t -lt $secs ]; do check_server || abort; sleep 20; t=$((t+20)); done; }

say "launching sanitized server (port $DC_PORT, build $DC_BUILD)"
DEALERSCHOICE_DATADIR="$DC_REPO/data" stdbuf -oL -eL "$DC_BUILD/dealers-choice-server" --port "$DC_PORT" --verbose > "$SRVLOG" 2>&1 &
SRV=$!
sleep 3
kill -0 "$SRV" 2>/dev/null || { say "server failed to start"; exit 1; }
say "server up (PID $SRV)"

say "=== PHASE 1: $P1_BOTS bots, $P1_MIN min ==="
launch_bots "$P1_BOTS"; soak "$P1_MIN"; kill_bots; sleep 3; check_server || abort; say "phase 1 OK"

say "=== PHASE 2: $P2_BOTS bots, $P2_MIN min ==="
launch_bots "$P2_BOTS"; soak "$P2_MIN"; kill_bots; sleep 3; check_server || abort; say "phase 2 OK"

say "=== PHASE 3: force 0-player path x$P3_ROUNDS ==="
for r in $(seq 1 "$P3_ROUNDS"); do
  launch_bots 2
  sleep 8   # let a hand get going
  for p in "${BOTS[@]}"; do kill "$p" 2>/dev/null; done; BOTS=()   # kill all ~simultaneously
  say "round $r: killed all bots mid-hand"
  sleep 3
  check_server || { say "server died on 0-path round $r"; kill "$SRV" 2>/dev/null; exit 1; }
  say "round $r: server survived"
  sleep 2
done

say "=== PHASE 4: stop tests ==="
kill_bots; kill "$SRV" 2>/dev/null
say "DONE: all phases passed, server stopped clean"
exit 0
