#!/usr/bin/env bash
# Run a 4-bot game session and randomly kill+restart one bot every 60-120s
# to exercise the server's disconnect/reconnect handling.
#
# Usage:
#   scripts/disconnect_test.sh [duration_seconds] [output_dir]
#
# Default duration: 3600s (1 hour wall clock).
#
# Output files in <out>/:
#   server.conf          mirror of data/server.conf with short end-of-game
#                        timeout and a password set
#   server.log           server stdout/stderr (includes "Client N
#                        disconnected" and reconnect/auth events)
#   bot{1..4}.log        each bot's per-decision verbose log
#   bot{1..4}_runs.log   wrapper log: timestamps of every (re)start
#   hands.jsonl          server's --server-log-hands output
#   disconnect_events    one line per disconnect cycle (start ts, victim, gap_s)
#   game_results.md      existing markdown game log

set -euo pipefail

DURATION="${1:-3600}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/_build_dealers_choice"
SERVER="$BUILD_DIR/dealers-choice"
BOT="$BUILD_DIR/dealers-choice-bot"

if [[ ! -x "$SERVER" || ! -x "$BOT" ]]; then
  echo "error: build first ('meson compile -C _build_dealers_choice')" >&2
  exit 1
fi

OUT_DIR="${2:-$REPO_ROOT/disconnect_session.$(date +%Y%m%d_%H%M%S)}"
mkdir -p "$OUT_DIR"
PORT="${DC_PORT:-23456}"
PASSWORD="${DC_PASSWORD:-disc-test-pw}"

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

# Same sanitizer + line-buffer setup as run_bot_session.sh.
PRELOAD_LIBS=()
if grep -q -- "-fsanitize=address" "$BUILD_DIR/compile_commands.json" 2>/dev/null; then
  ASAN_LIB="$(gcc -print-file-name=libasan.so 2>/dev/null)"
  UBSAN_LIB="$(gcc -print-file-name=libubsan.so 2>/dev/null)"
  [[ -e "$ASAN_LIB" ]] && PRELOAD_LIBS+=("$ASAN_LIB")
  [[ -e "$UBSAN_LIB" ]] && PRELOAD_LIBS+=("$UBSAN_LIB")
fi
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
export _STDBUF_O=L _STDBUF_E=L
export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=0:halt_on_error=0:log_path=$OUT_DIR/asan}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:log_path=$OUT_DIR/ubsan}"

NUM_BOTS=4
declare -a BOT_PIDS

# Spawn / respawn a bot.  Each call overwrites the bot's log file; the
# bot{N}_runs.log file records each (re)start with a timestamp.
spawn_bot() {
  local n="$1"
  "$BOT" --host 127.0.0.1 --port "$PORT" --nick "Bot$n" --verbose \
    >>"$OUT_DIR/bot$n.log" 2>&1 &
  BOT_PIDS[$n]=$!
  echo "$(date +%H:%M:%S) START Bot$n pid=${BOT_PIDS[$n]}" \
    >>"$OUT_DIR/bot${n}_runs.log"
}

cleanup() {
  local rc=$?
  [[ -n "${SERVER_PID:-}" ]] && kill "$SERVER_PID" 2>/dev/null || true
  for n in $(seq 1 "$NUM_BOTS"); do
    [[ -n "${BOT_PIDS[$n]:-}" ]] && kill "${BOT_PIDS[$n]}" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  exit "$rc"
}
trap cleanup EXIT INT TERM

echo "Starting server on port $PORT (out: $OUT_DIR), running ${DURATION}s" >&2
"$SERVER" --server \
  --server-conf "$OUT_DIR/server.conf" \
  --server-log-hands "$OUT_DIR/hands.jsonl" \
  --server-log-game-results "$OUT_DIR/game_results.md" \
  >"$OUT_DIR/server.log" 2>&1 &
SERVER_PID=$!

sleep 1

for n in $(seq 1 "$NUM_BOTS"); do
  spawn_bot "$n"
  sleep 0.3
done

END_TS=$(($(date +%s) + DURATION))
NEXT_DISCONNECT=$(($(date +%s) + RANDOM % 61 + 60))   # first 60-120 s

# Disconnect loop: while we're still inside DURATION, when the next disconnect
# time arrives, pick a random bot, kill it, sleep 5-15 s, respawn it.  Other
# bots and the server keep running across the disconnect.
while [[ $(date +%s) -lt $END_TS ]]; do
  now=$(date +%s)
  if [[ $now -ge $NEXT_DISCONNECT ]]; then
    victim=$((1 + RANDOM % NUM_BOTS))
    gap=$((RANDOM % 11 + 5))   # 5-15 s offline
    echo "$(date +%H:%M:%S) DISCONNECT Bot$victim pid=${BOT_PIDS[$victim]} gap=${gap}s" \
      >>"$OUT_DIR/disconnect_events"
    kill "${BOT_PIDS[$victim]}" 2>/dev/null || true
    wait "${BOT_PIDS[$victim]}" 2>/dev/null || true
    sleep "$gap"
    spawn_bot "$victim"
    echo "$(date +%H:%M:%S) RECONNECT Bot$victim pid=${BOT_PIDS[$victim]}" \
      >>"$OUT_DIR/disconnect_events"
    NEXT_DISCONNECT=$((now + RANDOM % 61 + 60))
  fi
  sleep 5
done

echo "Stopping" >&2
for n in $(seq 1 "$NUM_BOTS"); do
  kill "${BOT_PIDS[$n]}" 2>/dev/null || true
done
sleep 1
kill "$SERVER_PID" 2>/dev/null || true
wait 2>/dev/null || true
trap - EXIT
BOT_PIDS=()
SERVER_PID=

hands=0
[[ -f "$OUT_DIR/hands.jsonl" ]] && hands=$(wc -l <"$OUT_DIR/hands.jsonl")
echo "Logged $hands hand(s) → $OUT_DIR/hands.jsonl" >&2
echo "$OUT_DIR"
