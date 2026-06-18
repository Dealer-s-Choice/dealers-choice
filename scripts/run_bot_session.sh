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

. "$(dirname "$0")/_harness_common.sh"

DURATION="${1:-90}"
dc_locate_binaries

OUT_DIR="${2:-$REPO_ROOT/bot_session.$(date +%Y%m%d_%H%M%S)}"
mkdir -p "$OUT_DIR"
PORT="${DC_PORT:-23123}"
PASSWORD="${DC_PASSWORD:-bot-test-pw}"

dc_write_server_conf "$OUT_DIR/server.conf" "$PORT" "$PASSWORD"

export DC_PASSWORD="$PASSWORD"
dc_export_runtime_env "$OUT_DIR"

cleanup() {
  local rc=$?
  if [[ -n "${SERVER_PID:-}" ]]; then kill "$SERVER_PID" 2>/dev/null || true; fi
  for pid in "${BOT_PIDS[@]:-}"; do kill "$pid" 2>/dev/null || true; done
  wait 2>/dev/null || true
  exit "$rc"
}
trap cleanup EXIT INT TERM

echo "Starting server on port $PORT (out: $OUT_DIR)" >&2
"$SERVER" \
  --server-conf "$OUT_DIR/server.conf" \
  --server-log-hands "$OUT_DIR/hands.jsonl" \
  --server-log-game-results "$OUT_DIR/game_results.md" \
  >"$OUT_DIR/server.log" 2>&1 &
SERVER_PID=$!

sleep 1

BOT_PIDS=()
for i in 1 2 3; do
  "$BOT" --host 127.0.0.1 --port "$PORT" --nick "Bot$i" --verbose \
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
