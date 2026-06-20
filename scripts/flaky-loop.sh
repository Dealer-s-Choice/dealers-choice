#!/bin/bash
# Repeatedly run named meson tests under ASan/UBSan to flush out
# timing-dependent flakes, classifying each failure as a TIMEOUT (recv hang)
# vs an ASSERTION (turn-order desync).  Run it concurrently with soak.sh so the
# tests execute under CPU load — the documented trigger for the flaky timeouts.
#
# Usage:   flaky-loop.sh [test_name ...]        (default: test_raises test_check)
# Env:     ROUNDS (default 40), DC_BUILD (default <repo>/_build_asan)
#
# NOTE: do NOT run this under `tc netem` impairment. These are deterministic
# DC_TEST-mode tests with deliberately short server timeouts; added latency makes
# the server time out the lockstep client and disconnect it (spurious failures,
# not real bugs). For lag/loss testing use scripts/soak.sh under scripts/netem.sh
# (real bots, normal 30 s timeouts).
DC_REPO="${DC_REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DC_BUILD="${DC_BUILD:-$DC_REPO/_build_asan}"
ROUNDS="${ROUNDS:-40}"
LOG="${LOG:-/tmp/dc_flaky_loop.log}"
TESTS=("$@"); [ ${#TESTS[@]} -eq 0 ] && TESTS=(test_raises test_check)

cd "$DC_REPO" || exit 1
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:abort_on_error=1:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:abort_on_error=1}"
: > "$LOG"
pass=0; fail=0; timeouts=0; asserts=0
echo "$(date '+%H:%M:%S') START: $ROUNDS rounds of {${TESTS[*]}} under ASan" >> "$LOG"
for i in $(seq 1 "$ROUNDS"); do
  if out=$(meson test -C "$DC_BUILD" "${TESTS[@]}" --print-errorlogs 2>&1); then
    pass=$((pass+1)); echo "$(date '+%H:%M:%S') run $i: PASS" >> "$LOG"
  else
    fail=$((fail+1)); echo "$(date '+%H:%M:%S') run $i: FAIL ==========" >> "$LOG"
    echo "$out" | grep -iE "Timeout|timed out" >/dev/null && { timeouts=$((timeouts+1)); echo "  -> TIMEOUT (recv hang)" >> "$LOG"; }
    echo "$out" | grep -iE "assert|expected_bet_turn|raises\.c:" >/dev/null && { asserts=$((asserts+1)); echo "  -> ASSERTION (turn-order desync)" >> "$LOG"; }
    echo "$out" | grep -iE "timeout|assert|expected_bet_turn|raises\.c|SUMMARY:|AddressSanitizer|runtime error:" >> "$LOG"
    cp "$DC_BUILD/meson-logs/testlog.txt" "/tmp/dc_flaky_fail_run$i.txt" 2>/dev/null
  fi
done
echo "$(date '+%H:%M:%S') DONE: $pass pass / $fail fail of $ROUNDS  (timeouts=$timeouts asserts=$asserts)" >> "$LOG"
