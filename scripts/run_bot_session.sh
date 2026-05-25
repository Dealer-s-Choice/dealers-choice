#!/usr/bin/env bash
# Bring up a local Dealer's Choice server with 3 bots for automated analysis.
#
# Usage:
#   scripts/run_bot_session.sh [duration_seconds] [output_dir]
#
# Defaults: 90 seconds, ./bot_session.<timestamp>/ inside the repo root.
#
# Writes:
#   <out>/server.conf         server config used (short timeouts, bet_amounts)
#   <out>/hands.jsonl         JSON-lines hand log (analyze_hands.py input)
#   <out>/game_results.md     markdown game log (existing format)
#   <out>/server.log          server stdout/stderr
#   <out>/bot1.log .. bot3.log

set -euo pipefail

DURATION="${1:-90}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/_build_dealers_choice"
SERVER="$BUILD_DIR/dealers-choice"
BOT="$BUILD_DIR/dealers-choice-bot"

if [[ ! -x "$SERVER" || ! -x "$BOT" ]]; then
  echo "error: build first ('meson compile -C _build_dealers_choice')" >&2
  exit 1
fi

OUT_DIR="${2:-$REPO_ROOT/bot_session.$(date +%Y%m%d_%H%M%S)}"
mkdir -p "$OUT_DIR"
PORT="${DC_PORT:-23123}"
PASSWORD="${DC_PASSWORD:-bot-test-pw}"

cat >"$OUT_DIR/server.conf" <<EOF
bind_address = 127.0.0.1
port = $PORT
end_of_game_timeout = 5
action_timeout = 30
action_timeout_max = 3
dealer_timeout = 30
ante = 50
bringin_amount = 50
starting_coins = 20000
max_raises = 3
bet_amounts = list, 100, 250, 500
password = $PASSWORD
max_connections_per_minute = 60
max_connections_per_ip = 0
EOF

export DEALERSCHOICE_DATADIR="$REPO_ROOT/data"
export DC_PASSWORD="$PASSWORD"

# When the build is sanitized (b_sanitize=address,undefined) the LSan/UBSan
# runtimes must be preloaded for any non-link-time invocation.  Auto-detect
# and combine with libstdbuf so `stdbuf -oL` keeps working (it relies on
# LD_PRELOAD itself, so anything we add must include it).
PRELOAD_LIBS=()
if grep -q -- "-fsanitize=address" "$BUILD_DIR/compile_commands.json" 2>/dev/null; then
  ASAN_LIB="$(gcc -print-file-name=libasan.so 2>/dev/null)"
  UBSAN_LIB="$(gcc -print-file-name=libubsan.so 2>/dev/null)"
  [[ -e "$ASAN_LIB" ]] && PRELOAD_LIBS+=("$ASAN_LIB")
  [[ -e "$UBSAN_LIB" ]] && PRELOAD_LIBS+=("$UBSAN_LIB")
fi
# Locate libstdbuf — different distros put it in different places.
for stdbuf_lib in /usr/lib/coreutils/libstdbuf.so /usr/libexec/coreutils/libstdbuf.so \
                 /usr/lib64/coreutils/libstdbuf.so; do
  if [[ -e "$stdbuf_lib" ]]; then
    PRELOAD_LIBS+=("$stdbuf_lib")
    break
  fi
done
if [[ ${#PRELOAD_LIBS[@]} -gt 0 ]]; then
  LD_PRELOAD_VAL="$(IFS=:; echo "${PRELOAD_LIBS[*]}")"
  export LD_PRELOAD="$LD_PRELOAD_VAL"
fi
# Default stdio is block-buffered when redirected to a file; force line-buffered
# via stdbuf so progress shows up in the logs in real time and isn't lost on
# SIGTERM.  These vars are what `stdbuf` itself sets; setting them directly
# avoids the `stdbuf` wrapper replacing LD_PRELOAD.
export _STDBUF_O=L
export _STDBUF_E=L

# Make any sanitizer crash visible in the log directory.
export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=0:halt_on_error=0:log_path=$OUT_DIR/asan}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:log_path=$OUT_DIR/ubsan}"
RUN_SERVER=("$SERVER")
RUN_BOT=("$BOT")

cleanup() {
  local rc=$?
  if [[ -n "${SERVER_PID:-}" ]]; then kill "$SERVER_PID" 2>/dev/null || true; fi
  for pid in "${BOT_PIDS[@]:-}"; do kill "$pid" 2>/dev/null || true; done
  wait 2>/dev/null || true
  exit "$rc"
}
trap cleanup EXIT INT TERM

echo "Starting server on port $PORT (out: $OUT_DIR)" >&2
"${RUN_SERVER[@]}" --server \
  --server-conf "$OUT_DIR/server.conf" \
  --server-log-hands "$OUT_DIR/hands.jsonl" \
  --server-log-game-results "$OUT_DIR/game_results.md" \
  >"$OUT_DIR/server.log" 2>&1 &
SERVER_PID=$!

sleep 1

BOT_PIDS=()
for i in 1 2 3; do
  "${RUN_BOT[@]}" --host 127.0.0.1 --port "$PORT" --nick "Bot$i" --verbose \
    >"$OUT_DIR/bot$i.log" 2>&1 &
  BOT_PIDS+=("$!")
  sleep 0.2
done

echo "Running for ${DURATION}s..." >&2
sleep "$DURATION"

echo "Stopping..." >&2
for pid in "${BOT_PIDS[@]}"; do kill "$pid" 2>/dev/null || true; done
sleep 1
kill "$SERVER_PID" 2>/dev/null || true
wait 2>/dev/null || true
trap - EXIT
BOT_PIDS=()
SERVER_PID=

if [[ -f "$OUT_DIR/hands.jsonl" ]]; then
  hands=$(wc -l <"$OUT_DIR/hands.jsonl")
  echo "Logged $hands hand(s) → $OUT_DIR/hands.jsonl" >&2
else
  echo "Warning: no hands.jsonl produced" >&2
fi
echo "$OUT_DIR"
