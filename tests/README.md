
# Testing dealer's choice

There are three layers of tests against a built tree, all rooted at
`_build_dealers_choice/`:

1. **Meson test suite** — unit tests and game-logic tests, run via `meson test`.
2. **Offline harnesses in `scripts/`** — local server + N bots, useful for
   ad-hoc exploration and stress tests that don't fit a single `meson test`
   invocation.
3. **Network impairment via `tc netem`** — wraps any of the above and stresses
   the tcpme transport layer under realistic WAN conditions.

Plus the `src/pokeval/tests/` suite, which runs as part of the pokeval
subproject and gets exercised whenever you run `meson test` from the
parent tree.

## Meson test suite

    meson test -C _build_dealers_choice -v

Two groups (see `tests/meson.build`):

- **Unit tests** — compiled C binaries that exercise specific functions
  in isolation. Includes `serialization`, `get_next_player`,
  `debug_print_cards`, `layout_cards`, `no_peek`, `rate_limit`,
  `pokeval_fuzz` (a Python wrapper around the offline fuzzer; see
  below), plus everything under `src/pokeval/tests/`.
- **Game-logic tests** — Python harness (`game_logic.py`) that spawns a
  server binary plus C test clients over TCP and exercises full hand
  flows. Each test gets a unique TCP port derived from `base_port =
  22778` so they parallelize cleanly.

### Single test or suite

    meson test -C _build_dealers_choice -v test_serialization
    meson test -C _build_dealers_choice -v --suite test_NAME

### Faster game-logic runs

The game-logic tests default to 3 hand-rotation passes each.  For
faster iteration drop to 1 pass:

    meson test -C _build_dealers_choice -v --setup One_pass

### Pokeval fuzz at higher volume

`meson test` runs `test_pokeval_fuzz` at the default 250 hands per
seed (chosen to stay under the s390x-under-QEMU timeout in CI).  For
heavier offline coverage, drive the binary directly:

    DEALERSCHOICE_DATADIR=$PWD/data \
      _build_dealers_choice/tests/test_pokeval_fuzz 10000 1 > hands.jsonl
    scripts/analyze_hands.py hands.jsonl

The fuzzer cycles through every variant (5-card draw / showdown / stud
{5,6,7}-card / California lowball / Texas Hold'em / Omaha / 7-card
no-peek, each in non-wild and where applicable wild form) and writes
one JSON object per hand in the same format the server emits via
`--log-hands`.  `analyze_hands.py` independently ranks every
hand and asserts pokeval's declared winner matches.

Override the count and seed via env vars when running through meson:

    DC_FUZZ_N=2000 DC_FUZZ_SEED=42 meson test -C _build_dealers_choice -v test_pokeval_fuzz

## Local server + bots harnesses

Two shell scripts in `scripts/` bring up a real server with bot
clients connected, write per-bot decision logs, and emit the same
hand-log JSON the meson fuzz consumes.  Useful for watching how the
bot decides under different conditions, or for stressing the server
with traffic the unit tests don't generate.

### `scripts/run_bot_session.sh` — three-bot session

    scripts/run_bot_session.sh [duration_seconds] [output_dir]

Defaults: 90 seconds, output under
`./bot_session.<timestamp>/`.  Writes:

- `server.conf` — the per-run server config (short
  `end_of_game_timeout`, password set)
- `server.log` — server stdout/stderr
- `bot{1..3}.log` — verbose bot decision traces
- `hands.jsonl` — `--log-hands` output
- `game_results.md` — markdown game log

Pipe `hands.jsonl` to `scripts/analyze_hands.py` after the run to
cross-check showdown winners against the independent evaluator.

### `scripts/disconnect_test.sh` — four-bot disconnect / reconnect stress

    scripts/disconnect_test.sh [duration_seconds] [output_dir]

Default: 3600 s.  Spawns 4 bots, then every 60–120 s kills a random
bot and respawns it after a 5–15 s gap.  Same output files as
`run_bot_session.sh` plus `disconnect_events` (timestamped log of
every cycle) and `bot{N}_runs.log` (timestamped log of every spawn,
so you can correlate bot logs across reconnects).

What it's checking, post-run:

- Any `asan.<pid>` / `ubsan.<pid>` files in the output dir? Server
  hit a sanitizer-flagged bug.
- Reconnect success rate: number of `Connected to` lines in each
  bot log should match the number of starts in its `bot{N}_runs.log`.
- `scripts/analyze_hands.py hands.jsonl` — pokeval winner
  disagreements.
- Server log: `tcpme_send failed`, `Failed to send`, `Invalid`,
  `Rejected` should all be zero for a clean run.

### Common harness internals — `scripts/_harness_common.sh`

Both scripts source this for binary lookup, `server.conf` template,
and the LD_PRELOAD juggling needed when the build has sanitizers on
(libasan / libubsan must be preloaded, but stdbuf for line-buffered
logs uses LD_PRELOAD too, so they have to be combined).

## Network impairment via `tc netem`

