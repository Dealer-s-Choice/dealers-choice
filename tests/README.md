
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
`--server-log-hands`.  `analyze_hands.py` independently ranks every
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
- `hands.jsonl` — `--server-log-hands` output
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

Stress tcpme by simulating WAN conditions on the loopback interface,
then run any of the harnesses above unchanged.

### Setup

    sudo tc qdisc add dev lo root netem delay 75ms 15ms

That's 75 ms ± 15 ms loopback latency.  Verify:

    tc qdisc show dev lo
    ping -c 5 127.0.0.1   # should report ~75 ms RTT

For added stress with packet loss:

    sudo tc qdisc change dev lo root netem delay 75ms 15ms loss 0.5%

0.5 % loss exercises TCP retransmits and partial-read paths without
making the link so chaotic that real bugs hide under transport noise.

### Cleanup (important — affects everything else using lo)

    sudo tc qdisc del dev lo root

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

(The `### What it stresses` section below belongs to the netem block
above — keep that in mind when adding more profiling content here.)

### What netem stresses

- Connection handshake (SYN/ACK + DCPROTO header + nonce auth) under
  realistic latency — exposes any code path that assumed instant
  loopback response.
- `recv_all_tcp` loop behavior when reads come back partial.
- Action / dealer timeout budgets — 30 s defaults survive 75 ms RTT
  fine, but combining with loss and many round-trips can push them.

## Troubleshooting

### Port already in use

Each network test binds a dedicated TCP port (assigned sequentially
from `base_port` in `tests/meson.build`) so tests can run in
parallel.  If a test server process was left running after a crash or
interrupt, its port will still be occupied and the next test run will
fail on that test.  The other tests are unaffected.

Kill the leftover process:

    pkill -f "dealers-choice.*--server"

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
