# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Run from the `dealers_choice/` directory:

```sh
meson setup _build_dealers_choice --reconfigure -Dgen_protobuf=true
meson compile -C _build_dealers_choice
```

All `meson compile`, `meson test`, and `meson devenv` commands must use `-C _build_dealers_choice`. Never use `_build` or `build`.

To update subprojects after upstream changes:

```sh
meson subprojects update
```

## Tests

```sh
meson test -C _build_dealers_choice -v
meson test -C _build_dealers_choice -v --setup One_pass   # single pass (faster)
meson test -C _build_dealers_choice -v --suite test_NAME  # single test
```

Tests fall into two groups:
- **Unit tests** (`tests/serialization.c`, `get_next_player.c`, etc.) — compiled C executables.
- **Game-logic tests** (`tests/game_logic.py`) — Python harness that spawns a server binary + bot client(s) over TCP; each gets a unique port derived from `base_port = 22778` so they run in parallel safely.
  - **Parallel worktrees collide on these ports.** The unique-port scheme is per *run* — two worktrees (e.g. parallel subagents) running the suite at once reuse the same ports (22782/3/5…) and fail with `tcpme_listen: Address already in use`. Give each worktree a distinct `DC_PORT` base (the harness honours `DC_PORT`) or serialize the game-logic test phase across worktrees.

deckhandler and pokeval are built in-tree (`src/deckhandler`, `src/pokeval`), so their tests run in the normal `_build_dealers_choice` suite — under the `deckhandler` and `pokeval` suite names (`meson test -C _build_dealers_choice --suite pokeval`).

**Manual / live verification of a fix** — running the server + a bot (and/or a
GUI client) to play real hands — should use a **sanitized build**:

```sh
CC=clang CFLAGS="-fno-sanitize-recover=all" meson setup _build_asan \
  -Db_sanitize=address,undefined -Db_lundef=false -Dgen_protobuf=true
```

so memory errors and UB surface during real play, and any recurrence of a
crash yields a full ASan/UBSan report instead of a bare SIGABRT. Run the
server/bot with `ASAN_OPTIONS`/`UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1`.
Also **line-buffer the verbose log** (`stdbuf -oL -eL ./dealers-choice-server
--verbose > log 2>&1`) — stdio block-buffers to a file, so without this the
server's verbose output lags and you can't watch the game flow in real time.
For a fully-automated repro run **two bots** (they auto-deal/play) instead of a
manual GUI client. To stress timing/network paths, add loopback latency per
`tests/README.md`: `sudo tc qdisc add dev lo root netem delay 75ms 15ms`
(remove with `... del dev lo root`).

**Verify with `gcc -Werror` before calling a branch CI-ready.** The ASan build
above uses **clang**, but the CI *gate* compiles with **gcc `-Werror`**, which
emits warnings clang stays silent on (e.g. `-Wformat-truncation`). A branch that
is clean under clang/ASan can still red the gate. Do a quick gcc pass before
pushing: `CC=gcc CFLAGS=-Werror meson setup _build_gcc && meson compile -C
_build_gcc`. (This bit a subagent branch once — clang-clean, gate-red on a
truncation warning.)