Stress tcpme by simulating WAN conditions on the loopback interface.
`scripts/netem.sh` wraps the `tc` commands (needs sudo):

    scripts/netem.sh on 60 12 0.5   # 60 ms +/- 12 ms latency, 0.5% loss
    scripts/netem.sh status
    scripts/netem.sh off            # IMPORTANT: affects all loopback traffic

0.5 % loss exercises TCP retransmits and partial-read paths without
making the link so chaotic that real bugs hide under transport noise.

### What to run under impairment

Pair netem with the **soak** (`scripts/soak.sh`) — real bots on normal
30 s timeouts that tolerate latency:

    scripts/netem.sh on 60 12 0.5
    DC_BUILD=$PWD/_build_asan scripts/soak.sh   # judge by the log
    scripts/netem.sh off

Do **not** run the deterministic `meson test` game-logic tests (or
`scripts/flaky-loop.sh`) under netem: they run in `DC_TEST` mode with
deliberately short server timeouts, so added latency makes the server
time out the lockstep client and disconnect it — 100 % spurious
failures, not real bugs. Use `flaky-loop.sh` for flushing timing flakes
under *CPU* load (run it alongside the soak), not network impairment.

### The manual `tc` commands

    sudo tc qdisc add dev lo root netem delay 75ms 15ms        # add latency
    tc qdisc show dev lo
    ping -c 5 127.0.0.1                                         # ~75 ms RTT
    sudo tc qdisc change dev lo root netem delay 75ms 15ms loss 0.5%  # + loss
    sudo tc qdisc del dev lo root                              # cleanup

### What netem stresses

- Connection handshake (SYN/ACK + DCPROTO header + nonce auth) under
  realistic latency — exposes any code path that assumed instant
  loopback response.
- `recv_all_tcp` loop behavior when reads come back partial.
- Action / dealer timeout budgets — 30 s defaults survive 75 ms RTT
  fine, but combining with loss and many round-trips can push them.

## Profiling memory allocations with heaptrack

When investigating allocation hot paths in the GUI client (e.g. the
per-frame card text-texture allocations that motivated the
`card_text_atlas` refactor), use heaptrack against a non-sanitized
build:

    sudo pacman -S heaptrack                 # Arch / Manjaro — heaptrack_gui
                                             # ships in the same package
    # Debian / Ubuntu may split the GUI into a separate heaptrack-gui
    # package — adjust accordingly.
    meson setup _build-release -Db_sanitize=none --buildtype=debug
    meson compile -C _build-release

Then under heaptrack:

    heaptrack ./_build-release/dealers-choice --port 23999 --verbose

Play the scenario you want to profile and quit cleanly (the X button
or ESC, not `kill`).  heaptrack writes
`heaptrack.dealers-choice.<pid>.zst` in the current directory.  Open
it interactively:

    heaptrack_gui heaptrack.dealers-choice.<pid>.zst

Or text summary:

    heaptrack_print heaptrack.dealers-choice.<pid>.zst | head -40

Useful things to look at:

- "MOST CALLS TO ALLOCATION FUNCTIONS" — top callstacks by allocation
  count.  Per-frame work shows up here even when peak memory is fine.
- The footer's "calls to allocation functions" rate (calls/s).
  Anything above ~5k/s for a 2D card game is suspect.
- "total memory leaked" — non-zero usually means
  allocate-once-and-forget, not always a bug (think singletons), but
  worth checking the callstacks.

heaptrack is **not** part of `meson test` (it requires a real GUI
session) and **not** a checkdepends; treat it as a manual investigation
tool.

## Sanitized build for manual / live verification

Running the server + a bot (and/or the GUI client) to play real hands should use
a sanitized build, so memory errors / UB surface during play with a full
ASan/UBSan report instead of a bare SIGABRT:

    CC=clang CFLAGS="-fno-sanitize-recover=all" meson setup _build_asan \
      -Db_sanitize=address,undefined -Db_lundef=false -Dgen_protobuf=true

Run the binaries with
`ASAN_OPTIONS` / `UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1`.
Line-buffer the verbose log so it doesn't lag behind the game (stdio
block-buffers to a file):

    stdbuf -oL -eL ./dealers-choice-server --verbose > log 2>&1

For a fully-automated repro, run **two bots** (they auto-deal/play) instead of a
manual GUI client.

### gcc -Werror before calling a branch CI-ready

The ASan build above uses **clang**, but the CI gate compiles with **gcc
`-Werror`**, which emits warnings clang stays silent on (e.g.
`-Wformat-truncation`). A branch clean under clang/ASan can still red the gate.
Do a quick gcc pass first:

    CC=gcc CFLAGS=-Werror meson setup _build_gcc -Dgen_protobuf=true
    meson compile -C _build_gcc

### scripts/soak.sh — phased sanitizer soak (run after server-side changes)

