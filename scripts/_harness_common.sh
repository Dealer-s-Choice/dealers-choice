# Shared setup for the local-server bot harnesses
# (run_bot_session.sh, disconnect_test.sh).  Source this from a bash
# script; it defines a few functions and exports common runtime env.
#
# Usage:
#   . "$(dirname "$0")/_harness_common.sh"
#   dc_locate_binaries           # sets REPO_ROOT, BUILD_DIR, SERVER, BOT
#   dc_write_server_conf <path> <port> <password>
#   dc_export_runtime_env <out_dir>

# Resolve the repo root and the dealers-choice / dealers-choice-bot
# binaries built under _build_dealers_choice.  Aborts with a hint if
# the build is missing.
dc_locate_binaries() {
  REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[1]}")/.." && pwd)"
  BUILD_DIR="$REPO_ROOT/_build_dealers_choice"
  SERVER="$BUILD_DIR/dealers-choice-server"
  BOT="$BUILD_DIR/dealers-choice-bot"
  if [[ ! -x "$SERVER" || ! -x "$BOT" ]]; then
    echo "error: build first ('meson compile -C _build_dealers_choice')" >&2
    exit 1
  fi
}

# Write a server.conf to the given path that mirrors data/server.conf
# but with the short end_of_game_timeout the harnesses want (so runs
# aren't padded with idle waits between hands), a password set (bots
# refuse to connect without one), and a higher
# max_connections_per_minute so disconnect/reconnect cycles don't trip
# rate limiting.
dc_write_server_conf() {
  local path="$1" port="$2" password="$3"
  cat >"$path" <<EOF
bind_address = 127.0.0.1
port = $port
end_of_game_timeout = 5
action_timeout = 30
action_timeout_max = 3
dealer_timeout = 30
ante = 50
bringin_amount = 50
starting_coins = 20000
max_raises = 3
bet_amounts = list, 100, 250, 500
password = $password
max_connections_per_minute = 60
max_connections_per_ip = 0
EOF
}

# Export the runtime env both harnesses need:
#   - DEALERSCHOICE_DATADIR  (server needs data/ to start)
#   - LD_PRELOAD             (sanitized builds need libasan + libubsan,
#                             combined with libstdbuf so the
#                             _STDBUF_* line-buffering trick keeps
#                             working — stdbuf itself uses LD_PRELOAD,
#                             so anything we add must include it)
#   - _STDBUF_O / _STDBUF_E  (force line-buffering on the server and bots)
#   - ASAN_OPTIONS / UBSAN_OPTIONS (route sanitizer reports to the
#                             output dir so a crash is visible
#                             post-run)
#
# Callers should already have set OUT_DIR.
dc_export_runtime_env() {
  local out_dir="$1"
  export DEALERSCHOICE_DATADIR="$REPO_ROOT/data"

  local preload_libs=()
  if grep -q -- "-fsanitize=address" "$BUILD_DIR/compile_commands.json" 2>/dev/null; then
    local asan_lib ubsan_lib
    asan_lib="$(gcc -print-file-name=libasan.so 2>/dev/null)"
    ubsan_lib="$(gcc -print-file-name=libubsan.so 2>/dev/null)"
    [[ -e "$asan_lib" ]] && preload_libs+=("$asan_lib")
    [[ -e "$ubsan_lib" ]] && preload_libs+=("$ubsan_lib")
  fi
  for stdbuf_lib in /usr/lib/coreutils/libstdbuf.so \
                    /usr/libexec/coreutils/libstdbuf.so \
                    /usr/lib64/coreutils/libstdbuf.so; do
    if [[ -e "$stdbuf_lib" ]]; then
      preload_libs+=("$stdbuf_lib")
      break
    fi
  done
  if [[ ${#preload_libs[@]} -gt 0 ]]; then
    export LD_PRELOAD="$(IFS=:; echo "${preload_libs[*]}")"
  fi

  export _STDBUF_O=L _STDBUF_E=L
  export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=0:halt_on_error=0:log_path=$out_dir/asan}"
  export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:log_path=$out_dir/ubsan}"
}