**Reusable test/dev tooling lives in `~/src/DealersChoice/tools/`** (outside
the repo — local aids, not project source). Save any new reusable scripts
there rather than scattering them in `/tmp`; existing ones are `soak.sh` and
`flaky-loop.sh` (see that dir's `README.md`).

**Canonical launcher:** `~/src/DealersChoice/tools/soak.sh` already runs a
sanitized server + bots correctly (sets `DC_PASSWORD`, no `---test`, exports
`DEALERSCHOICE_DATADIR`) — use/adapt it instead of hand-rolling a launch
script. The gotchas below are why it does each of those things.

**Gotchas when running the binaries directly** (i.e. not via `meson test` /
`game_logic.py`):
- **Data dir:** non-Windows resolution is `../data` relative to **cwd**
  (`util.c:get_data_dir`), *not* relative to the binary (that path is
  Windows-only). So `cd _build_asan && ./dealers-choice …` finds `data/` on
  its own; running `./_build_asan/dealers-choice …` from the repo root fails
  with "Unable to find data" (it looks for `../data` = `DealersChoice/data`,
  one level too high). Either `cd` into the build dir, or set
  `DEALERSCHOICE_DATADIR=$PWD/data`. (The `../data` lookup is a pre-release
  temporary hack — see the comment in `get_data_dir`.)
- **Bots need a password, not `---test`:** `dealers-choice-bot` only joins a
  server that has a password set — export `DC_PASSWORD=…` for the server
  *and* each bot. Do **not** use `---test`: test mode skips the server's
  auth handshake while the bot always performs it, desyncing the connection
  (server logs `[NET] Invalid message size …`). `---test` is for the C
  test-client in `game_logic.py`, not the bot binary. (`docs/BOT.md` states
  the password requirement.)
- **`gen_protobuf` is a manual target:** `-Dgen_protobuf=true` only adds a
  `gen-proto` run-target; it does not regenerate on `.proto` change. After
  editing `dc_protocol.proto`, run `meson compile -C _build_… gen-proto`
  before the normal compile.

**Screenshotting the GUI (X11, for verifying a visual change):** the SDL window
is titled "Dealer's Choice" but `xdotool search --name` for it is unreliable
(the apostrophe breaks the regex, and the window isn't matchable right after
launch). Do **not** capture the root window — that grabs your other windows.
Capture only the DC window via a before/after window-list diff:

```sh
xdotool search "" | sort > /tmp/wb.txt          # windows before launch
# launch the GUI backgrounded, then sleep ~6s for the connect screen to render
xdotool search "" | sort > /tmp/wa.txt           # windows after
for w in $(comm -13 /tmp/wb.txt /tmp/wa.txt); do
  case "$(xdotool getwindowname "$w")" in
    *ealer*) import -window "$w" /tmp/dc_shot.png; break;;
  esac
done
```

For a throwaway GUI run that won't touch your real config, set
`XDG_CONFIG_HOME=/tmp/somedir` (it writes a fresh `player.conf` there) and
`DEALERSCHOICE_DATADIR=$PWD/data`; `--disable-audio` skips the audio threads.

## Architecture

**Client-server** networked game, split across three binaries that talk over
TCP: the GUI client (`dealers-choice`), a headless server
(`dealers-choice-server`), and a headless bot (`dealers-choice-bot`). The GUI is
a **client only** — it no longer hosts a server (the old `--server` mode is a
deprecation stub; see "headless server binary name" below). Sources live under
`src/{game,net,server,ui}/` by area, with the SDL-free logic in `libdc_core`.

### Static library chain

Meson builds the local libs bottom-up:

1. **tcpme_lib** (`src/tcpme/`) — cross-platform TCP socket abstraction (POSIX + Winsock). Exposed as `tcpme_dep` and merged into `shared_dep`. Note: tcpme was written almost entirely by an LLM, which is a potential concern for correctness and security.
   - **Keep tcpme generic and DC-agnostic.** It is meant to be a standalone, reusable library (SDL_net-style), so it provides *mechanism* only; DC provides *policy*. Do not put DC-specific values, constants, comments, or assumptions in `src/tcpme/`. Example: socket I/O timeouts — tcpme exposes `tcpme_set_timeout(sock, ms)` (the generic primitive); DC owns the value (`SOCKET_IO_TIMEOUT_MS` in `net.h`) and applies it after connect/accept. When a fix needs a tunable or a project-specific decision, expose a knob in tcpme and set it from DC, don't hardcode it in the library.
2. **`libdc_core`** (`dc_core_dep`) — the **SDL-free** core: game rules, protocol/serialization, and the server engine (`common_src` = `src/{game,net,server}` + core root files). No SDL, ttf, image, or audio. Linked by all three binaries.
3. **`_lib` ("game")** (`game_dep`) — the GUI library: rendering, widgets, menus (`gui_src` = `src/ui/` + `widgets_src`), adding SDL2/ttf/image + miniaudio on top of `dc_core_dep`.

Binaries: `dealers-choice` = `main.c` + `game_dep`; `dealers-choice-bot` = `bot.c` + `dc_core_dep`; `dealers-choice-server` = `src/server/server_main.c` + `dc_core_dep`.

**Why the split:** the bot and server binaries must link **no SDL at all**, so anything pulling in SDL/ttf/image/audio belongs in `src/ui/` (the GUI lib), never in `libdc_core`. When adding a source file, decide: shared game/protocol/server logic → an area dir under `src/{game,net,server}`; GUI-only → `src/ui/` (`ui_src`) or `src/widgets/`.

### Key source files

| File | Role |
|---|---|
| `src/main.c` | GUI entry point: SDL init, top-level menu, connection loop |
| `src/cli.c` | Shared CLI parsing — `parse_cli_args` (client) + `parse_server_args` (server) |
| `src/ui/client.c` | GUI client: connection lifecycle, audio threads, SDL setup/teardown |
| `src/ui/game_logic.c` | Gameplay-screen render + input loop (`handle_game_logic`) |
| `src/ui/game_select.c` | Pre-game lobby / game-selection screen |
| `src/ui/menus.c` | Connect / settings / hotkeys menu screens |
| `src/server/server.c` | Server run loop, lobby, client management, messaging |
| `src/server/round.c` | Betting / draw / showdown engine |
| `src/server/variants.c` | Poker variants + per-hand play orchestration (`play_game`) |
| `src/server/server_main.c` | Headless `dealers-choice-server` entry point |
| `src/net/net.c` | Protobuf-c serialization/deserialization, magic-byte framing |
| `src/game/game.c` | Poker variant definitions + shared game model |
| `src/bot.c` | Headless bot (links `libdc_core` only) |
| `src/dc_config.c` | canfigger-based config file handling |
| `src/types.h` | Canonical type definitions (`Player_t`, `GameState_t`, `EPlayerAction_t`, etc.) |

### Networking

- Protocol: custom binary framing ("DCPROTO" magic + version + flags) wrapping protobuf-c messages.
- Version constant: `GAME_PROTOCOL_VERSION` in `src/net/net.h`. **Andy owns this bump — never change it yourself.** He bumps it whenever the wire protocol changes (a new game/opcode, a new field in the protobuf schema, framing changes), not on a fixed per-release schedule; expect it to change less often as the protocol stabilizes.
- Default port: 22777.
- Auth: libsodium-based password hashing.

### Audio

miniaudio is used for sound effects. Both `ma_engine_init` and `ma_engine_uninit` can block for several seconds when a sound server (e.g. PulseAudio) is slow or a USB audio device is involved. Both run on background SDL threads (`audio_init_thread_fn` / `audio_uninit_thread_fn` in `src/ui/client.c`) while the main thread pumps events to keep the window responsive. Do not move either call back to the main thread.

#### Audio device-change handling — in-progress work

**Confirmed fixed:** When the user switches audio devices mid-session, `ma_device_uninit` used to block forever because the PA device worker thread was stuck in `pa_mainloop_iterate(block=1)`. Fix: `audio_uninit_thread_fn` calls `ma_engine_stop` before `ma_engine_uninit`; this routes through `ma_device_data_loop_wakeup__pulse` → `pa_mainloop_wakeup`, breaking the blocked iterate. Do not revert this.

**Unresolved: no sound after device switch / on startup.**

Two possible root causes have been identified — they may both be in play:

1. **Marginal USB port putting the C-Media sink in a bad state.** The C-Media USB audio chip (0d8c:0012) was enumerating cleanly (`dmesg` showed no errors, `power/control = on`) but PA's sink was going `SUSPENDED` and not resuming correctly, causing DC and browser video to stall when trying to open audio streams. Switching the headset to a different USB port fixed it immediately; switching back to the original port continued to work. The port had a marginal connection that was good enough to enumerate but caused intermittent PA sink state corruption.

   **System config in place** (good hygiene regardless of root cause):
   - `restore_device=false` in `~/.config/pulse/default.pa` — prevents module-stream-restore from pinning streams to a remembered sink
   - `module-suspend-on-idle` commented out in `~/.config/pulse/default.pa` — prevents PA from auto-suspending idle sinks (note: `timeout=0` means "suspend immediately", not "never suspend"; commenting out the module entirely is the correct way to disable auto-suspend)

2. **`module-stream-restore` sink routing** — ruled out. `restore_device=false` is confirmed active (`pactl list modules short | grep stream-restore` shows `restore_device=false`). The config is in `~/.config/pulse/default.pa` and is being picked up. This is no longer a suspect.

**Current state of the restart code in `src/ui/client.c`:** There is substantial `audio_restart_thread_fn` / `maybe_restart_audio` / `g_audio_missed_notification` machinery that attempts a full engine+sounds uninit/reinit when PA sends `rerouted` or `stopped` notifications. This code is unreliable against active `module-stream-restore` (PA moves the new stream again immediately after reinit) and has introduced its own bugs (cleanup hang when `ma_engine_init` blocks in the restart thread). Consider stripping this back to the freeze-fix-only state once the root cause is better understood.

### Subprojects

`canfigger` and `miniaudio` are Meson subprojects under `subprojects/` (canfigger via a pinned `.wrap`); changes to them are committed in their own upstreams, not the main repo. `pokeval` and `deckhandler` were moved **in-tree** (`src/pokeval`, `src/deckhandler`) as of 0.0.14 — they are **not** subprojects, and their upstream repos are archived, so edit and commit them directly in the main repo.

### SDL3_net — deferred (branch `use-sdl3_net`)

SDL3_net was evaluated as a replacement/supplement for tcpme. It has async DNS, dual-stack, and interface binding — all things SDL2_net lacks. However, SDL3 itself (a hard dependency of SDL3_net) is not packaged in Ubuntu (not in Jammy, Noble, or any backport) because it was only released January 2025. This makes it impractical for packagers and end users building from source. **tcpme already covers all the needed functionality**, so SDL3_net was set aside. Revisit when major distros ship SDL3.

## Styling system — `style.h` / `style.c`

> **STATUS (2026-06-19): planned, NOT yet implemented.** On trunk, `src/ui/style.c`
> is ~53 lines of hardcoded `SDL_Color` constants; there is no `[styles]` section in
> `data/layout.conf`, no `StyleConfig_t`/`g_style_cfg`/`get_style_config`/`style_fields`,
> and no `parse_color`. The design below is the target (task #66) — do not assume these
> symbols exist.

Widget colors and font choices are configured in the `[styles]` section of `layout.conf` and parsed at startup into `g_style_cfg`. **Do not use libcss or any external CSS library** — CSS is designed for flow-based document layout and does not map to an absolute-positioned SDL2 UI.

### Files

| File | Role |
|---|---|
| `src/ui/style.h` | `StyleConfig_t` typedef, `extern StyleConfig_t g_style_cfg`, `get_style_config()` declaration |
| `src/ui/style.c` | `get_style_config()` implementation, `parse_color()`, `parse_font()`, offset-table dispatch |
| `data/layout.conf` `[styles]` | Per-role color/font values read at startup |

`style.h` is included via `globals_gui.h` (the GUI umbrella header), so it is available everywhere `globals_gui.h` is included. (`globals.h` itself is SDL-free since the renovation and no longer pulls in `style.h`.) Files outside that chain (e.g. `graphics.c`, `widgets/ping.c`) include `style.h` directly.

### `StyleConfig_t` fields

Color-pair roles (each has `.bg` and `.fg`): `button_primary`, `button_danger`, `button_warn`, `button_cancel`, `indicator_wild`, `indicator_game`.

Single-color roles: `text_on_dark`, `text_on_light`, `text_muted`, `link_normal`, `link_hover`, `timer_bg`, `timer_elapsed`.

Font roles (store `FONT_*` index): `button_font`, `link_font`.

### `layout.conf` format

Color-pair keys use canfigger's attribute syntax — value is the bg color, first comma-separated attribute is the fg color:

```ini
button_primary = black, yellow
indicator_wild = purple, white
```

Single colors and font names are plain values:

```ini
text_on_dark = white
timer_elapsed = #F0CC30
button_font = bold
```

Color strings: named (`white`, `black`, `table_green`, `orange`, `purple`, `brown`, …) or hex `#RRGGBB` / `#RRGGBBAA`. Font strings: `bold`, `default`, `link`, `title`, `version`, `status_msg`, `card`, `default_bold`, `wild_select`.

### Dispatch — offset table in `style.c`

The parser uses a static `style_fields[]` table (key, `FieldType_t`, `offsetof`, default values). Adding a new style field is one table row — no `else if` chain to maintain.

### Widget creation

- `button_widget_create_colored(text, SDL_Color bg, SDL_Color fg, font, hotkey)` — SDL_Color entry point alongside the existing `EColor_t` version.
- `create_indicator_colored(text, font, SDL_Color bg, SDL_Color fg)` — likewise for indicators.
- Call sites pass `g_style_cfg.button_primary.bg` / `.fg` etc. directly.

### What stays out of config

- Card back pattern colors (intentional per-style literals in `card_back_styles[]`)
- Colors computed at runtime (animation tints, alpha blends, the circle timer's 3D border gradient)
- Logical dimensions (`LOGICAL_WIDTH` / `LOGICAL_HEIGHT`) — compile-time constants

## Code conventions

- **String duplication:** use `dc_strdup()` (`src/util.c`) in files that don't already include SDL headers. Use `SDL_strdup` only where SDL is already included. Never use POSIX `strdup` — it's unavailable on MSVC.
- **C standard:** C11 (`-std=c11`), warning level 3, with `-Werror` on aliasing, ODR, LTO type mismatches, and int conversion.
- **Portability:** code runs on Linux, macOS, Windows (MSVC), and BSD. Platform guards use `host_sys` / `_WIN32`.
- **i18n:** user-visible strings go through gettext macros (see `src/translate.h`). Translation `.po` files live in `po/`.
- **Comments are rare and short.** Default to none; add one only when the *why* is non-obvious (a hidden constraint, a prior bug, a surprising invariant) — not to restate what the code does. **Exception:** public API docs — doxygen/header docblocks on public functions, parameters, and return codes — can be as long as needed; that's contract documentation for callers, not noise.

## Working style & contributions

Applies to human contributors and to any LLM/AI assistant used on this project:

- **No sycophancy.** Don't agree just to be agreeable. Take an idea seriously, push back when something looks wrong, and propose the better alternative.
- **ChangeLog: one line per entry** (two only if a single line genuinely can't carry the meaning). The diff and the linked PR/issue hold the detail — don't restate the change.
- **LLM-drafted public text gets a short preface.** When an AI assistant writes content posted under a contributor's account — GitHub issues / PRs / comments, or a commit-message *body* (one-line subjects need none) — it should identify itself as an LLM (with model/version), note it was posted at that contributor's direction, write in its own voice (first person for what the tool did; the contributor's plain name — never an `@`-mention — for what they did), avoid anthropomorphic phrasing, and flag anything it hasn't verified. A contributor's own assistant config may set the exact format; this is just the project default.

## UI / SDL2 menus

SDL2 is a rendering/input layer, not a widget toolkit, so every screen (in
`src/ui/menus.c`, `game_select.c`, `game_logic.c`; `main.c` drives the
top-level loop) is a hand-rolled `while (running) { poll events; render; }` loop.
The `ui_widget`/`UIRegistry` abstraction (auto render/destroy) and
`button_widget_create_styled` soften it, but each menu still repeats a lot of
boilerplate. Two refactors worth doing once there are ~4–5 of these screens
(don't bolt on mid-feature):

- **Use the existing `ui_table_begin/add/layout` helper** (`ui_widget.h`) for
  row/column screens instead of hand-computing `x/y`. `menu_display_hotkeys`
  positions its label+box rows manually and is a candidate.
- **Shared "menu event loop" helper** to handle quit / Escape / back-arrow /
  F11 fullscreen once, instead of every screen copying that block.

A heavier framework (Dear ImGui, etc.) is **not** the answer — it fights the
absolute-positioned style and the `style.h` config system.

### Configurable game-play hotkeys (branch `claude/configurable-hotkeys`)

Action hotkeys (check/bet/fold/call/raise/complete/discard) live in
`player.conf` as `hotkey_*` string entries (`CFG_GROUP_HOTKEYS` in
`player_config_entries[]`), resolved to keycodes at startup by
`src/hotkeys.c` into `g_hotkey_cfg`. The grouping keeps them off the main
Settings grid; they're edited on a press-to-bind sub-screen
(`menu_display_hotkeys`) reached from a "Hotkeys" button on Settings.
Card-selection and bet-amount digit keys (`1`–`8`) are intentionally
hard-coded — they're positional, not semantic.

**Follow-up idea (not yet built): F1 in-game opens the hotkeys screen.**
Reusing `menu_display_hotkeys` for display is easy, but two caveats before
building it: (1) it's a *blocking* modal, so opening it mid-hand freezes the
table view while the server action timer keeps counting — risk of timing out
on your turn; (2) action buttons bake in their hotkey at creation
(`button_widget_create_styled(..., action_hotkey(i))`), so a mid-game rebind
won't take effect until the buttons are rebuilt unless the in-game key
handler is changed to read `g_hotkey_cfg` live. Prefer treating F1 as a
read-only reference overlay, with editing left to the Settings path — or do
the live-`g_hotkey_cfg` change if in-game editing is wanted.

## Known issues — open on branch `claude/automated-bot-testing`

Surfaced by `tests/test_pokeval_fuzz` (registered in `tests/meson.build`) and the harness in `scripts/disconnect_test.sh`. The fuzz test currently fails CI on this branch because it pins behavior that is still wrong; pick these up before merging.

### 1. Pokeval wild straight-flush / flush tie-breaks ~~(~2 mismatches per 1000 fuzz hands)~~ — FIXED

Fixed by introducing `compare_wild_same_rank` in `src/pokeval/pokeval.c`, which centralizes the wild-aware tie-break dispatch and is shared between `update_best_wild` (best-5-of-7 selection) and `POKEVAL_compare_hands_wild` (cross-player winner determination).  The old code had each call site fall back to `compare_high_cards` on the raw combo, which left wilds at `face_val == wild_face` and mismeasured anything past HIGH_CARD.  50,000 fuzz hands across 5 seeds now pass with 0 mismatches; `tests/pokeval_fuzz_check.py` runs the full variant set (no more `--skip-wild`).


`POKEVAL_compare_hands_wild` already has rank-aware tiebreaks for FIVE_OF_A_KIND / FOUR / FULL_HOUSE / THREE_OF_A_KIND / PAIR / TWO_PAIR (the last two added in commit `pokeval: wild-aware tie-break for PAIR and TWO_PAIR`).  STRAIGHT, STRAIGHT_FLUSH, and FLUSH still fall through to the `default:` case, which calls `compare_high_cards` on the raw combo — and the raw combo has wilds at `face_val = 2`.  When two flushes / straight flushes tie at the same rank, the wild card sorts to the bottom of the hand instead of being treated as its substituted face, so a Q-high wild SF can lose to a 5-high wheel SF (or a wild flush gets miscompared against a natural one).

Concrete failure case from the fuzz harness:
```
P1: Js 2h 9c 8c Tc 2c Qd   — Q-high straight flush in clubs (wilds = Jc, Qc)
P2: Kh 5h 2s 6s 4h Ah 2d   — 5-high wheel straight flush in hearts (wilds = 2h, 3h)
```
Pokeval picks P2 because `compare_high_cards` on the raw combos walks `10 9 8 2 2` vs `14 5 4 2 2` and `14 > 10`.

Fix sketch: add `POKEVAL_STRAIGHT`, `POKEVAL_STRAIGHT_FLUSH`, and `POKEVAL_FLUSH` cases to the `compare_hands_wild` switch.  For straights, substitute the wild into the missing-rung position(s) and compare the resulting straight-high values (handle the A-2-3-4-5 wheel the same way `update_best_5card` already does at line ~819).  For flush, substitute each wild to the dominant suit and pick the highest non-duplicate face for each, then compare high-down.

### 2. ~~Bot reconnect handshake silently hangs ~47% of the time~~ — RESOLVED

Originally observed on the 60-min disconnect harness: roughly half of bot reconnect attempts would print `Exchanging protocol information...` and stop, with no error and no follow-up `Connected to…`.  Re-running the same harness after the server stack-use-after-return fix (`server: clear starting_turn after play_game to avoid stack-use-after-return`) returned **44/44 successful reconnects** across 4 bots in 60 minutes.

Root cause was downstream of the UAR: when the server dereferenced freed stack memory in `args->starting_turn` during the post-game wait window, the corrupted state plausibly stalled `register_new_client` for queued accept-queue entries, so the bot would sit in `recv_all_tcp` waiting for the server's 1-byte ACK that never came.  No tcpme-level bug; the pattern disappears when the server doesn't reach into freed memory.

### 3. Bot wire format leaks ACE_HIGH (face_val = 14) when not using the GUI client

`POKEVAL_sort_hand` mutates aces from `DH_CARD_ACE` (1) to `POKEVAL_ACE` (14) in place so its straight detector sees them at the top of the sort.  Commit `server: restore ace face_val before broadcasting sorted hand` (server.c:`handle_sort_hand`) restores the value before the broadcast, but the underlying pokeval design still couples the sort to evaluator-internal numbering.  Worth considering a cleaner pokeval API split: a pure `POKEVAL_sort_hand_for_eval` that callers know is destructive, vs. a display-safe sort.

## Open for discussion — headless server binary name

The headless server binary added during the renovation is currently named
`dealers-choice-server` (executable in `meson.build`, entry point
`src/server/server_main.c`, deprecation stub for the GUI's `--server` in
`src/cli.c`).  Andy isn't sold on the name — revisit before release.

Name proposals so far:
- `dealers-choice-server` (current placeholder)
- `dealers-choice-listen-for-clients-who-wish-to-player-poker-and-accept-the-connection-if-there-are-no-errors-while-connecting`
  (Andy's, submitted in jest — kept here so the bar for "too verbose" stays well documented)

Whatever it lands on, these all reference it and must change together: `meson.build`,
the GUI `--server` deprecation message (`src/cli.c`), `tests/game_logic.py`,
`scripts/_harness_common.sh`, `docker/docker-compose.yml`, and the docs
(`README.md`, `docs/CONFIG.md`, `tests/README.md`).