`scripts/soak.sh` drives a sanitized server + bots through sustained play, bot
churn, and the 0-player showdown path, with ASan/UBSan armed to abort on the
first violation. `DC_REPO` defaults to the repo root, so it works from any
clone/worktree. Override the phase lengths for a ~5-minute run:

    DC_BUILD=$PWD/_build_asan DC_PORT=24770 DC_PASSWORD=x \
      P1_BOTS=2 P1_MIN=3 P2_BOTS=3 P2_MIN=2 P3_ROUNDS=3 \
      LOG=/tmp/soak.log bash scripts/soak.sh

**Verify by reading the log, not the exit code.** A real pass ends with
`DONE: all phases passed`. (The script has reported exit 0 while the server
never started, and backgrounding it can surface a harmless non-zero code.)
Check `/tmp/soak.log` and grep the `SRVLOG` for `runtime error` /
`AddressSanitizer`.

### scripts/flaky-loop.sh — flush timing flakes under CPU load

`scripts/flaky-loop.sh [test ...]` re-runs named meson tests under ASan many
times, classifying each failure as a TIMEOUT (recv hang) vs ASSERTION
(turn-order desync). Run it alongside `soak.sh` so the tests execute under CPU
load — the documented trigger for the flaky 90 s recv-hangs (#282). `ROUNDS`
defaults to 40; default tests are `test_raises test_check`.

    ROUNDS=40 bash scripts/flaky-loop.sh test_raises test_check

Do **not** run it under `tc netem` — DC_TEST's short timeouts make latency look
like failures (see the netem section).

## Running the binaries directly

When running binaries by hand (not via `meson test` / `game_logic.py`):

- **Data dir:** non-Windows resolution is `../data` relative to the **cwd**
  (`util.c:get_data_dir`), not the binary. So `cd _build_asan && ./dealers-choice`
  finds `data/`; running `./_build_asan/dealers-choice` from the repo root fails
  with "Unable to find data". Either `cd` into the build dir or set
  `DEALERSCHOICE_DATADIR=$PWD/data`. (The `../data` lookup is a pre-release hack.)
- **Bots need a password, not `---test`:** `dealers-choice-bot` only joins a
  server that has a password set — export `DC_PASSWORD` for the server *and* each
  bot. Do **not** use `---test` (it skips the server's auth handshake while the
  bot still performs it, desyncing the connection; the server logs "Invalid
  message size"). `---test` is for the C test-client in `game_logic.py`, not the
  bot binary.
- **`gen_protobuf` is a manual target:** `-Dgen_protobuf=true` only adds a
  `gen-proto` run-target; it does not regenerate on a `.proto` change. After
  editing `dc_protocol.proto`, run `meson compile -C _build_… gen-proto` first.

## Screenshotting the GUI (X11)

The SDL window is titled "Dealer's Choice" but `xdotool search --name` for it is
unreliable (the apostrophe breaks the regex; the window isn't matchable right
after launch). Don't capture the root window — that grabs unrelated windows.
Capture only the DC window via a before/after window-list diff:

    xdotool search "" | sort > /tmp/wb.txt    # windows before launch
    # launch the GUI backgrounded, then sleep ~6s for the connect screen
    xdotool search "" | sort > /tmp/wa.txt     # windows after
    for w in $(comm -13 /tmp/wb.txt /tmp/wa.txt); do
      case "$(xdotool getwindowname "$w")" in
        *ealer*) import -window "$w" /tmp/dc_shot.png; break;;
      esac
    done

For a throwaway run that won't touch your real config, set
`XDG_CONFIG_HOME=/tmp/somedir` (fresh `player.conf`) and
`DEALERSCHOICE_DATADIR=$PWD/data`; `--disable-audio` skips the audio threads.

## Troubleshooting

### Port already in use

Each network test binds a dedicated TCP port (assigned sequentially
from `base_port` in `tests/meson.build`) so tests can run in
parallel.  If a test server process was left running after a crash or
interrupt, its port will still be occupied and the next test run will
fail on that test.  The other tests are unaffected.

**Parallel worktrees collide:** the unique-port scheme is per *run*, so two
worktrees running the suite at the same time reuse the same ports and fail with
`tcpme_listen: Address already in use`. Give each worktree a distinct `DC_PORT`
base (the harness honours `DC_PORT`) or serialize the game-logic phase across
worktrees.

Kill the leftover process:

    pkill -f dealers-choice-server

Or look it up first:

    ss -tlnp | grep <port>

On Windows:

    netstat -ano | findstr <port>
    taskkill /PID <pid> /F

### Leftover harness processes

`run_bot_session.sh` and `disconnect_test.sh` clean up their children
on normal exit and SIGTERM, but a hard kill can leave bots running.
Same cleanup as above; `pkill -f "dealers-choice"` catches both the
server and bot processes.

### Sanitizer reports

When the build is configured with `b_sanitize=address,undefined`
(default for `meson setup _build_dealers_choice`), both the meson
test suite and the harness scripts route ASan / UBSan reports
through `ASAN_OPTIONS=log_path=…` so a crash shows up as a file in
the output directory rather than getting lost in interleaved stdout.
Always check for `asan.*` / `ubsan.*` files in the run's output
directory.
